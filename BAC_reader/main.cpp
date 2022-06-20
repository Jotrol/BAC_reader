#include <Windows.h>

#include "Crypto.hpp"
#include "CardReader.hpp"
#include "MRZDecoder.hpp"

#include "FreeImagePlus.h"
#pragma comment(lib, "FreeImagePlus.lib")
using namespace std;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static fipImage* fipimage = nullptr;
	static BITMAPINFO bmi = { 0 };

	switch (uMsg) {
	case WM_SHOWWINDOW:
		fipimage = (fipImage*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = fipimage->getWidth();
		bmi.bmiHeader.biHeight = fipimage->getHeight();
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biCompression = BI_RGB;
		bmi.bmiHeader.biBitCount = fipimage->getBitsPerPixel();
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_PAINT:
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);
		SetDIBitsToDevice(hdc, 0, 0, fipimage->getWidth(), fipimage->getHeight(), 0, 0, 0, fipimage->getHeight(), fipimage->accessPixels(), &bmi, DIB_RGB_COLORS);
		EndPaint(hwnd, &ps);
		return 0;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK ParentProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

#define CLASS_NAME L"Sample Window Class"

HWND createImageWindow(HINSTANCE hInst, const char* filename, HWND parentWnd, fipImage& image) {
	WNDCLASS wc = {};
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInst;
	wc.lpszClassName = CLASS_NAME;
	if (!RegisterClass(&wc)) {
		MessageBox(nullptr, L"Не удалось зарегистрировать класс окна", L"Ошибка", MB_OK | MB_ICONERROR);
		return nullptr;
	}

	if (!image.load(filename)) {
		MessageBox(nullptr, L"Не загрузилось изображение", L"Ошибка", MB_OK | MB_ICONERROR);
		return nullptr;
	}
	image.convertTo24Bits();

	HWND hWnd = CreateWindow(CLASS_NAME, L"Learn to Program Windows", ~WS_BORDER & ~WS_THICKFRAME & (WS_CHILD | WS_OVERLAPPEDWINDOW), CW_USEDEFAULT, CW_USEDEFAULT, image.getWidth(), image.getHeight(), parentWnd, NULL, hInst, NULL);
	if (!hWnd) {
		MessageBox(nullptr, L"Не удалось создать окно", L"Ошибка", MB_OK | MB_ICONERROR);
		return nullptr;
	}

	SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)&image);
	UpdateWindow(hWnd);
	ShowWindow(hWnd, 1);
	return hWnd;
}
HWND createParentWindow(HINSTANCE hInst) {
	WNDCLASS wc = {};
	wc.lpfnWndProc = ParentProc;
	wc.hInstance = hInst;
	wc.lpszClassName = L"ParentWindow";
	if (!RegisterClass(&wc)) {
		MessageBox(nullptr, L"Не удалось зарегистрировать класс окна", L"Ошибка", MB_OK | MB_ICONERROR);
		return nullptr;
	}

	HWND hWnd = CreateWindow(L"ParentWindow", L"Parent Window", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 500, 500, nullptr, nullptr, hInst, nullptr);
	if (!hWnd) {
		MessageBox(nullptr, L"Не удалось создать окно", L"Ошибка", MB_OK | MB_ICONERROR);
		return nullptr;
	}

	UpdateWindow(hWnd);
	ShowWindow(hWnd, 1);
	return hWnd;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, PSTR pCmdLine, int nCmdShow) {
	fipImage image;

	HWND parentWindow = createParentWindow(hInstance);
	HWND imageWindow = createImageWindow(hInstance, "Faceimage.jp2", parentWindow, image);

	MSG msg = { 0 };
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}