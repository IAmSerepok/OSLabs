#pragma once

#ifdef WIN32
#	include <windows.h>  // HANDLE и CRITICAL_SECTION
typedef CRITICAL_SECTION rc_mutex; 
typedef HANDLE rc_thread;
typedef DWORD rc_thread_id;
#else
#	include <pthread.h>       // pthread_* functions
#	include <signal.h>        // pthread_kill()
#   include <unistd.h>        // pause()
#   include <errno.h>         // system error types
typedef pthread_t rc_thread;
typedef pthread_mutex_t rc_mutex;
typedef pthread_t rc_thread_id;
#endif

#include <string.h> // memset()
#include <deque>    // std::deque

namespace cplib
{
	// Коды возврата
	enum ThreadReturns
	{
		THREAD_SUCCESS = 0,      // Успех
		THREAD_FAILURE = -1,     // Общая, неспецифицированная ошибка
		THREAD_TIMEOUT = -2,     // Таймаут
		THREAD_WRONG_SEQ = -3,   // Неверный порядок вызова
		THREAD_INVALIDPAR = -4,  // Неверный параметр
		THREAD_STOP_SIG = -5     // Сигнал на остановку потока
	};
	// Простой, рекурсивный мьютекс
	class Mutex
	{
		friend class AutoMutex;
	public:
		Mutex() {
#ifdef WIN32
			InitializeCriticalSection(&_mutex);
#else
			pthread_mutexattr_t attr;
			pthread_mutexattr_init(&attr);
			pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
			pthread_mutex_init(&_mutex, &attr);
			pthread_mutexattr_destroy(&attr);
#endif
		}
		~Mutex() {
#ifdef WIN32
			DeleteCriticalSection(&_mutex);
#else
			pthread_mutex_destroy(&_mutex);
#endif
		}
		void Lock() {
#ifdef WIN32
			EnterCriticalSection(&_mutex);
#else
			pthread_mutex_lock(&_mutex);
#endif
		}
		void UnLock() {
#ifdef WIN32
			LeaveCriticalSection(&_mutex);
#else
			pthread_mutex_unlock(&_mutex);
#endif
		}
		bool TryLock() {
#ifdef WIN32
			return (TryEnterCriticalSection(&_mutex) != 0);
#else
			return (pthread_mutex_trylock(&_mutex) == 0);
#endif
		}
	private:
		rc_mutex _mutex;
		// Защита от копирования
	private:
		Mutex(Mutex const&) {}
		Mutex& operator=(Mutex const&) { return *this; }
	};

	// Специальный класс мьютекса, который блокируется при создании
	// и разблокируется при уничтожении
	// использует либо внешний объект мьютекса, либо создает локальный
	// для защиты локального участка кода
	class AutoMutex
	{
	public:
		// Создает локальный мьютекс
		AutoMutex() :_local(true) {
			_mutex = new rc_mutex();
#ifdef WIN32
			InitializeCriticalSection(_mutex);
			EnterCriticalSection(_mutex);
#else
			pthread_mutex_init(_mutex, NULL);
			pthread_mutex_lock(_mutex);
#endif
		}
		// Использует существующий объект мьютекса
		AutoMutex(Mutex& mut) :_local(false) {
			_mutex = &mut._mutex;
#ifdef WIN32
			EnterCriticalSection(_mutex);
#else
			pthread_mutex_lock(_mutex);
#endif
		}
		~AutoMutex() {
#ifdef WIN32
			LeaveCriticalSection(_mutex);
			if (_local)
				DeleteCriticalSection(_mutex);
#else
			pthread_mutex_unlock(_mutex);
			if (_local)
				pthread_mutex_destroy(_mutex);
#endif
			if (_local)
				delete(_mutex);
		}
	private:
		rc_mutex* _mutex;
		bool _local;
		AutoMutex(Mutex const&) {}
		AutoMutex& operator=(Mutex const&) { return *this; }
	};

	// Барьер
	class Barrier
	{
	public:
		// Параметр конструктора - число ожидающих потоков
		Barrier(int value = 2):_value(value) {
			_entered_barrier = 0;
			_exited_barrier = 0;
#ifdef WIN32
			_entrance_semaphore = CreateSemaphore(NULL, 0, 4096, NULL);
			_exit_semaphore = CreateSemaphore(NULL, 0, 4096, NULL);
#else
			pthread_mutex_init(&_mutex, NULL);
			pthread_cond_init(&_entered_cond, NULL);
			pthread_cond_init(&_exited_cond, NULL);
#endif
		}
		~Barrier() {
#ifdef WIN32
			CloseHandle(_entrance_semaphore);
			CloseHandle(_exit_semaphore);
#else
			pthread_mutex_destroy(&_mutex);
			pthread_cond_destroy(&_entered_cond);
			pthread_cond_destroy(&_exited_cond);
#endif
		}
		// Подождать, пока все потоки войдут за барьер
		void WaitEnter() {
#ifdef WIN32
			if (InterlockedIncrement(&_entered_barrier) < _value)
				WaitForSingleObject(_entrance_semaphore, INFINITE);
			else {
				_exited_barrier = 0;
				ReleaseSemaphore(_entrance_semaphore, _value - 1, NULL);
			}
#else
			pthread_mutex_lock(&_mutex);
			_entered_barrier++;
			if (_entered_barrier < _value)
				pthread_cond_wait(&_entered_cond, &_mutex);
			else {
				_exited_barrier = 0;
				pthread_cond_broadcast(&_entered_cond);
			}
			pthread_mutex_unlock(&_mutex);
#endif
		}
		// Подождать, пока все потоки выйдут
		void WaitExit() {
#ifdef WIN32
			if (InterlockedIncrement(&_exited_barrier) < _value)
				WaitForSingleObject(_exit_semaphore, INFINITE);
			else {
				_entered_barrier = 0;
				ReleaseSemaphore(_exit_semaphore, _value - 1, NULL);
			}
#else
			pthread_mutex_lock(&_mutex);
			_exited_barrier++;
			if (_exited_barrier < _value)
				pthread_cond_wait(&_exited_cond, &_mutex);
			else {
				_entered_barrier = 0;
				pthread_cond_broadcast(&_exited_cond);
			}
			pthread_mutex_unlock(&_mutex);
#endif
		}
		// Ожидание в потоке
		void Wait() {
#ifdef WIN32
			if (InterlockedIncrement(&_exited_barrier) < _value)
				WaitForSingleObject(_exit_semaphore, INFINITE);
			else {
				_exited_barrier = 0;
				ReleaseSemaphore(_exit_semaphore, _value - 1, NULL);
			}
#else
			pthread_mutex_lock(&_mutex);
			_exited_barrier++;
			if (_exited_barrier < _value)
				pthread_cond_wait(&_exited_cond, &_mutex);
			else {
				_exited_barrier = 0;
				pthread_cond_broadcast(&_exited_cond);
			}
			pthread_mutex_unlock(&_mutex);
#endif
		}
	private:
		int _value;
#ifdef WIN32
		volatile LONG _entered_barrier;
		volatile LONG _exited_barrier;
#else
		volatile int _entered_barrier;
		volatile int _exited_barrier;
#endif
#ifdef WIN32
		HANDLE _entrance_semaphore;
		HANDLE _exit_semaphore;
#else
		pthread_mutex_t _mutex;
		pthread_cond_t _entered_cond;
		pthread_cond_t _exited_cond;
#endif
		// Защита от копирования
	private:
		Barrier(Barrier const&) {}
		Barrier& operator=(Barrier const&) { return *this; }
	};

	// Условная переменная, которую могут подождать несколько потоков
	class CondVar
	{
	public:
		CondVar() {
#ifdef WIN32
			InitializeCriticalSection(&_crtmutex);
			InitializeCriticalSection(&_wlmutex);
#else
			pthread_mutex_init(&_crtmutex, NULL);
			pthread_cond_init(&_cond, NULL);
#endif
		}
		~CondVar() {
#ifdef WIN32
			DeleteCriticalSection(&_crtmutex);
			DeleteCriticalSection(&_wlmutex);
#else
			pthread_mutex_destroy(&_crtmutex);
			pthread_cond_destroy(&_cond);
#endif
		}
		// Ожидать сигнала time секунд. Вечно, есть time < 0
		int Wait(const double& time = -1.0)
		{
#ifdef WIN32
			// Заблокируем мьютьекс доступа
			EnterCriticalSection(&_crtmutex);
			// Создадим новое событие
			HANDLE wait_event = CreateEvent(NULL, FALSE, FALSE, NULL);
			if (wait_event == NULL)
				return THREAD_FAILURE;
			// Поместим событие в deque
			EnterCriticalSection(&_wlmutex);
			_waiting_list.push_back(wait_event);
			LeaveCriticalSection(&_wlmutex);
			// Освободим мьютекс доступа
			LeaveCriticalSection(&_crtmutex);
			// ожидаем события
			DWORD millisecs = INFINITE;
			if (time >= 0.0)
				millisecs = (DWORD)(time * 1e3);
			DWORD wait_result = WaitForSingleObjectEx(wait_event, millisecs, FALSE);
			// Заблокируем мьютьекс доступа
			EnterCriticalSection(&_crtmutex);
			// Если мы вышли по таймауту - значит никто не достал событие из списка
			// значит сделаем это сами
			if (wait_result == WAIT_TIMEOUT) {
				EnterCriticalSection(&_wlmutex);
				_waiting_list.pop_front();
				LeaveCriticalSection(&_wlmutex);
			}
			// Закроем хэндл события
			if (!CloseHandle(wait_event)) {
				LeaveCriticalSection(&_crtmutex);
				return THREAD_FAILURE;
			}
			// Выходим из функции
			LeaveCriticalSection(&_crtmutex);
			if (wait_result == WAIT_TIMEOUT)
				return THREAD_TIMEOUT;
			if (wait_result == WAIT_OBJECT_0 || wait_result == WAIT_IO_COMPLETION)
				return THREAD_SUCCESS;
			return THREAD_FAILURE;
#else
			int ret = -1;
			// небольшая защита - если поток будет прерван во время pthread_cond_wait(),
			// он сразу же залочит мьютекс - мы должны разлочить его перед выходом
			pthread_cleanup_push((void(*)(void*))pthread_mutex_unlock, (void*)&this->_crtmutex);
			// блокируем мьютекс доступа
			pthread_mutex_lock(&_crtmutex);
			if (time >= 0.0) {
				struct timespec tp;
				clock_gettime(CLOCK_REALTIME, &tp);
				tp.tv_sec  += (int)time;
				tp.tv_nsec += (int)((time - (int)time)*1e9);
				ret = pthread_cond_timedwait(&_cond, &_crtmutex, &tp);
			}
			else
				ret = pthread_cond_wait(&_cond, &_crtmutex);
			pthread_mutex_unlock(&_crtmutex);
			pthread_cleanup_pop(0);
			if (!ret)
				return THREAD_SUCCESS;
			if (ret == ETIMEDOUT)
				return THREAD_TIMEOUT;
			return THREAD_FAILURE;
#endif
		}
		// Предупредить один ожидающий поток
		int Notify() {
#ifdef WIN32
			// Достать первый поток из очереди (который ждет дольше всех)
			EnterCriticalSection(&_wlmutex);
			HANDLE wait_event = NULL;
			if (!_waiting_list.empty()) {
				wait_event = _waiting_list.front();
				_waiting_list.pop_front();
			}
			LeaveCriticalSection(&_wlmutex);
			// Если никто не ждет - выходим
			if (wait_event == NULL)
				return THREAD_SUCCESS;
			// Сигнализируем событие
			if (SetEvent(wait_event))
				return THREAD_SUCCESS;
			return THREAD_FAILURE;
#else
			pthread_mutex_lock(&_crtmutex);
			int ret = pthread_cond_signal(&_cond);
			pthread_mutex_unlock(&_crtmutex);
			return ret;
#endif
		}
		// Предупредить все ожидающие потоки
		int NotifyAll() {
#ifdef WIN32
			int ret = THREAD_SUCCESS;
			EnterCriticalSection(&_wlmutex);
#if __cplusplus >= 201103L
			::std::deque<HANDLE>::const_iterator it = _waiting_list.begin();
			::std::deque<HANDLE>::const_iterator end = _waiting_list.end();
#else
			::std::deque<HANDLE>::iterator it = _waiting_list.begin();
			::std::deque<HANDLE>::iterator end = _waiting_list.end();
#endif
			for (; it < end; it++) {
				if (!SetEvent(*it)) {
					ret = THREAD_FAILURE;
					break;
				}
			}
			if (ret == THREAD_SUCCESS)
				_waiting_list.clear();
			else
				_waiting_list.erase(_waiting_list.begin(), it);
			LeaveCriticalSection(&_wlmutex);
			return ret;
#else
			pthread_mutex_lock(&_crtmutex);
			int ret = pthread_cond_broadcast(&_cond);
			pthread_mutex_unlock(&_crtmutex);
			return ret;
#endif
		}
	private:
		// Мьютекс для синхронизации
		rc_mutex _crtmutex;
#ifdef WIN32
		// Список ожидающих потоков
		::std::deque<HANDLE> _waiting_list;
		// мьютекс для защиты листа ожидания
		rc_mutex _wlmutex;
#else
		// условная переменнпя
		pthread_cond_t _cond;
#endif
		// защита от копирования
	private:
		CondVar(CondVar const&) {}
		CondVar& operator=(CondVar const&) { return *this; }
	};

	// Класс события
	// 
	class Event
	{
	public:
		Event(int type) :_evt_type(type) {}
		bool IsUserEvent() {
			return _evt_type > 0;
		}
		int Type() {
			return _evt_type;
		}
	private:
		int _evt_type;
	};

	// Класс потока
	// Наследники перегружают функции Main(), MainStart() и MainStop()
	class Thread
	{
	public:
		// Машина состояний потока
		enum State
		{
			STATE_STOPPED,
			STATE_RUNNING,
			STATE_STOPPING,
			STATE_RESTARTING
		};
		// Результат запуска потока
		enum StartingFlag
		{
			FLAG_NOT_STARTED = -1,
			FLAG_STARTED_SUCCESFULLY = 0,
			FLAG_FAIL_TO_START = 1
		};
		// exception бросаемый при завершении
		struct TermEx
		{
			TermEx(int code) :exit_code(code) {}
			int exit_code;
		};

		Thread() :_state(STATE_STOPPED), _barrier(2), _start_flag(FLAG_NOT_STARTED) {
#ifdef WIN32
			_cleanup_event = NULL;
			_thread = NULL;
#else
			memset(&_thread, 0, sizeof(_thread));
#endif
		}

		virtual ~Thread() {
			// Остановим поток и подождем его завершения 1 секунду
			Stop();
			int ret = Join(1.0);
			// Убъем поток, если он не хочет тормозить )
			if (ret != THREAD_SUCCESS && ret != THREAD_WRONG_SEQ)
				Kill();
		}

	public:
		static void Sleep(double timeout) {
#if defined (WIN32)
			if (timeout <= 0.0)
				::Sleep(INFINITE);
			else
				::Sleep((DWORD)(timeout * 1e3));
#else
			if (timeout <= 0.0)
				pause();
			else {
				struct timespec t;
				t.tv_sec = (int)timeout;
				t.tv_nsec = (int)((timeout - t.tv_sec)*1e9);
				nanosleep(&t, NULL);
			}
#endif
		}
		// Состояние потока 
		Thread::State ThreadState() {
			Thread::State state;
			_mutex.Lock();
			state = _state;
			_mutex.UnLock();
			return state;
		}
		// Результат запуска потока
		int StartFlag() {
			_sync_mutex.Lock();
			int ret = _start_flag;
			_sync_mutex.UnLock();
			return ret;
		}
		// Запустить поток и продолжить исполнение родителя
		int Start() {
			_mutex.Lock();
			// запуск возможен только из состояния STOP
			if (_state == STATE_STOPPED) {
				// Почистим за собой предыдущим
				CleanupThread();
#ifdef WIN32
				_cleanup_event = CreateEvent(NULL, FALSE, FALSE, NULL);
				if (!_cleanup_event) {
					_mutex.UnLock();
					return THREAD_FAILURE;
				}
				_thread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)(Thread::RealMain), this, 0, NULL);
				if (!_thread) {
					_mutex.UnLock();
					return THREAD_FAILURE;
				}
#else
				int ret = pthread_create(&_thread, NULL, &Thread::RealMain, this);
				if (ret) {
					_mutex.UnLock();
					return THREAD_FAILURE;
				}
#endif
				// Подождем старта потока
				_barrier.Wait();
				_state = STATE_RUNNING;
				_mutex.UnLock();
				return THREAD_SUCCESS;
			}
			else if (_state == STATE_STOPPING) {
				_state = STATE_RESTARTING;
				_mutex.UnLock();
				return THREAD_SUCCESS;
			}
			_mutex.UnLock();
			return THREAD_WRONG_SEQ;
		}
		// Подождать запуска потока в родителе
		int WaitStartup() {
			int ret = FLAG_NOT_STARTED;
			// Проверим, может мы и не стартовали
			State st = ThreadState();
			if (st == STATE_STOPPED || st == STATE_STOPPING)
				return ret;
			// Подождем старта
			for (; (ret = StartFlag()) == Thread::FLAG_NOT_STARTED; Sleep(0.05));
			return ret;
		}
		// Послать потоку команду на останов
		// В функции Main() потока должна быть хотя бы одна из функций
		// CancelPoint() или Wait(interruptable == true)
		int Stop() {
			_mutex.Lock();
			if (_state == STATE_RUNNING) {
#ifdef WIN32
				SetEvent(_cleanup_event);
#else
				pthread_cancel(_thread);
#endif
				_state = STATE_STOPPING;
				_mutex.UnLock();
				_sync_mutex.Lock();
				_events.clear();
				_events.push_back(Event(THREAD_STOP_SIG));
				_sync_mutex.UnLock();
				_condvar.NotifyAll();
				return THREAD_SUCCESS;
			}
			else if (_state == STATE_RESTARTING) {
				_state = STATE_STOPPING;
				_mutex.UnLock();
				return THREAD_SUCCESS;
			}
			_mutex.UnLock();
			return THREAD_WRONG_SEQ;
		}
		// Подождать завершение исполнения потока time секунд
		// Вечно, если time < 0
		int Join(const double& time = -1.0) {
			_mutex.Lock();
			if (_state == STATE_STOPPED) {
				_mutex.UnLock();
				return THREAD_SUCCESS;
			}
			_mutex.UnLock();
#ifdef WIN32
			DWORD wtm = INFINITE;
			if (time > 0.0)
				wtm = (DWORD)(time * 1e3);
			int ret = WaitForSingleObject(_thread, wtm);
			if (ret == WAIT_TIMEOUT)
				return THREAD_TIMEOUT;
			if (ret == WAIT_OBJECT_0) {
				_mutex.Lock();
				CleanupThread();
				_mutex.UnLock();
				return THREAD_SUCCESS;
			}
#else
			struct timespec abstime;
			abstime.tv_sec  = (int)time;
			abstime.tv_nsec = (int)((time - abstime.tv_sec)*1e9);
			int ret = pthread_timedjoin_np(_thread, NULL, &abstime);
			if (!ret) {
				_mutex.Lock();
				CleanupThread();
				_mutex.UnLock();
				return THREAD_SUCCESS;
			}
#endif
			return THREAD_FAILURE;
		}
		// Убить поток принудительно
		int Kill() {
#ifdef WIN32
			int ret = TerminateThread(_thread, 0);
#else
			int ret = !(pthread_kill(_thread, SIGTERM));
#endif
			if (ret) {
				_mutex.Lock();
				_state = STATE_STOPPED;
				CleanupThread();
				_mutex.UnLock();
				return THREAD_SUCCESS;
			}
			return THREAD_FAILURE;
		}
		// Предупредить ожидающий поток
		void Notify(const Event& evt) {
			_sync_mutex.Lock();
			_events.push_back(evt);
			_sync_mutex.UnLock();
			_condvar.Notify();
		}

	protected:
		// Главная функция в потоке
		// Если содержит цикл, должна содержать CancelPoint() или Wait(interruptable == true)
		// чтобы поток можно было тормознуть извне
		virtual void Main() = 0;
		// Функция, запускаемая до запуска основного потока
		// Можно выделять динамическую память и т.п.
		// Все хорошо - возвращаем 0, ошибка - код ошибки
		virtual int MainStart() { return 0; }
		// Функция, запуская перед остановкой потока
		// Поток прерывается в CancelPoint() или Wait(interruptable == true)
		// Поэтому именно здесь нужно чистить память и т.п.
		virtual void MainQuit() {}
		// Точка возможного прерывания потока
		// В Windows-реализации должна кидать exception
#ifdef WIN32
#	if defined (_MSC_VER)
		void CancelPoint() throw(...) {
#   else
		void CancelPoint() {
#	endif
			int ret = WaitForSingleObject(_cleanup_event, 0);
			if (ret == WAIT_OBJECT_0) {
				throw TermEx(0);
			}
		}
#else
		void CancelPoint() {
			int oldstate;
			pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);
			pthread_testcancel();
			pthread_setcancelstate(oldstate, NULL);
		}
#endif
	protected:
		// Подождать предупреждения (Notify())
		// Если interruptable == true, значит во время ожидания поток может быть прерван
		// Возвращает произошедшее событие, положительный код - событие пользователя, отрицательный код - системное
		Event Wait(const double& time = -1.0, bool interruptable = true) {
			while (_events.empty()) {
				int rt = _condvar.Wait(time);
				if (rt == THREAD_TIMEOUT)
					return Event(THREAD_TIMEOUT);
				else if (rt != THREAD_SUCCESS)
					return Event(THREAD_FAILURE);
			}
			_sync_mutex.Lock();
			Event evt = _events.front();
			_events.pop_front();
			_sync_mutex.UnLock();
			if (evt.Type() == THREAD_STOP_SIG && interruptable)
				CancelPoint();
			return evt;
		}
	private:
		// Основная функция потока
		static void* RealMain(void* thread_ptr) {
			Thread* thr = reinterpret_cast<Thread*>(thread_ptr);
			thr->_sync_mutex.Lock();
			thr->_start_flag = FLAG_NOT_STARTED;
			thr->_sync_mutex.UnLock();
#ifndef WIN32
			// Отключить прерывание потока
			pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
#endif
			// синхронизация с потоком-родителем
			thr->_barrier.Wait();
#ifndef WIN32
			// установить функцию, срабатывающую по прерыванию
			pthread_cleanup_push(&RealMainQuit, thread_ptr);
#endif
			// запустим код пользователя
			int ret = thr->MainStart();
			// Запускаем Main()
			if (ret == 0) {
				thr->_sync_mutex.Lock();
				thr->_start_flag = FLAG_STARTED_SUCCESFULLY;
				thr->_sync_mutex.UnLock();
#ifdef WIN32
				try { thr->Main(); }
				catch (TermEx)
				{
				}
#else
				thr->Main();
#endif
			}
			else {
				thr->_sync_mutex.Lock();
				thr->_start_flag = ret;
				thr->_sync_mutex.UnLock();
			}
#ifndef WIN32
			pthread_cleanup_pop(1);
#else
			RealMainQuit(thr);
#endif
			return NULL;
		}
		// Основная функция завершения потока
		static void RealMainQuit(void* thread_ptr) {
			Thread* thr = reinterpret_cast<Thread*>(thread_ptr);
			thr->_mutex.Lock();
			if (thr->_state == STATE_RESTARTING) {
				thr->_state = STATE_STOPPED;
				thr->Start();
			}
			else {
				thr->_state = STATE_STOPPED;
			}
			thr->_mutex.UnLock();
			thr->MainQuit();
			thr->_start_flag = FLAG_NOT_STARTED;
		}
		// Очистить поток
		int CleanupThread() {
			_events.clear();
			_start_flag = FLAG_NOT_STARTED;
			// мьютекс должен быть залочен тут!
#ifdef WIN32
			if (_thread) {
				CloseHandle(_thread);
				CloseHandle(_cleanup_event);
				_thread = NULL;
				_cleanup_event = NULL;
				return THREAD_SUCCESS;
			}
			return THREAD_WRONG_SEQ;
#else
			rc_thread clr;
			memset(&clr, 0, sizeof(clr));
			if (!memcmp(&_thread, &clr, sizeof(rc_thread))) {
				pthread_detach(_thread);
				memset(&_thread, 0, sizeof(_thread));
				return THREAD_SUCCESS;
			}
			return THREAD_WRONG_SEQ;
#endif
		}
		// Состояние потока
		State _state;
		// Системный объект потока
		rc_thread _thread;
		// Условная переменная для ожидания
		CondVar _condvar;
		// Барьер для синхронизации потоков на старте
		Barrier _barrier;
		// Мьютексы для защиты данных потока
		Mutex _sync_mutex;
		Mutex _mutex;
		// Notify-сигнал
		::std::deque<Event> _events;
		// Результат старта
		int _start_flag;
#ifdef WIN32
		//Событие очистки
		HANDLE _cleanup_event;
#endif
		// Защита от копирования
	private:
		Thread(Thread const&) {}
		Thread& operator=(Thread const&) { return *this; }
	};
}