#include <Windows.h>

#include "Crypto.hpp"
#include "CardReader.hpp"
#include "MRZDecoder.hpp"

#include "FreeImagePlus.h"
#pragma comment(lib, "FreeImagePlus.lib")
using namespace std;

#include <objidl.h>
#include <gdiplus.h>
#include <Shlwapi.h>

using namespace Gdiplus;
#pragma comment (lib, "Gdiplus.lib")
#pragma comment (lib, "Shlwapi.lib")

Gdiplus::Bitmap* image = nullptr;
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_PAINT:
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        
        Graphics graphics(hdc);
        graphics.DrawImage(image, 0, 0);

        EndPaint(hwnd, &ps);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, PSTR pCmdLine, int nCmdShow) {
    ULONG_PTR token;
    GdiplusStartupInput input = { 0 };
    input.GdiplusVersion = 1;
    GdiplusStartup(&token, &input, NULL);

    const wchar_t CLASS_NAME[] = L"Sample Window Class";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    fipImage fipimage;
    if (!fipimage.load("FaceImage.jp2")) {
        MessageBox(nullptr, L"Не загрузилось изображение", L"Ошибка", MB_OK | MB_ICONERROR);
    }

    HWND hwnd = CreateWindow(CLASS_NAME, L"Learn to Program Windows", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, fipimage.getWidth(), fipimage.getHeight() + 38,NULL,NULL,hInstance,NULL);
    if (!hwnd) {
        cerr << "Can't create window!" << endl;
        GdiplusShutdown(token);
        return 1;
    }

    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = fipimage.getWidth();
    bmi.bmiHeader.biHeight = fipimage.getHeight();
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biBitCount = fipimage.getBitsPerPixel();
    image = new Gdiplus::Bitmap(&bmi, fipimage.accessPixels());

    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return 0;
}