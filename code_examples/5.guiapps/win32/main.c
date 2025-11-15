// Очень простое приложение на WINAPI, выводящее окошко
// с кнопкой, по нажатию на которую отображается сообщение

#include <windows.h>    // WinAPI

// Идентификатор события нажатия кнопки
#define BTN1_SIGNAL 1
// Описание функции обработки событий
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
// Имя класса приложения
const char APP_CLASS_NAME[]   = "HelloWorld App";
// Заголовок основного окна приложения
const char APP_WINDOW_TITLE[] = "Hello App, ver 1.0";

// Точка входа в оконное приложение
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE hprevinst, LPSTR cmdline, int cmd_show)
{
	MSG   msg;     // Сообщение
	HWND  hwnd;    // HANDLE главного окна приложения
	HWND  btn1;    // HANDLE кнопки
	
	// Инициализация приложения
	WNDCLASS wc;
	memset(&wc,0,sizeof(wc));
	wc.style = 0;
	wc.lpfnWndProc = (WNDPROC) WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hinst;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wc.lpszMenuName = (LPSTR)NULL;
	wc.lpszClassName = (LPSTR)APP_CLASS_NAME;
	if (!RegisterClass(&wc))
		return FALSE;
	
	// Создание главного окна программы
	hwnd = CreateWindow(APP_CLASS_NAME,APP_WINDOW_TITLE,WS_OVERLAPPEDWINDOW,
						CW_USEDEFAULT,CW_USEDEFAULT,640,480,
						NULL,NULL,hinst,NULL);
	if (!hwnd)
		return FALSE;
	// Отрисовка окна
	ShowWindow(hwnd,cmd_show);
	UpdateWindow(hwnd);
	
	// Создание кнопки
	btn1 = CreateWindow("button","Button 1",WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
	                    20,60,90,30,hwnd,(HMENU)BTN1_SIGNAL,hinst,NULL);
	
	// Запуск цикла обработки сообщений
	while(GetMessage(&msg,0,0,0))
		DispatchMessage(&msg);
	// Сообщение на выход из приложения
	return msg.wParam;
}

// Функция обработки событий
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wpar, LPARAM lpar)
{
	switch (msg) {
		// Команда от элемента интерфейса
		case WM_COMMAND:
		{
			if (wpar == BTN1_SIGNAL) {
				MessageBox(hwnd, "Button 1 pushed!", "Message from APP",MB_OK);
			}
			return 0;
		}
		// Команда на закрытие окна
		case WM_DESTROY:
		{
			PostQuitMessage(0);
			return 0;
		}
	}
	return DefWindowProc(hwnd,msg,wpar,lpar);
}