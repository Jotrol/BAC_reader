#include <Windows.h>

#include "Passport.hpp"
#include "ImageContainer.hpp"

#include <random>
#include <ctime>
using namespace std;

string getMRZCode1() {
	return "5200000001RUS8008046F0902274<<<<<<<<<<<<<<<06";
}

#define IDC_BUTTON_READ 0x1000
#define IDC_BUTTON_CLEAR 0x1001
#define IDC_BUTTON_CONNECT 0x1002

fipImage image;
bool isLoaded = false;
BITMAPINFO bmi = { 0 };

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_CLOSE) {
		PostQuitMessage(0);
		return 0;
	}

	if (msg == WM_PAINT) {
		if (isLoaded) {
			PAINTSTRUCT ps = { 0 };
			HDC hdc = BeginPaint(hWnd, &ps);
			FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
			SetDIBitsToDevice(hdc, 0, 0, image.getWidth(), image.getHeight(), 0, 0, 0, image.getHeight(), image.accessPixels(), &bmi, DIB_RGB_COLORS);
			EndPaint(hWnd, &ps);
			return 0;
		}
	}

	if (msg == WM_COMMAND && wParam == IDC_BUTTON_READ) {
		//ImageContainer::ImageStream container("container.bin", ios::in | ios::out | ios::binary);
		//container.seekg(0);

		//ImageContainer::Image isoImage(container);
		//ImageContainer::ImageStream imageFile("image", ios::in | ios::out | ios::trunc);
		//imageFile.write(isoImage.getRawImage(), isoImage.getRawImageSize());
		//imageFile.close();

		image.clear();
		if (!image.load("image.jpg")) {
			throw std::exception();
		}
		if (!image.convertTo24Bits()) {
			throw std::exception();
		}

		bmi.bmiHeader.biWidth = image.getWidth();
		bmi.bmiHeader.biHeight = image.getHeight();
		bmi.bmiHeader.biBitCount = image.getBitsPerPixel();
		bmi.bmiHeader.biPlanes = 1;

		isLoaded = true;
		InvalidateRect(hWnd, nullptr, TRUE);
		return 0;
	}
	
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow) {
	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInst;
	wc.lpszClassName = L"MainWindow";
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

	RegisterClass(&wc);

	HWND hWnd = CreateWindow(L"MainWindow", L"Main Window", WS_OVERLAPPEDWINDOW, 0, 0, 300, 300, nullptr, nullptr, hInst, nullptr);
	CreateWindow(L"BUTTON", L"Показать изображение", WS_CHILD | WS_OVERLAPPED | WS_VISIBLE | WS_BORDER, 0, 0, 170, 25, hWnd, (HMENU)IDC_BUTTON_READ, hInst, nullptr);

	UpdateWindow(hWnd);
	ShowWindow(hWnd, nCmdShow);

	MSG msg = { 0 };
	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}