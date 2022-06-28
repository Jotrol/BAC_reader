#pragma once

#include <Windows.h>
#include "ImageContainer.hpp"
#include "Passport.hpp"
#include "CardReader.hpp"
#include "Util.hpp"

#include <fstream>
using namespace std;

namespace GUI {
	LRESULT CALLBACK ImageWndProc(HWND, UINT, WPARAM, LPARAM);
	class ImageWindow {
		HWND hWnd;
		fipImage image;
		BITMAPINFO bmi;
		POINT imagePos;

		const INT INIT_WIDTH = 300;
		const INT INIT_HEIGHT = 400;
		const wchar_t* CLASS_NAME = L"ImageWindow";
		bool RegisterImageWindowClass(HINSTANCE hInst) {
			WNDCLASS wc = { 0 };
			wc.hInstance = hInst;
			wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
			wc.lpszClassName = CLASS_NAME;
			wc.lpfnWndProc = ImageWndProc;

			return RegisterClass(&wc);
		}
	public:
		ImageWindow() : hWnd(nullptr), bmi({ 0 }), imagePos({ 0 }) {}
		void create(HINSTANCE hInst, HWND hParent, INT X, INT Y) {
			/* Регистрация класса окна для изображений */
			if (!RegisterImageWindowClass(hInst)) {
				throw std::exception("Ошибка: не удалось зарегестрировать класс для окна изображения");
			}

			/* Создание окна для изображений */
			hWnd = CreateWindow(CLASS_NAME, L"", WS_CHILD | WS_OVERLAPPED, X, Y, INIT_WIDTH, INIT_HEIGHT, hParent, nullptr, hInst, nullptr);
			if (!hWnd) {
				throw std::exception("Ошибка: не удалось создать окно изображения");
			}

			/* Добавил контейнер для визуального отображения области, в которой могу выводится фотографии */
			CreateWindow(L"BUTTON", L"Фотография", WS_CHILD | WS_VISIBLE | BS_GROUPBOX | BS_CENTER, 0, 0, INIT_WIDTH, INIT_HEIGHT, hWnd, nullptr, hInst, nullptr);

			UpdateWindow(hWnd);
			ShowWindow(hWnd, 1);
		}

		void loadImage(ByteStream& imageContainerFile) {
			/* Загружаем и декодируем контейнер, в котором находится изображение */
			ImageContainer::Image isoImage(imageContainerFile);

			/* Получаем изображение и копируем его в память для FreeImage */
			fipMemoryIO imageMemory(isoImage.getRawImage(), isoImage.getRawImageSize());

			/* Очистим прошлое изображение */
			image.clear();

			/* Загрузим из памяти новое изображние */
			if (!image.loadFromMemory(imageMemory)) {
				throw std::exception("Ошибка: не удалось загрузить изображение");
			}

			/* Конвертируем его в 24-х битное изображение */
			if (!image.convertTo24Bits()) {
				throw std::exception("Ошибка: не удалось конвертировать изображение");
			}

			/* Заполняем заголовок изображения */
			bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bmi.bmiHeader.biWidth = image.getWidth();
			bmi.bmiHeader.biHeight = image.getHeight();
			bmi.bmiHeader.biPlanes = 1;
			bmi.bmiHeader.biCompression = BI_RGB;
			bmi.bmiHeader.biBitCount = image.getBitsPerPixel();

			/* Центрируем изображение внутри виджета */
			imagePos.x = INIT_WIDTH / 2 - image.getWidth() / 2;
			imagePos.y = INIT_HEIGHT / 2 - image.getHeight() / 2;

			/* Подаём ссылку для обработчика */
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)this);

			/* Заставляем выполнить перерисовку из-за смены изображения */
			InvalidateRect(hWnd, nullptr, TRUE);
		}

		void drawImage() {
			PAINTSTRUCT ps = { 0 };
			HDC hdc = BeginPaint(hWnd, &ps);
			FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
			SetDIBitsToDevice(hdc, imagePos.x, imagePos.y, image.getWidth(), image.getHeight(), 0, 0, 0, image.getHeight(), image.accessPixels(), &bmi, DIB_RGB_COLORS);
			EndPaint(hWnd, &ps);
		}
	};

	LRESULT CALLBACK ImageWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		ImageWindow* imgWindow = (ImageWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

		if (msg == WM_CLOSE) {
			PostQuitMessage(0);
			return 0;
		}
		if (msg == WM_PAINT && imgWindow) {
			imgWindow->drawImage();
			return 0;
		}

		return DefWindowProc(hWnd, msg, wParam, lParam);
	}

	LRESULT CALLBACK MainWindowProc(HWND, UINT, WPARAM, LPARAM);
	DWORD WINAPI CardConnectionThread(LPVOID lpParameter);

#define IDC_BUTTON_LOAD 0x1000
#define IDC_BUTTON_CONNECT 0x1001
#define IDC_EDIT_MRZ 0x1002
#define IDC_INPUT_MRZ 0x1003

	class MainWindow {
	private:
		HWND hWnd;

		ImageWindow imageWindow;
		DWORD passportConnectionThread;

		const INT WIDTH = 400;
		const INT HEIGHT = 600;
		const wchar_t* CLASS_NAME = L"MainWindow";
		bool RegisterMainWindowClass(HINSTANCE hInst) {
			WNDCLASS wc = { 0 };
			wc.lpszClassName = CLASS_NAME;
			wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
			wc.hInstance = hInst;
			wc.lpfnWndProc = MainWindowProc;

			return RegisterClass(&wc);
		}
	public:
		enum CardActions { Init, Connect, ReadDG1, ReadDG2, Disconnect, Stop };

		MainWindow(HINSTANCE hInst) : hWnd(nullptr) {
			if (!RegisterMainWindowClass(hInst)) {
				throw std::exception("Не удалось зарегистрировать класс главного окна");
			}

			HANDLE hEvent = CreateEvent(nullptr, true, false, L"ThreadStarted");
			if (!hEvent) {
				throw std::exception("Ошибка: не удалось создать hEvent");
			}

			CreateThread(nullptr, 0, CardConnectionThread, hEvent, 0, &passportConnectionThread);
			WaitForSingleObject(hEvent, INFINITE);

			hWnd = CreateWindow(CLASS_NAME, L"Главное окно", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, WIDTH, HEIGHT, nullptr, nullptr, hInst, nullptr);
			if (!hWnd) {
				throw std::exception("Ошибка: не удалось создать главное окно");
			}
			imageWindow.create(hInst, hWnd, 20, 100);

			/* Органы управления */
			CreateWindow(L"BUTTON", L"Считать", WS_CHILD | WS_VISIBLE | WS_BORDER, 0, 40, 100, 20, hWnd, (HMENU)IDC_BUTTON_LOAD, hInst, nullptr);
			CreateWindow(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 20, 60, 500, 20, hWnd, (HMENU)IDC_INPUT_MRZ, hInst, nullptr);
			CreateWindow(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 20, 500, 400, 30, hWnd, (HMENU)IDC_EDIT_MRZ, hInst, nullptr);

			SendDlgItemMessage(hWnd, IDC_EDIT_MRZ, EM_SETREADONLY, TRUE, 0);
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)this);
			PostThreadMessage(passportConnectionThread, WM_COMMAND, CardActions::Init, (LPARAM)hWnd);

			UpdateWindow(hWnd);
			ShowWindow(hWnd, 1);
		}

		void run() {
			MSG msg = { 0 };
			while (GetMessage(&msg, nullptr, 0, 0)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		LRESULT handleMessages(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
			static char mrzStrRaw[100] = { 0 };

			if (msg == WM_CLOSE) {
				PostThreadMessage(passportConnectionThread, WM_COMMAND, (WPARAM)CardActions::Disconnect, 0);
				PostQuitMessage(0);
				return 0;
			}
			if (msg == WM_COMMAND && wParam == IDC_BUTTON_LOAD) {
				/* 'A' потому что используются обычные строки */
				memset(mrzStrRaw, 0, sizeof(mrzStrRaw));
				LRESULT mrzLen = SendDlgItemMessageA(hWnd, IDC_INPUT_MRZ, WM_GETTEXT, sizeof(mrzStrRaw), (LPARAM)mrzStrRaw);
				if (mrzLen < 44) {
					MessageBox(hWnd, L"Вы ввели недостаточно символов.\nПожалуйста, проверьте ваш ввод", L"Ошибка", MB_OK | MB_ICONERROR);
					return 0;
				}

				PostThreadMessage(passportConnectionThread, WM_COMMAND, (WPARAM)CardActions::Connect, (LPARAM)mrzStrRaw);
				PostThreadMessage(passportConnectionThread, WM_COMMAND, (WPARAM)CardActions::ReadDG1, 0);
				PostThreadMessage(passportConnectionThread, WM_COMMAND, (WPARAM)CardActions::ReadDG2, 0);
				PostThreadMessage(passportConnectionThread, WM_COMMAND, (WPARAM)CardActions::Disconnect, 0);
				return 0;
			}

			if (msg == WM_COMMAND && wParam == CardActions::ReadDG1) {
				/* 'A' в конце потому что строка, которая передаётся через lParam - ASCII */
				SendDlgItemMessageA(hWnd, IDC_EDIT_MRZ, WM_SETTEXT, 0, (LPARAM)lParam);
				UpdateWindow(hWnd);
			}

			if (msg == WM_COMMAND && wParam == CardActions::ReadDG2) {
				ByteStream* imgContainerFile = (ByteStream*)lParam;
				imageWindow.loadImage(*imgContainerFile);
				imgContainerFile->close();
			}

			return DefWindowProc(hWnd, msg, wParam, lParam);
		}
	};

	LRESULT CALLBACK MainWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		MainWindow* mainWindow = (MainWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		return (mainWindow ? mainWindow->handleMessages(hWnd, msg, wParam, lParam) : DefWindowProc(hWnd, msg, wParam, lParam));
	}

	DWORD WINAPI CardConnectionThread(LPVOID lpParameter) {
		Passport passport;
		CardReader cardReader;

		ByteStream imgContainerFile;
		basic_string<UINT8> mrzCodeStr;

		auto& readers = cardReader.getReadersList();
		wstring readerName = readers[0];

		MSG msg = { 0 };
		PeekMessage(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
		SetEvent(lpParameter);

		HWND hWnd = nullptr;
		while (GetMessage(&msg, nullptr, 0, 0)) {
			if (msg.message == WM_COMMAND) {
				try {
					if (msg.wParam == MainWindow::CardActions::Init) {
						hWnd = (HWND)msg.lParam;
					}
					if (msg.wParam == MainWindow::CardActions::Connect) {
						passport.connectPassport(cardReader, readerName);

						char* mrzLine2Raw = (char*)msg.lParam;
						string mrzLine2 = mrzLine2Raw;
						passport.perfomBAC(cardReader, mrzLine2);
					}
					else if (msg.wParam == MainWindow::CardActions::ReadDG1) {
						HWND hProgressBar = CreateWindow(PROGRESS_CLASS, L"", WS_CHILD | WS_VISIBLE, 20, 550, 300, 30, hWnd, nullptr, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), nullptr);
						passport.readDG(cardReader, Passport::DGFILES::DG1, hProgressBar);
						ByteVec mrzCode = passport.getPassportMRZ();
						mrzCodeStr = basic_string<UINT8>(mrzCode.begin(), mrzCode.end());

						SendMessage(hWnd, WM_COMMAND, MainWindow::CardActions::ReadDG1, (LPARAM)mrzCodeStr.c_str());
					}
					else if (msg.wParam == MainWindow::CardActions::ReadDG2) {
						HWND hProgressBar = CreateWindow(PROGRESS_CLASS, L"", WS_CHILD | WS_VISIBLE, 20, 550, 300, 30, hWnd, nullptr, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), nullptr);
						passport.readDG(cardReader, Passport::DGFILES::DG2, hProgressBar);

						ByteVec rawImage = passport.getPassportRawImage();

						imgContainerFile.open("imgContainer.bin", ios::in | ios::out | ios::binary | ios::trunc);
						imgContainerFile.write(rawImage.data(), rawImage.size());
						imgContainerFile.flush();
						imgContainerFile.seekg(0);

						SendMessage(hWnd, WM_COMMAND, MainWindow::CardActions::ReadDG2, (LPARAM)&imgContainerFile);
					}
					else if (msg.wParam == MainWindow::CardActions::Disconnect) {
						passport.disconnectPassport(cardReader);
					}
				}
				catch (std::exception& e) {
					passport.disconnectPassport(cardReader);
					MessageBoxA(hWnd, e.what(), "Ошибка", MB_OK | MB_ICONERROR);
				}
			}
		}

		return 0;
	}
}