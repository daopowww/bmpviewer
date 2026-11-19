// bmpviewer.cpp : Определяет точку входа для приложения.
//

#include "framework.h"
#include "bmpviewer.h"
#include <commdlg.h>
#include <windowsx.h>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Gdi32.lib")

#define MAX_LOADSTRING 100

constexpr UINT ID_OPEN_BMP = 50001;
constexpr int IMAGE_MARGIN = 20;

HBITMAP loadedBitmap = nullptr;
HBITMAP loadedMask = nullptr;
int imageWidth = 0;
int imageHeight = 0;

int imageRightOffset = IMAGE_MARGIN;
int imageTopOffset = IMAGE_MARGIN;
bool dragging = false;
POINT dragOffset = {};

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
POINT GetImagePosition(HWND hWnd);
bool HitTestImage(HWND hWnd, int mouseX, int mouseY);
void StopDragging(HWND hWnd);
void MoveImage(HWND hWnd, int mouseX, int mouseY);
void ChooseFigureColor(HWND hWnd);

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
    wcex.hbrBackground  = CreateSolidBrush(RGB(100, 0, 0));
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

struct FigureColors
{
    std::uint32_t fill = 0;
    std::uint32_t text = 0;
};

struct ProcessedImage
{
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> colors;
    std::vector<std::uint32_t> mask;
    FigureColors palette;
};

ProcessedImage originalImage;
std::uint32_t displayedFigureColor = 0;

ProcessedImage RemoveOuterBackground(
    const std::vector<std::uint32_t>& pixels, int width, int height)
{
    if (width <= 0 || height <= 0
        || pixels.size() % static_cast<std::size_t>(width) != 0
        || pixels.size() / static_cast<std::size_t>(width)
            != static_cast<std::size_t>(height))
    {
        throw std::invalid_argument("Invalid image dimensions");
    }

    // Цвет фона определяется по наиболее частому цвету на границе изображения.
    std::unordered_map<std::uint32_t, std::size_t> borderColors;
    for (int x = 0; x < width; ++x)
    {
        ++borderColors[pixels[x] & 0x00FFFFFFu];
        if (height > 1)
        {
            ++borderColors[pixels[static_cast<std::size_t>(height - 1)
                * width + x] & 0x00FFFFFFu];
        }
    }
    for (int y = 1; y < height - 1; ++y)
    {
        const std::size_t row = static_cast<std::size_t>(y) * width;
        ++borderColors[pixels[row] & 0x00FFFFFFu];
        if (width > 1)
        {
            ++borderColors[pixels[row + width - 1] & 0x00FFFFFFu];
        }
    }

    std::uint32_t backgroundColor = pixels.front() & 0x00FFFFFFu;
    std::size_t largestCount = borderColors.at(backgroundColor);
    for (const auto& entry : borderColors)
    {
        if (entry.second > largestCount)
        {
            backgroundColor = entry.first;
            largestCount = entry.second;
        }
    }

    std::vector<unsigned char> background(pixels.size(), 0);
    std::vector<std::size_t> queue;
    auto addBackground = [&](std::size_t index)
    {
        if (!background[index]
            && (pixels[index] & 0x00FFFFFFu) == backgroundColor)
        {
            background[index] = 1;
            queue.push_back(index);
        }
    };

    for (int x = 0; x < width; ++x)
    {
        addBackground(x);
        addBackground(static_cast<std::size_t>(height - 1) * width + x);
    }
    for (int y = 1; y < height - 1; ++y)
    {
        addBackground(static_cast<std::size_t>(y) * width);
        addBackground(static_cast<std::size_t>(y) * width + width - 1);
    }

    // Обход по четырём соседям сохраняет замкнутые области внутри фигуры,
    // в том числе надпись того же цвета, что и внешний фон.
    for (std::size_t current = 0; current < queue.size(); ++current)
    {
        const std::size_t index = queue[current];
        const int x = static_cast<int>(index % width);
        const int y = static_cast<int>(index / width);
        if (x > 0)
        {
            addBackground(index - 1);
        }
        if (x + 1 < width)
        {
            addBackground(index + 1);
        }
        if (y > 0)
        {
            addBackground(index - width);
        }
        if (y + 1 < height)
        {
            addBackground(index + width);
        }
    }

    int left = width;
    int top = height;
    int right = -1;
    int bottom = -1;
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            if (!background[static_cast<std::size_t>(y) * width + x])
            {
                left = (std::min)(left, x);
                top = (std::min)(top, y);
                right = (std::max)(right, x);
                bottom = (std::max)(bottom, y);
            }
        }
    }
    if (right < left)
    {
        throw std::invalid_argument("The image contains only background");
    }

    ProcessedImage result;
    result.width = right - left + 1;
    result.height = bottom - top + 1;
    const std::size_t resultSize = static_cast<std::size_t>(result.width)
        * result.height;
    result.colors.resize(resultSize);
    result.mask.resize(resultSize);
    for (int y = 0; y < result.height; ++y)
    {
        for (int x = 0; x < result.width; ++x)
        {
            const std::size_t source = static_cast<std::size_t>(top + y)
                * width + left + x;
            const std::size_t target = static_cast<std::size_t>(y)
                * result.width + x;
            const bool transparent = background[source] != 0;
            result.mask[target] = transparent ? 0x00FFFFFFu : 0;
            result.colors[target] = transparent ? 0
                : pixels[source] & 0x00FFFFFFu;
        }
    }
    return result;
}

// Основной цвет определяется по непрозрачным пикселям исходной фигуры.
FigureColors DetectFigureColors(const ProcessedImage& image)
{
    std::unordered_map<std::uint32_t, std::size_t> counts;
    for (std::size_t i = 0; i < image.colors.size(); ++i)
    {
        if (image.mask[i] == 0)
        {
            ++counts[image.colors[i]];
        }
    }
    if (counts.empty())
    {
        throw std::invalid_argument("The image has no visible pixels");
    }

    FigureColors result;
    std::size_t largestCount = 0;
    for (const auto& entry : counts)
    {
        if (entry.second > largestCount)
        {
            result.fill = entry.first;
            largestCount = entry.second;
        }
    }

    result.text = result.fill;
    largestCount = 0;
    for (const auto& entry : counts)
    {
        if (entry.first != result.fill && entry.second > largestCount)
        {
            result.text = entry.first;
            largestCount = entry.second;
        }
    }
    return result;
}

// Каждый выбор цвета рассчитывается по исходному изображению.
std::vector<std::uint32_t> RecolorImage(
    const ProcessedImage& image, std::uint32_t newFill)
{
    std::vector<std::uint32_t> result = image.colors;
    const std::uint32_t fill = image.palette.fill;
    if (newFill == fill)
    {
        return result;
    }
    const std::uint32_t text = image.palette.text;
    double fillChannels[3];
    double textChannels[3];
    double newChannels[3];
    double direction[3];
    double lengthSquared = 0;
    for (int channel = 0; channel < 3; ++channel)
    {
        const int shift = 16 - channel * 8;
        fillChannels[channel] = (fill >> shift) & 255u;
        textChannels[channel] = (text >> shift) & 255u;
        newChannels[channel] = (newFill >> shift) & 255u;
        direction[channel] = textChannels[channel] - fillChannels[channel];
        lengthSquared += direction[channel] * direction[channel];
    }

    for (std::size_t i = 0; i < result.size(); ++i)
    {
        if (image.mask[i] != 0)
        {
            continue;
        }
        const std::uint32_t pixel = image.colors[i];
        if (pixel == fill)
        {
            result[i] = newFill;
            continue;
        }
        if (pixel == text || lengthSquared == 0)
        {
            continue;
        }

        // Сглаженные края букв содержат смесь цвета текста и заливки.
        double channels[3];
        double projection = 0;
        for (int channel = 0; channel < 3; ++channel)
        {
            channels[channel] = (pixel >> (16 - channel * 8)) & 255u;
            projection += (channels[channel] - fillChannels[channel])
                * direction[channel];
        }
        const double textWeight = projection / lengthSquared;
        if (textWeight < 0 || textWeight > 1)
        {
            continue;
        }

        bool isBlendedText = true;
        for (int channel = 0; channel < 3; ++channel)
        {
            const double expected = fillChannels[channel]
                + textWeight * direction[channel];
            if (std::abs(channels[channel] - expected) > 2.0)
            {
                isBlendedText = false;
                break;
            }
        }
        if (!isBlendedText)
        {
            continue;
        }

        std::uint32_t blended = 0;
        for (int channel = 0; channel < 3; ++channel)
        {
            const double value = newChannels[channel] * (1 - textWeight)
                + textChannels[channel] * textWeight;
            blended |= static_cast<std::uint32_t>(std::lround(value))
                << (16 - channel * 8);
        }
        result[i] = blended;
    }
    return result;
}

void ChooseFigureColor(HWND hWnd)
{
    StopDragging(hWnd);
    static COLORREF customColors[16] = {};
    CHOOSECOLORW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = hWnd;
    dialog.lpCustColors = customColors;
    dialog.rgbResult = RGB((displayedFigureColor >> 16) & 255u,
        (displayedFigureColor >> 8) & 255u, displayedFigureColor & 255u);
    dialog.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (!ChooseColorW(&dialog))
    {
        if (CommDlgExtendedError() != 0)
        {
            MessageBoxW(hWnd, L"Не удалось открыть диалог выбора цвета.",
                L"Ошибка", MB_OK | MB_ICONERROR);
        }
        return;
    }

    const std::uint32_t newColor =
        (static_cast<std::uint32_t>(GetRValue(dialog.rgbResult)) << 16)
        | (static_cast<std::uint32_t>(GetGValue(dialog.rgbResult)) << 8)
        | GetBValue(dialog.rgbResult);
    try
    {
        const auto pixels = RecolorImage(originalImage, newColor);
        DIBSECTION section = {};
        if (GetObjectW(loadedBitmap, sizeof(section), &section) == 0
            || !section.dsBm.bmBits)
        {
            throw std::runtime_error("Cannot access bitmap pixels");
        }
        GdiFlush();
        std::memcpy(section.dsBm.bmBits, pixels.data(),
            pixels.size() * sizeof(std::uint32_t));
        displayedFigureColor = newColor;
    }
    catch (const std::exception&)
    {
        MessageBoxW(hWnd, L"Не удалось изменить цвет фигуры.",
            L"Ошибка", MB_OK | MB_ICONERROR);
        return;
    }
    InvalidateRect(hWnd, nullptr, TRUE);
}

// Приводим пиксели к 32 битам и создаём изображение с маской прозрачности.
bool PrepareBitmap(HWND hWnd, HBITMAP source, const BITMAP& sourceInfo,
    HBITMAP& colorBitmap, HBITMAP& maskBitmap, int& width, int& height,
    ProcessedImage& savedImage)
{
    ProcessedImage image;
    try
    {
        const std::size_t sourceWidth = sourceInfo.bmWidth;
        const std::size_t sourceHeight = sourceInfo.bmHeight;
        std::vector<std::uint32_t> pixels;
        if (sourceWidth > pixels.max_size() / sourceHeight)
        {
            throw std::length_error("Image too large");
        }
        pixels.resize(sourceWidth * sourceHeight);

        BITMAPINFO sourceFormat = {};
        sourceFormat.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        sourceFormat.bmiHeader.biWidth = sourceInfo.bmWidth;
        sourceFormat.bmiHeader.biHeight = -sourceInfo.bmHeight;
        sourceFormat.bmiHeader.biPlanes = 1;
        sourceFormat.bmiHeader.biBitCount = 32;
        sourceFormat.bmiHeader.biCompression = BI_RGB;

        HDC hdc = GetDC(hWnd);
        if (!hdc)
        {
            throw std::runtime_error("Cannot access window device context");
        }
        const int lines = GetDIBits(hdc, source, 0, sourceInfo.bmHeight,
            pixels.data(), &sourceFormat, DIB_RGB_COLORS);
        ReleaseDC(hWnd, hdc);
        if (lines != sourceInfo.bmHeight)
        {
            throw std::runtime_error("Cannot read bitmap pixels");
        }

        image = RemoveOuterBackground(pixels,
            sourceInfo.bmWidth, sourceInfo.bmHeight);
        image.palette = DetectFigureColors(image);
    }
    catch (const std::exception&)
    {
        MessageBoxW(hWnd,
            L"Не удалось обработать изображение. Проверьте файл, "
            L"наличие фигуры на однотонном фоне и доступную память.",
            L"Ошибка обработки", MB_OK | MB_ICONERROR);
        return false;
    }

    BITMAPINFO format = {};
    format.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    format.bmiHeader.biWidth = image.width;
    format.bmiHeader.biHeight = -image.height;
    format.bmiHeader.biPlanes = 1;
    format.bmiHeader.biBitCount = 32;
    format.bmiHeader.biCompression = BI_RGB;

    void* colorBits = nullptr;
    HBITMAP newColor = CreateDIBSection(nullptr, &format,
        DIB_RGB_COLORS, &colorBits, nullptr, 0);
    if (!newColor)
    {
        MessageBoxW(hWnd, L"Не удалось создать изображение.",
            L"Ошибка памяти", MB_OK | MB_ICONERROR);
        return false;
    }

    void* maskBits = nullptr;
    HBITMAP newMask = CreateDIBSection(nullptr, &format,
        DIB_RGB_COLORS, &maskBits, nullptr, 0);
    if (!newMask)
    {
        DeleteObject(newColor);
        MessageBoxW(hWnd, L"Не удалось создать маску изображения.",
            L"Ошибка памяти", MB_OK | MB_ICONERROR);
        return false;
    }

    const std::size_t byteCount = image.colors.size() * sizeof(std::uint32_t);
    std::memcpy(colorBits, image.colors.data(), byteCount);
    std::memcpy(maskBits, image.mask.data(), byteCount);
    colorBitmap = newColor;
    maskBitmap = newMask;
    width = image.width;
    height = image.height;
    savedImage = std::move(image);
    return true;
}

// Выбор и загрузка BMP. Старое изображение заменяется после успешной загрузки.
void OpenBitmapFile(HWND hWnd)
{
    StopDragging(hWnd);
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
        LR_LOADFROMFILE));

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

    HBITMAP newColor = nullptr;
    HBITMAP newMask = nullptr;
    int newWidth = 0;
    int newHeight = 0;
    ProcessedImage newOriginal;
    const bool prepared = PrepareBitmap(hWnd, newBitmap, newBitmapInfo,
        newColor, newMask, newWidth, newHeight, newOriginal);
    DeleteObject(newBitmap);
    if (!prepared)
    {
        return;
    }

    if (loadedBitmap)
    {
        DeleteObject(loadedBitmap);
    }
    if (loadedMask)
    {
        DeleteObject(loadedMask);
    }
    loadedBitmap = newColor;
    loadedMask = newMask;
    imageWidth = newWidth;
    imageHeight = newHeight;
    originalImage = std::move(newOriginal);
    displayedFigureColor = originalImage.palette.fill;
    imageRightOffset = IMAGE_MARGIN;
    imageTopOffset = IMAGE_MARGIN;

    InvalidateRect(hWnd, nullptr, TRUE);
}

// После перетаскивания сохраняются новые отступы от правого и верхнего краёв.
POINT GetImagePosition(HWND hWnd)
{
    RECT clientRect = {};
    GetClientRect(hWnd, &clientRect);
    const int clientWidth = clientRect.right;
    const int clientHeight = clientRect.bottom;
    const int maxX = (std::max)(0, clientWidth - imageWidth);
    const int maxY = (std::max)(0, clientHeight - imageHeight);

    POINT position = {};
    position.x = (std::min)(maxX,
        (std::max)(0, clientWidth - imageWidth - imageRightOffset));
    position.y = (std::min)(maxY, imageTopOffset);
    return position;
}

// Учитываем маску, чтобы захватывать мышью видимые пиксели фигуры.
bool HitTestImage(HWND hWnd, int mouseX, int mouseY)
{
    if (!loadedBitmap || !loadedMask)
    {
        return false;
    }

    const POINT position = GetImagePosition(hWnd);
    const int x = mouseX - position.x;
    const int y = mouseY - position.y;
    if (x < 0 || y < 0 || x >= imageWidth || y >= imageHeight)
    {
        return false;
    }

    HDC memoryDC = CreateCompatibleDC(nullptr);
    if (!memoryDC)
    {
        return false;
    }
    HGDIOBJ previousBitmap = SelectObject(memoryDC, loadedMask);
    if (!previousBitmap || previousBitmap == HGDI_ERROR)
    {
        DeleteDC(memoryDC);
        return false;
    }

    const COLORREF pixel = GetPixel(memoryDC, x, y);
    SelectObject(memoryDC, previousBitmap);
    DeleteDC(memoryDC);
    return pixel == RGB(0, 0, 0);
}

void StopDragging(HWND hWnd)
{
    dragging = false;
    if (GetCapture() == hWnd)
    {
        ReleaseCapture();
    }
}

void MoveImage(HWND hWnd, int mouseX, int mouseY)
{
    RECT clientRect = {};
    GetClientRect(hWnd, &clientRect);
    const int clientWidth = clientRect.right;
    const int clientHeight = clientRect.bottom;
    const int maxX = (std::max)(0, clientWidth - imageWidth);
    const int maxY = (std::max)(0, clientHeight - imageHeight);
    const int x = (std::min)(maxX,
        (std::max)(0, mouseX - static_cast<int>(dragOffset.x)));
    const int y = (std::min)(maxY,
        (std::max)(0, mouseY - static_cast<int>(dragOffset.y)));
    const POINT oldPosition = GetImagePosition(hWnd);

    imageRightOffset = (std::max)(0, clientWidth - imageWidth - x);
    imageTopOffset = y;
    if (oldPosition.x != x || oldPosition.y != y)
    {
        InvalidateRect(hWnd, nullptr, TRUE);
    }
}

// Положение пересчитывается при каждой перерисовке окна.
void DrawBitmap(HWND hWnd, HDC hdc)
{
    if (!loadedBitmap || !loadedMask)
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

    HGDIOBJ selectedMask = SelectObject(memoryDC, loadedMask);
    if (!selectedMask || selectedMask == HGDI_ERROR)
    {
        SelectObject(memoryDC, previousBitmap);
        DeleteDC(memoryDC);
        return;
    }

    const POINT position = GetImagePosition(hWnd);

    // Маска оставляет фон окна и очищает область фигуры.
    BitBlt(hdc, position.x, position.y, imageWidth, imageHeight,
        memoryDC, 0, 0, SRCAND);
    SelectObject(memoryDC, loadedBitmap);
    // Добавляем пиксели фигуры и надпись в очищенную область.
    BitBlt(hdc, position.x, position.y, imageWidth, imageHeight,
        memoryDC, 0, 0, SRCPAINT);

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
    case WM_LBUTTONDOWN:
        {
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);
            if (HitTestImage(hWnd, x, y))
            {
                const POINT position = GetImagePosition(hWnd);
                dragOffset.x = x - position.x;
                dragOffset.y = y - position.y;
                SetCapture(hWnd);
                dragging = GetCapture() == hWnd;
            }
        }
        return 0;
    case WM_MOUSEMOVE:
        if (dragging)
        {
            if (wParam & MK_LBUTTON)
            {
                MoveImage(hWnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            }
            else
            {
                StopDragging(hWnd);
            }
        }
        return 0;
    case WM_LBUTTONUP:
        if (dragging)
        {
            MoveImage(hWnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            StopDragging(hWnd);
        }
        return 0;
    case WM_RBUTTONUP:
        if (!dragging && HitTestImage(hWnd,
            GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)))
        {
            ChooseFigureColor(hWnd);
        }
        return 0;
    case WM_CAPTURECHANGED:
        dragging = false;
        return 0;
    case WM_CANCELMODE:
        StopDragging(hWnd);
        return 0;
    case WM_SIZE:
        StopDragging(hWnd);
        InvalidateRect(hWnd, nullptr, TRUE);
        return 0;
    case WM_DESTROY:
        StopDragging(hWnd);
        if (loadedBitmap)
        {
            DeleteObject(loadedBitmap);
            loadedBitmap = nullptr;
        }
        if (loadedMask)
        {
            DeleteObject(loadedMask);
            loadedMask = nullptr;
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
