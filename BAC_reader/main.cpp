#include <Windows.h>

#include "Crypto.hpp"
#include "CardReader.hpp"
#include "MRZDecoder.hpp"
#include "ImageContainer.hpp"

#include <fstream>
using namespace std;

#define IDC_BUTTON_LOAD 0x1000

HINSTANCE ghInst = nullptr;
ImageContainer::ImageWindow* imageWindow;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_CLOSE) {
		PostQuitMessage(0);
		return 0;
	}

	if (msg == WM_COMMAND && wParam == IDC_BUTTON_LOAD) {
		ifstream file("EncodedPhotoLytkina.bin", ios::binary);
		ImageContainer::Image image(file);

		delete imageWindow;
		imageWindow = new ImageContainer::ImageWindow(ghInst, image, hWnd, 100, 100);

		UpdateWindow(hWnd);
		InvalidateRect(hWnd, nullptr, TRUE);
		return 0;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, INT nCmdShow) {
	WNDCLASS wc = { 0 };
	wc.lpszClassName = L"MainWindow";
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.hInstance = hInst;
	wc.lpfnWndProc = WndProc;

	ghInst = hInst;
	RegisterClass(&wc);

	HWND hWnd = CreateWindow(L"MainWindow", L"Window", WS_OVERLAPPEDWINDOW, 0, 0, 500, 500, nullptr, nullptr, hInst, nullptr);
	CreateWindow(L"BUTTON", L"Загрузить", WS_CHILD | WS_VISIBLE, 0, 0, 100, 20, hWnd, (HMENU)IDC_BUTTON_LOAD, hInst, nullptr);

	UpdateWindow(hWnd);
	ShowWindow(hWnd, nCmdShow);

	MSG msg = { 0 };
	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}