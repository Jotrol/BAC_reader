#include <Windows.h>

#include "Crypto.hpp"
#include "CardReader.hpp"
#include "MRZDecoder.hpp"
#include "ImageContainer.hpp"
#include "Passport.hpp"

#include <fstream>
using namespace std;

#define IDC_BUTTON_LOAD 0x1000
#define IDC_BUTTON_CONNECT 0x1001

string getMRZCode() {
	return "5200000001RUS8008046F0902274<<<<<<<<<<<<<<06";
}

HINSTANCE ghInst = nullptr;
ImageContainer::ImageWindow* imageWindow = nullptr;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_CLOSE) {
		PostQuitMessage(0);
		return 0;
	}

	if (msg == WM_COMMAND && wParam == IDC_BUTTON_LOAD) {
		if (imageWindow) {
			DestroyWindow(imageWindow->getHWND());
			delete imageWindow;
		}

		ifstream file("imageContainer.bin", ios::binary);
		ImageContainer::Image image(file);
		imageWindow = new ImageContainer::ImageWindow(ghInst, image, hWnd, 100, 100);

		UpdateWindow(hWnd);
		InvalidateRect(hWnd, nullptr, TRUE);
		return 0;
	}

	//if (msg == WM_COMMAND && wParam == IDC_BUTTON_CONNECT) {
	//	passport.connectPassport(reader, readerName);
	//	passport.perfomBAC(reader, getMRZCode());

	//	MessageBox(hWnd, L"Успешно!", L"", MB_OK);
	//}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, INT nCmdShow) {
	//CardReader reader;
	//auto& readers = reader.getReadersList();
	//wstring readerName = readers[0];

	//Passport passport;
	//passport.connectPassport(reader, readerName);
	//passport.perfomBAC(reader, getMRZCode());
	//passport.readDG(reader, Passport::DGFILES::DG1);
	//passport.readDG(reader, Passport::DGFILES::DG2);

	//ByteVec dg2Data = passport.getPassportRawImage();
	//ofstream ofile("imageContainer.bin", ios::binary | ios::trunc);
	//ofile.write((char*)dg2Data.data(), dg2Data.size());
	//ofile.close();

	ImageContainer::ImageWindow::RegisterImageWindowClass(hInst);

	WNDCLASS wc = { 0 };
	wc.lpszClassName = L"MainWindow";
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.hInstance = hInst;
	wc.lpfnWndProc = WndProc;

	ghInst = hInst;
	RegisterClass(&wc);

	HWND hWnd = CreateWindow(L"MainWindow", L"Window", WS_OVERLAPPEDWINDOW, 0, 0, 500, 500, nullptr, nullptr, hInst, nullptr);
	CreateWindow(L"BUTTON", L"Загрузить", WS_CHILD | WS_VISIBLE, 0, 0, 100, 20, hWnd, (HMENU)IDC_BUTTON_LOAD, hInst, nullptr);
	CreateWindow(L"BUTTON", L"Connect", WS_CHILD | WS_VISIBLE, 0, 40, 100, 20, hWnd, (HMENU)IDC_BUTTON_CONNECT, hInst, nullptr);

	UpdateWindow(hWnd);
	ShowWindow(hWnd, nCmdShow);

	MSG msg = { 0 };
	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}