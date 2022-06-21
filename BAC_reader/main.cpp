#include <Windows.h>

#include "Crypto.hpp"
#include "CardReader.hpp"
#include "MRZDecoder.hpp"

#include "FreeImagePlus.h"
#pragma comment(lib, "FreeImagePlus.lib")
using namespace std;

LRESULT CALLBACK ImageWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
class ImageWindow {
private:
	fipImage image;
	BITMAPINFO bmi;
	HWND hWnd;
	
	const wchar_t* ImageClassName = L"ImageWindow";
	const DWORD ImageWindowStyle = WS_CHILD | WS_OVERLAPPED;

	bool RegisterImageWindowClass(HINSTANCE hInst) {
		WNDCLASS wc = { 0 };
		wc.hInstance = hInst;
		wc.lpfnWndProc = ImageWindowProc;
		wc.lpszClassName = ImageClassName;

		return RegisterClass(&wc);
	}
public:
	ImageWindow(HINSTANCE hInst, HWND hParentWnd, INT x, INT y, const char* filename) {
		if (!image.load(filename)) {
			throw std::exception("Ошибка: не удалось загрузить фотографию");
		}
		if (!image.convertTo24Bits()) {
			throw std::exception("Ошибка: не удалось конвертировать фотографию");
		}

		DWORD w = image.getWidth();
		DWORD h = image.getHeight();
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = w;
		bmi.bmiHeader.biHeight = h;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biCompression = BI_RGB;
		bmi.bmiHeader.biBitCount = image.getBitsPerPixel();

		hWnd = CreateWindow(ImageClassName, L"", ImageWindowStyle, x, y, w, h, hParentWnd, nullptr, hInst, nullptr);
		if (!hWnd) {
			if (!RegisterImageWindowClass(hInst)) {
				throw std::exception("Ошибка: не удалось зарегистрировать класс окна для изображений");
			}
			hWnd = CreateWindow(ImageClassName, L"", ImageWindowStyle, x, y, w, h, hParentWnd, nullptr, hInst, nullptr);
			if (!hWnd) {
				throw std::exception("Ошибка: не удалось создать окно");
			}
		}

		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)this);
		UpdateWindow(hWnd);
		ShowWindow(hWnd, 1);
	}

	bool reloadImage(const char* filename) {
		image.clear();

		if (!image.load(filename)) {
			return false;
		}
		if (!image.convertTo24Bits()) {
			image.clear();
			return false;
		}

		DWORD w = image.getWidth();
		DWORD h = image.getHeight();
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = w;
		bmi.bmiHeader.biHeight = h;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biCompression = BI_RGB;
		bmi.bmiHeader.biBitCount = image.getBitsPerPixel();

		MoveWindow(hWnd, 0, 0, w, h, false);
		InvalidateRect(hWnd, nullptr, true);
		return true;
	}
	
	LRESULT CALLBACK handleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
		case WM_PAINT:
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
			SetDIBitsToDevice(hdc, 0, 0, image.getWidth(), image.getHeight(), 0, 0, 0, image.getHeight(), image.accessPixels(), &bmi, DIB_RGB_COLORS);
			EndPaint(hWnd, &ps);
			return 0;
		}

		return DefWindowProc(hWnd, message, wParam, lParam);
	}
};
LRESULT CALLBACK ImageWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	ImageWindow* imageWindow = (ImageWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (message) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return (imageWindow ? imageWindow->handleMessage(hWnd, message, wParam, lParam)
		: DefWindowProc(hWnd, message, wParam, lParam));
}

ImageWindow* image = nullptr;
LRESULT CALLBACK ParentProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_PAINT:
		PAINTSTRUCT ps = { 0 };
		HDC hdc = BeginPaint(hWnd, &ps);
		FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
		EndPaint(hWnd, &ps);
		return 0;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
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
	HWND parentWindow = createParentWindow(hInstance);
	image = new ImageWindow(hInstance, parentWindow, 0, 0, "Faceimage.jp2");

	MSG msg = { 0 };
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}