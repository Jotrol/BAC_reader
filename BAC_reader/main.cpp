#include <Windows.h>

#include "Crypto.hpp"
#include "CardReader.hpp"
#include "MRZDecoder.hpp"

#include "FreeImagePlus.h"
#pragma comment(lib, "FreeImagePlus.lib")
using namespace std;

fipImage fipimage;
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static BITMAPINFO bmi = { 0 };
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = fipimage.getWidth();
	bmi.bmiHeader.biHeight = fipimage.getHeight();
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biBitCount = fipimage.getBitsPerPixel();

	switch (uMsg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_PAINT:
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);
		SetDIBitsToDevice(hdc, 0, 0, fipimage.getWidth(), fipimage.getHeight(), 0, 0, 0, fipimage.getHeight(), fipimage.accessPixels(), &bmi, DIB_RGB_COLORS);
		EndPaint(hwnd, &ps);
		return 0;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

#define CLASS_NAME L"Sample Window Class"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, PSTR pCmdLine, int nCmdShow) {
	WNDCLASS wc = {};
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	RegisterClass(&wc);

	if (!fipimage.load("FaceImage.jp2")) {
		MessageBox(nullptr, L"Не загрузилось изображение", L"Ошибка", MB_OK | MB_ICONERROR);
	}

	HWND hwnd = CreateWindow(CLASS_NAME, L"Learn to Program Windows", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, fipimage.getWidth(), fipimage.getHeight() + 38, NULL, NULL, hInstance, NULL);
	if (!hwnd) {
		cerr << "Can't create window!" << endl;
		return 1;
	}

	ShowWindow(hwnd, nCmdShow);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}