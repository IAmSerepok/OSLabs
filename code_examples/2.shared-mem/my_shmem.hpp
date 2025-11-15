
#pragma once

#include <string.h>   // strlen()
#include <stdlib.h>   // malloc()
#if defined (WIN32)
#   include <windows.h>
#	define MAP_NAME_PREFIX "Local\\"
#	define INV_HANDLE (NULL)
#	define CSEM        HANDLE
#else
#   include <sys/mman.h>
#   include <sys/stat.h>        /* Константы режимов */
#   include <fcntl.h>           /* Константы O_* */
#   include <unistd.h>          /* ftruncate() */
#   include <semaphore.h>       /* семафоры */
#   define HANDLE          int
#   define INV_HANDLE      (-1)
#	define MAP_NAME_PREFIX  "/"
#	define CSEM            sem_t*
#endif

#define SEM_NAME_POSTFIX "_sem"

namespace cplib
{
    template <class T> class SharedMem
    {
    public:
        SharedMem(const char* name, bool create_if_not_exists = true):_fd(INV_HANDLE),_mem(NULL), _sem(NULL){
			// Получим системное имя для объекта памяти
			_fname = (char*)malloc(strlen(name) + strlen(MAP_NAME_PREFIX) + 1);
			memcpy(_fname, MAP_NAME_PREFIX, strlen(MAP_NAME_PREFIX));
			memcpy(_fname + strlen(MAP_NAME_PREFIX), name, strlen(name)+1);
			_semname = (char*)malloc(strlen(_fname) + strlen(SEM_NAME_POSTFIX) + 1);
			memcpy(_semname, _fname, strlen(_fname));
			memcpy(_semname + strlen(_fname), SEM_NAME_POSTFIX, strlen(SEM_NAME_POSTFIX) + 1);

			// Попытаемся открыть или создать область памяти
			bool is_new = false;
			bool ret = OpenMem(_fname, _semname);
			if (!ret && create_if_not_exists) {
				if (ret = CreateMem(_fname, _semname))
					is_new = true;
			}
			// Попытаемся подключить область памяти
			if (ret)
				ret = MapMem();
			// Если подключили новую память - ее необходимо инициализировать
			if (ret && is_new) {
				_mem->cnt = 0;
				_mem->str = T();
			}
			if (ret) {
				// Зарегистрируемся
				LockSema();
				_mem->cnt++;
				UnlockSema();
			} else {
				// На каком-то этапе провалились - удалим (или освободим) память
				if (is_new)
					DestroyMem();
				else
					CloseMem();
			}
        }
		virtual ~SharedMem() {
			if (IsValid()) {
				int cnt = 0;
				LockSema();
				_mem->cnt--;
				cnt = _mem->cnt;
				UnlockSema();
				if (cnt <= 0)
					DestroyMem();
				else
					CloseMem();
			}
            // Освободим память, занятую строками с именами
            free(_fname);
			free(_semname);
		}
        bool IsValid() {return _fd != INV_HANDLE && _sem != NULL && _mem != NULL;}
		void Lock()    {LockSema();}
		T* Data() {
			if (!IsValid())
				return NULL;
			return &_mem->str;
		}
		void Unlock() {UnlockSema();}
	private:
        bool OpenMem(const char* mem_name, const char* sem_name) {
#if defined (WIN32)
			_fd = OpenFileMapping(FILE_MAP_WRITE, true, mem_name);
			if (_fd != INV_HANDLE)
				_sem = OpenSemaphore(SEMAPHORE_ALL_ACCESS, false, sem_name);
#else
            _fd = shm_open(mem_name, O_RDWR, 0644);
			if (_fd != INV_HANDLE) {
				_sem = sem_open(sem_name, 0);
                if (_sem == SEM_FAILED)
                    _sem = NULL;
            }
#endif
            return (_fd != INV_HANDLE && _sem != NULL);
        }
		bool CreateMem(const char* mem_name, const char* sem_name) {
#if defined (WIN32)
			_fd = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(shmem_contents), mem_name);
			if (_fd != INV_HANDLE)
				_sem = CreateSemaphore(NULL, 0, 1, sem_name);
#else
			_fd = shm_open(mem_name, O_CREAT | O_EXCL | O_RDWR, 0644);
			if (_fd != INV_HANDLE) {
				ftruncate(_fd, sizeof(shmem_contents));
				_sem = sem_open(sem_name, O_CREAT | O_EXCL, 0644, 1);
                if (_sem == SEM_FAILED)
                    _sem = NULL;
			}
#endif
            return (_fd != INV_HANDLE && _sem != NULL);
        }
		bool MapMem() {
			if (_fd == INV_HANDLE)
				return NULL;
#if defined (WIN32)
			_mem = reinterpret_cast<shmem_contents*>(MapViewOfFile(_fd, FILE_MAP_WRITE, 0, 0, sizeof(struct shmem_contents)));
#else
			void* res = mmap(NULL, sizeof(struct shmem_contents), PROT_WRITE | PROT_READ, MAP_SHARED, _fd, 0);
			if (res == MAP_FAILED)
				_mem = NULL;
            else
                _mem = reinterpret_cast<shmem_contents*>(res);
#endif
			return (_mem != NULL);
		}
		bool UnMapMem() {
			if (_mem == NULL)
				return false;
#if defined (WIN32)
			UnmapViewOfFile(_mem);
#else
			munmap(_mem, sizeof(struct shmem_contents));
#endif
			_mem = NULL;
			return true;
		}
		void CloseMem() {
			// Отключим память
			UnMapMem();
			if (_fd != INV_HANDLE) {
#if defined (WIN32)
				CloseHandle(_fd);
#else
				close(_fd);
#endif		
				_fd = INV_HANDLE;
			}
			if (_sem != NULL) {
#if defined (WIN32)
				CloseHandle(_sem);
#else
				sem_close(_sem);
#endif
				_sem = NULL;
			}
		}
		void DestroyMem()
		{
			CloseMem();
			// В Windows и семафоры и память удалятся автоматически, когда никто не будет их использовать
#if !defined (WIN32)
			shm_unlink(_fname);
			sem_unlink(_semname);
#endif
		}
		void LockSema()
		{
#if defined (WIN32)
			ReleaseSemaphore(_sem, 1, NULL);
#else
			sem_post(_sem);
#endif
		}
		void UnlockSema()
		{
#if defined (WIN32)
			WaitForSingleObject(_sem, 0);
#else
			sem_wait(_sem);
#endif
		}
        struct shmem_contents
        {
            T      str;
            int    cnt;
        } *_mem;
        CSEM   _sem;
        HANDLE _fd;
        char* _fname;
        char* _semname;
	};
}