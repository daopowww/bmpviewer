// bmpviewer.cpp : Определяет точку входа для приложения.
//

#include "framework.h"
#include "bmpviewer.h"
#include <commdlg.h>

#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Gdi32.lib")

#define MAX_LOADSTRING 100

constexpr UINT ID_OPEN_BMP = 50001;
constexpr int IMAGE_MARGIN = 20;

HBITMAP loadedBitmap = nullptr;
BITMAP bitmapInfo = {};

// Глобальные переменные:
HINSTANCE hInst;                                // текущий экземпляр
WCHAR szTitle[MAX_LOADSTRING];                  // Текст строки заголовка
WCHAR szWindowClass[MAX_LOADSTRING];            // имя класса главного окна

// Отправить объявления функций, включенных в этот модуль кода:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void OpenBitmapFile(HWND hWnd);
void DrawBitmap(HWND hWnd, HDC hdc);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Разместите код здесь.

    // Инициализация глобальных строк
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_BMPVIEWER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Выполнить инициализацию приложения:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_BMPVIEWER));

    MSG msg;

    // Цикл основного сообщения:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  ФУНКЦИЯ: MyRegisterClass()
//
//  ЦЕЛЬ: Регистрирует класс окна.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_BMPVIEWER));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_BMPVIEWER);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   ФУНКЦИЯ: InitInstance(HINSTANCE, int)
//
//   ЦЕЛЬ: Сохраняет маркер экземпляра и создает главное окно
//
//   КОММЕНТАРИИ:
//
//        В этой функции маркер экземпляра сохраняется в глобальной переменной, а также
//        создается и выводится главное окно программы.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Сохранить маркер экземпляра в глобальной переменной

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

// Выбор и загрузка BMP. Старое изображение заменяется после успешной загрузки.
void OpenBitmapFile(HWND hWnd)
{
    wchar_t fileName[32768] = {};

    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = hWnd;
    dialog.lpstrFilter = L"Изображения BMP (*.bmp)\0*.bmp\0Все файлы (*.*)\0*.*\0";
    dialog.lpstrFile = fileName;
    dialog.nMaxFile = ARRAYSIZE(fileName);
    dialog.lpstrTitle = L"Выберите изображение BMP";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&dialog))
    {
        if (CommDlgExtendedError() != 0)
        {
            MessageBoxW(hWnd, L"Не удалось открыть диалог выбора файла.",
                L"Ошибка", MB_OK | MB_ICONERROR);
        }
        return;
    }

    HBITMAP newBitmap = static_cast<HBITMAP>(LoadImageW(
        nullptr, fileName, IMAGE_BITMAP, 0, 0,
        LR_LOADFROMFILE | LR_CREATEDIBSECTION));

    if (!newBitmap)
    {
        MessageBoxW(hWnd, L"Не удалось загрузить BMP. Проверьте формат файла.",
            L"Ошибка загрузки", MB_OK | MB_ICONERROR);
        return;
    }

    BITMAP newBitmapInfo = {};
    if (GetObjectW(newBitmap, sizeof(newBitmapInfo), &newBitmapInfo) == 0
        || newBitmapInfo.bmWidth <= 0 || newBitmapInfo.bmHeight <= 0)
    {
        DeleteObject(newBitmap);
        MessageBoxW(hWnd, L"Не удалось определить размеры изображения.",
            L"Ошибка загрузки", MB_OK | MB_ICONERROR);
        return;
    }

    if (loadedBitmap)
    {
        DeleteObject(loadedBitmap);
    }
    loadedBitmap = newBitmap;
    bitmapInfo = newBitmapInfo;

    InvalidateRect(hWnd, nullptr, TRUE);
}

// Положение пересчитывается при каждой перерисовке окна.
void DrawBitmap(HWND hWnd, HDC hdc)
{
    if (!loadedBitmap)
    {
        return;
    }

    HDC memoryDC = CreateCompatibleDC(hdc);
    if (!memoryDC)
    {
        return;
    }

    HGDIOBJ previousBitmap = SelectObject(memoryDC, loadedBitmap);
    if (!previousBitmap || previousBitmap == HGDI_ERROR)
    {
        DeleteDC(memoryDC);
        return;
    }

    RECT clientRect = {};
    GetClientRect(hWnd, &clientRect);
    int x = clientRect.right - bitmapInfo.bmWidth - IMAGE_MARGIN;
    if (x < 0)
    {
        x = 0;
    }

    BitBlt(hdc, x, IMAGE_MARGIN, bitmapInfo.bmWidth, bitmapInfo.bmHeight,
        memoryDC, 0, 0, SRCCOPY);

    SelectObject(memoryDC, previousBitmap);
    DeleteDC(memoryDC);
}

//
//  ФУНКЦИЯ: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  ЦЕЛЬ: Обрабатывает сообщения в главном окне.
//
//  WM_COMMAND  - обработать меню приложения
//  WM_PAINT    - Отрисовка главного окна
//  WM_DESTROY  - отправить сообщение о выходе и вернуться
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        {
            HMENU fileMenu = GetSubMenu(GetMenu(hWnd), 0);
            if (!fileMenu || !InsertMenuW(fileMenu, 0,
                MF_BYPOSITION | MF_STRING, ID_OPEN_BMP, L"Открыть BMP..."))
            {
                MessageBoxW(hWnd, L"Не удалось добавить команду открытия файла.",
                    L"Ошибка", MB_OK | MB_ICONERROR);
                return -1;
            }
            DrawMenuBar(hWnd);
        }
        return 0;
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Разобрать выбор в меню:
            switch (wmId)
            {
            case ID_OPEN_BMP:
                OpenBitmapFile(hWnd);
                break;
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            DrawBitmap(hWnd, hdc);
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_SIZE:
        InvalidateRect(hWnd, nullptr, TRUE);
        return 0;
    case WM_DESTROY:
        if (loadedBitmap)
        {
            DeleteObject(loadedBitmap);
            loadedBitmap = nullptr;
        }
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Обработчик сообщений для окна "О программе".
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
