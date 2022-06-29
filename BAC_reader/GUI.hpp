#pragma once

#include <Windows.h>
#include <windowsx.h>

#include "ImageContainer.hpp"
#include "Passport.hpp"
#include "CardReader.hpp"
#include "Util.hpp"

#include <fstream>
using namespace std;

namespace GUI {
	/* �������� �������-����������� ��� ���� ��������� ����������� */
	LRESULT CALLBACK ImageWndProc(HWND, UINT, WPARAM, LPARAM);

	/* ����� ���� ��������� ����������� */
	/* � �������� ����� "��������", "�� ��������" */
	/* ������������ ��������� �� ���� ���� ��������� */
	/* � ����������� ���������. ���� ��������� ���� - */
	/* ������ ����� �������� ����������� */
	/* ����� - ����������� �������� ������ */
	class ImageWindow {
		/* ���������� ���� */
		HWND hWnd;

		/* ����� ��� ��������� ������� ���� */
		HBRUSH hBgBrush;

		/* �����, ������� ����� ������������ ����������� � �������� �� RGB ������ */
		fipImage image;

		/* ��������� ������ �������� ���� ��� ������������ �� ������ */
		POINT imagePos;

		/* ������� ���� ��� ���������  */
		const INT INIT_WIDTH = 300;
		const INT INIT_HEIGHT = 410;

		/* ����� ���� ��������� ����������� */
		const wchar_t* CLASS_NAME = L"ImageWindow";
		
		/* ������� ����������� ������ ���� ��������� - ��� �� ���� �� ������� */
		bool RegisterImageWindowClass(HINSTANCE hInst) {
			hBgBrush = CreateSolidBrush(RGB(240, 240, 240));

			WNDCLASS wc = { 0 };
			wc.hInstance = hInst;
			wc.hbrBackground = (HBRUSH)hBgBrush;
			wc.lpszClassName = CLASS_NAME;
			wc.lpfnWndProc = ImageWndProc;

			return RegisterClass(&wc);
		}
	public:
		/* ����������� �� ��������� ��������� ��� ���������� ������������� ������ ������ */
		ImageWindow() : hWnd(nullptr), imagePos({ 0 }), hBgBrush(nullptr) {}

		/* ������� �������� ���� ��������� */
		bool create(HINSTANCE hInst, HWND hParent, INT X, INT Y) {
			/* ����������� ������ ���� ��� ����������� */
			if (!RegisterImageWindowClass(hInst)) {
				return false;
			}

			/* �������� ���� ��� ����������� */
			hWnd = CreateWindow(CLASS_NAME, L"", WS_CHILD | WS_OVERLAPPED, X, Y, INIT_WIDTH, INIT_HEIGHT, hParent, nullptr, hInst, nullptr);
			if (!hWnd) {
				return false;
			}

			/* ������� ��������� ��� ����������� ����������� �������, � ������� ���� ��������� ���������� */
			if (!CreateWindow(L"BUTTON", L"����������", WS_CHILD | WS_VISIBLE | BS_GROUPBOX | BS_CENTER, 0, 0, INIT_WIDTH, INIT_HEIGHT, hWnd, nullptr, hInst, nullptr)) {
				return false;
			}

			/* ��������� � ������������� ���� */
			UpdateWindow(hWnd);
			ShowWindow(hWnd, 1);

			return true;
		}

		/* ������� �������� ����������� �� ����� ���������� ����������� */
		void loadImage(ByteStream& imageContainerFile) {
			/* ��������� � ���������� ���������, � ������� ��������� ����������� */
			ImageContainer::Image isoImage(imageContainerFile);

			/* �������� ����������� � �������� ��� � ������ ��� FreeImage */
			fipMemoryIO imageMemory(isoImage.getRawImage(), isoImage.getRawImageSize());

			/* ������� ������� ����������� */
			image.clear();

			/* �������� �� ������ ����� ���������� */
			if (!image.loadFromMemory(imageMemory)) {
				throw std::exception("������: �� ������� ��������� �����������");
			}

			/* ������������ ��� � 24-� ������ ����������� */
			if (!image.convertTo24Bits()) {
				throw std::exception("������: �� ������� �������������� �����������");
			}

			/* �������� ����������� �� ����� ��� �� ������� ���� �� 10 �������� ������ ����������� ���� */
			image.crop(0, 0, INIT_WIDTH - 20, INIT_HEIGHT - 25);

			/* ���������� ����������� ������ ������� */
			imagePos.x = INIT_WIDTH / 2 - image.getWidth() / 2;
			imagePos.y = INIT_HEIGHT / 2 - image.getHeight() / 2 + 5;

			/* ��������� ����� �����������, ����� ��������� ��� ����������� */
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)this);

			/* ���������� ��������� ����������� ��-�� ����� ����������� */
			InvalidateRect(hWnd, nullptr, TRUE);
		}

		/* ������� ����������� � ���� ����������� */
		void drawImage() {
			PAINTSTRUCT ps = { 0 };
			HDC hdc = BeginPaint(hWnd, &ps);
			FillRect(hdc, &ps.rcPaint, hBgBrush);

			/* ��������������� ����������� �������� � 24-� ������ ������� �� ������ ���� */
			SetDIBitsToDevice(hdc, imagePos.x, imagePos.y, image.getWidth(), image.getHeight(), 0, 0, 0, image.getHeight(), image.accessPixels(), (BITMAPINFO*)image.getInfoHeader(), DIB_RGB_COLORS);
			EndPaint(hWnd, &ps);
		}

		/* ������� ������� ����������� */
		void clearImage() {
			/* �������� ����������� */
			image.clear();

			/* ��������� �����������, ����� ������� ��������� */
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)nullptr);

			/* ���������� ������ ���������� ���� */
			InvalidateRect(hWnd, nullptr, TRUE);
		}
	};

	/* ���������� ������� ����������� */
	LRESULT CALLBACK ImageWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		/* �������� ��������� �� ���� ��������� */
		ImageWindow* imgWindow = (ImageWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

		if (msg == WM_CLOSE) {
			PostQuitMessage(0);
			return 0;
		}

		/* ���� ������� ��������� � ��� ���� ������� ��������� - ������ */
		if (msg == WM_PAINT && imgWindow) {
			imgWindow->drawImage();
			return 0;
		}

		/* � ��������� ��������� ��������� ������������ ������� ���� �� ��������� */
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	
	/* �������������� ������ � ����� ����� */
	#define IDC_BUTTON_LOAD 0x1000
	#define IDC_INPUT_MRZ 0x1001

	#define IDC_PASSPORT_TYPE 0x1002
	#define IDC_COUNTRY_CODE  0x1003
	#define IDC_PASSPORT_NAME 0x1004
	#define IDC_PASSPORT_FAM 0x1005
	#define IDC_PASSPORT_NUM 0x1006
	#define IDC_PASSPORT_NAT 0x1007
	#define IDC_PASSPORT_BIRTH 0x1008
	#define IDC_PASSPORT_SEX 0x1009
	#define IDC_PASSPORT_EXPIRES 0x100A

	#define IDC_BUTTON_ABOUT 0x100B
	#define IDC_BUTTON_EXIT 0x100C

	/* �������-���������� ��� �������� ���� */
	LRESULT CALLBACK MainWindowProc(HWND, UINT, WPARAM, LPARAM);

	/* �������. ������� ����������� � ��������� ������ */
	/* ��������� ������ ���������� � ��������� */
	DWORD WINAPI CardConnectionThread(LPVOID lpParameter);

	/* ����� �������� ���� */
	class MainWindow {
	private:
		/* ���������� ���������� ���� */
		HWND hWnd;

		/* ���������� ��� GroupBox ������ �������� */
		HWND hInfoBox;

		/* ���������� ��� ComboBox �� ��� ��������� ����������� */
		HWND hReadersList;

		/* ���������� ��� ���������� ��������� */
		HWND hProgressBar;

		/* ���� ��������� ����������� */
		ImageWindow imageWindow;

		/* ������������� ������ ��������� � ��������� */
		DWORD passportConnectionThread;

		/* ������� �������� ���� */
		const INT WIDTH = 920;
		const INT HEIGHT = 515;

		/* �������� ������ ���� */
		const wchar_t* CLASS_NAME = L"MainWindow";

		/* ������ "� ���������" */
		const wchar_t* ABOUT_STR = L"��������� �������� � ������ ���������������� ��������\n����������: ������� �.�.\n������� ������������: ����-������� �.�.";

		/* ������� ����������� ������ ���� */
		bool RegisterMainWindowClass(HINSTANCE hInst) {
			WNDCLASS wc = { 0 };
			wc.lpszClassName = CLASS_NAME;
			wc.hbrBackground = (HBRUSH)CreateSolidBrush(RGB(240, 240, 240));
			wc.hInstance = hInst;
			wc.lpfnWndProc = MainWindowProc;

			return RegisterClass(&wc);
		}
	public:
		/* ��������� ��� �������������� � ������� ������ �������� */
		enum ThreadActions { SendMainHWND, SendProgHWND, GetReaders, SetReader, Connect, ReadDG1, ReadDG2, Disconnect, Stop };
		enum ThreadResponces { ReadDG1End, ReadDG2End };

		MainWindow(HINSTANCE hInst) : hWnd(nullptr) {
			/* ����������� ������ ���� ��� WinaAPI */
			if (!RegisterMainWindowClass(hInst)) {
				throw std::exception("�� ������� ���������������� ����� �������� ����");
			}

			/* �������� �������, ������� ������ ����������� �������� ������ ���� */
			HANDLE hEvent = CreateEvent(nullptr, true, false, L"ThreadStarted");
			if (!hEvent) {
				throw std::exception("������: �� ������� ������� hEvent");
			}

			/* �������� �����, ������� ����� �������� ������ �� �������� */
			CreateThread(nullptr, 0, CardConnectionThread, hEvent, 0, &passportConnectionThread);

			/* ������� ������� �������� ������ ��������� � ��������� */
			WaitForSingleObject(hEvent, INFINITE);

			/* ������ ������� ���� */
			hWnd = CreateWindow(CLASS_NAME, L"������� ����", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, WIDTH, HEIGHT, nullptr, nullptr, hInst, nullptr);
			if (!hWnd) {
				throw std::exception("������: �� ������� ������� ������� ����");
			}
			
			/* ������ ���� ��������� */
			if (!imageWindow.create(hInst, hWnd, 20, 20)) {
				throw std::exception("������: �� ������� ������� ���� ���������");
			}

			/* ������ ����������; ��������� ��������� ������ �� ��������� ���� */
			DWORD textStyle = WS_CHILD | WS_VISIBLE | ES_RIGHT;
			DWORD editStyle = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER | ES_READONLY;

			/* ��������� ������ ����� ��������� ����� ���� ����: ������ ������ �����: ����-����� ���������� ����� */
			/* ���������� ������������� ����� � hInst */
			#define CREATE_EDIT_FIELD(label, x, y, w, wnd, command) \
					CreateWindow(L"STATIC", label, textStyle, x, y, w, 20, wnd, nullptr, hInst, nullptr);\
					CreateWindow(L"EDIT", L"", editStyle, x + w + 10, y, w, 20, wnd, (HMENU)command, hInst, nullptr)\

			/* �������� ������ ������ ��� ����������� ���������� � �������� */
			hInfoBox = CreateWindow(L"BUTTON", L"����������", WS_CHILD | WS_VISIBLE | BS_GROUPBOX | BS_CENTER, 330, 20, 560, 220, hWnd, nullptr, hInst, nullptr);
			CREATE_EDIT_FIELD(L"��� ��������:", 20, 30, 120, hInfoBox, IDC_PASSPORT_TYPE);
			CREATE_EDIT_FIELD(L"��� ���-��:", 290, 30, 120, hInfoBox, IDC_COUNTRY_CODE);

			CREATE_EDIT_FIELD(L"����� ��������:", 20, 60, 120, hInfoBox, IDC_PASSPORT_NUM);

			CREATE_EDIT_FIELD(L"�������:", 20, 100, 120, hInfoBox, IDC_PASSPORT_FAM);
			CREATE_EDIT_FIELD(L"���:", 290, 100, 120, hInfoBox, IDC_PASSPORT_NAME);

			CREATE_EDIT_FIELD(L"�����������:", 20, 140, 120, hInfoBox, IDC_PASSPORT_NAT);
			CREATE_EDIT_FIELD(L"���:", 290, 140, 120, hInfoBox, IDC_PASSPORT_SEX);

			CREATE_EDIT_FIELD(L"���� ��������:", 20, 180, 120, hInfoBox, IDC_PASSPORT_BIRTH);
			CREATE_EDIT_FIELD(L"���� ���������:", 290, 180, 120, hInfoBox, IDC_PASSPORT_EXPIRES);

			/* ���������� ComboBox ��� ������ ���������� */
			CreateWindow(L"BUTTON", L"�������� ���������", WS_CHILD | WS_VISIBLE | BS_GROUPBOX | BS_CENTER, 330, 250, 560, 60, hWnd, nullptr, hInst, nullptr);
			hReadersList = CreateWindow(WC_COMBOBOX, L"", CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_CHILD | WS_VISIBLE, 350, 270, 520, 200, hWnd, nullptr, hInst, nullptr);

			/* ���� ����� ������ MRZ ������ */
			CreateWindow(L"BUTTON", L"������� ������ ������ MRZ", WS_CHILD | WS_VISIBLE | BS_GROUPBOX | BS_CENTER, 330, 310, 560, 60, hWnd, nullptr, hInst, nullptr);
			CreateWindow(L"EDIT", L"", editStyle & ~ES_READONLY, 350, 330, 520, 20, hWnd, (HMENU)IDC_INPUT_MRZ, hInst, nullptr);

			/* ���� ������ ��������� */
			CreateWindow(L"BUTTON", L"�������� ��������", WS_CHILD | WS_VISIBLE | BS_GROUPBOX | BS_CENTER, 330, 370, 560, 60, hWnd, nullptr, hInst, nullptr);
			CreateWindow(L"BUTTON", L"�������", WS_CHILD | WS_VISIBLE | WS_BORDER, 350, 395, 160, 20, hWnd, (HMENU)IDC_BUTTON_LOAD, hInst, nullptr);
			CreateWindow(L"BUTTON", L"� ���������", WS_CHILD | WS_VISIBLE | WS_BORDER, 530, 395, 160, 20, hWnd, (HMENU)IDC_BUTTON_ABOUT, hInst, nullptr);
			CreateWindow(L"BUTTON", L"�����", WS_CHILD | WS_VISIBLE | WS_BORDER, 710, 395, 160, 20, hWnd, (HMENU)IDC_BUTTON_EXIT, hInst, nullptr);

			/* ������� ���� ����������� ��������� */
			hProgressBar = CreateWindow(PROGRESS_CLASS, L"", WS_CHILD | WS_VISIBLE, 20, 440, 870, 20, hWnd, nullptr, hInst, nullptr);

			/* ��������� � ���������� ���� ��������� �� ������� ����� */
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)this);

			/* ���������� � ����� ������� � ��������� ������� ���� � ���� ���������� �������� */
			PostThreadMessage(passportConnectionThread, WM_COMMAND, ThreadActions::SendMainHWND, (LPARAM)hWnd);
			PostThreadMessage(passportConnectionThread, WM_COMMAND, ThreadActions::SendProgHWND, (LPARAM)hProgressBar);
			PostThreadMessage(passportConnectionThread, WM_COMMAND, ThreadActions::GetReaders, (LPARAM)hReadersList);

			/* ��������� ���� � ������ ��� ������� */
			UpdateWindow(hWnd);
			ShowWindow(hWnd, 1);
		}

		/* �������� ���� ��������� */
		void run() {
			MSG msg = { 0 };
			while (GetMessage(&msg, nullptr, 0, 0)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		/* ������� ��������� ��������� ����������� ���� - �������������� ����������� �������-���������� */
		LRESULT handleMessages(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
			/* �������� ������ �� 100 �������� ��� �������� ��������� MRZ */
			static char mrzStrRaw[100] = { 0 };

			if (msg == WM_COMMAND && wParam == IDC_BUTTON_ABOUT) {
				MessageBox(hWnd, ABOUT_STR, L"� ���������", MB_OK | MB_ICONASTERISK);
				return 0;
			}

			/* ���� ������ ������� "�������" ��� ������ ������ "�����" */
			if (msg == WM_CLOSE || (msg == WM_COMMAND && wParam == IDC_BUTTON_EXIT)) {
				/* ������� ��������� � ����� ������� �������� */
				PostThreadMessage(passportConnectionThread, WM_COMMAND, (WPARAM)ThreadActions::Disconnect, 0);

				/* ����� ��� ����������� ������ */
				PostQuitMessage(0);
				return 0;
			}

			/* ���� ���� ������ ������ "�������" */
			if (msg == WM_COMMAND && wParam == IDC_BUTTON_LOAD) {
				/* ��������� MRZ ��� �� ������ ����� */
				/* �� ������� - ������� ���������� MRZ */
				memset(mrzStrRaw, 0, sizeof(mrzStrRaw));

				/* ����� ��������� ������ ������� ������ */
				/* 'A' ������ ��� ������������ ������� ������ */
				LRESULT mrzLen = GetDlgItemTextA(hWnd, IDC_INPUT_MRZ, mrzStrRaw, sizeof(mrzStrRaw));

				/* �������� ��������� */
				PostThreadMessage(passportConnectionThread, WM_COMMAND, (WPARAM)ThreadActions::SetReader, (LPARAM)hReadersList);

				/* ���������� ������� ������ ������ �� ���������� � ������ */
				PostThreadMessage(passportConnectionThread, WM_COMMAND, (WPARAM)ThreadActions::Connect, (LPARAM)mrzStrRaw);

				/* �� ������ ������ ������ ������ - MRZ */
				PostThreadMessage(passportConnectionThread, WM_COMMAND, (WPARAM)ThreadActions::ReadDG1, (LPARAM)mrzStrRaw);

				/* �� ������ ������ ������ ������ - ���� */
				PostThreadMessage(passportConnectionThread, WM_COMMAND, (WPARAM)ThreadActions::ReadDG2, 0);

				/* �� ���������� �������� */
				PostThreadMessage(passportConnectionThread, WM_COMMAND, (WPARAM)ThreadActions::Disconnect, 0);
				
				/* �������� ������� ����������� */
				imageWindow.clearImage();

				/* �������� ��� ���� ���������� */
				for (UINT16 i = IDC_PASSPORT_TYPE; i <= IDC_PASSPORT_EXPIRES; i += 1) {
					SetDlgItemTextA(hInfoBox, i, "");
				}

				return 0;
			}

			/* ��� ������� ����� �������� ����� ��������� � ��������� */
			/* �� ������ ��������, ��� ��������������� ������� ���� ��������� */
			/* ��� ������� ��������, ��� ���������� DG1 �����������*/
			if (msg == WM_COMMAND && wParam == ThreadResponces::ReadDG1End) {
				string mrzLine = mrzStrRaw;
				FullMRZDecoder mrzDecoder(mrzLine);

				/* 'A' � ����� ������ ��� ������ � ASCII */
				for (INT i = 0; i < mrzDecoder.infoVec.size(); i += 1) {
					SetDlgItemTextA(hInfoBox, IDC_PASSPORT_TYPE + i, mrzDecoder.infoVec[i].c_str());
				}
				
				/* ������ ������� �� ���� � ���������� ��������� */
				SendMessage(hProgressBar, PBM_SETPOS, 0, 0);

				/* ��������� ���� */
				UpdateWindow(hWnd);
			}

			/* ��� ������� ��������, ��� ������� ��������� � �������� ��������� ���������� DG2 */
			if (msg == WM_COMMAND && wParam == ThreadResponces::ReadDG2End) {
				/* � �������� ��������� ��� ������� ���������� ��������� ����� */
				/* � ������� ���������� ��������� � ������������ */
				ByteStream* imgContainerFile = (ByteStream*)lParam;

				/* ��������� ����������� �� ����� ���������� */
				imageWindow.loadImage(*imgContainerFile);

				/* � ��������� ��������� */
				imgContainerFile->close();

				/* ������ ������� �� ���� � ���������� ��������� */
				SendMessage(hProgressBar, PBM_SETPOS, 0, 0);

				/* ��������� ���� */
				UpdateWindow(hWnd);
			}

			return DefWindowProc(hWnd, msg, wParam, lParam);
		}
	};

	/* �������-�������� ��� ��������� ��������� */
	/* ���� ���� ��������� �� ���� - ��������� ����� ��� */
	/* ���� ��������� ��� - ��������� ������� �� ��������� */
	LRESULT CALLBACK MainWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		MainWindow* mainWindow = (MainWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		return (mainWindow ? mainWindow->handleMessages(hWnd, msg, wParam, lParam) : DefWindowProc(hWnd, msg, wParam, lParam));
	}

	/* �������, ����������� ���������� � ������� � ��������� */
	DWORD WINAPI CardConnectionThread(LPVOID lpParameter) {
		/* �������� � ���� ������ ��������, ����-������ */
		Passport passport;
		CardReader cardReader;

		/* ����, ������� ����� ��������� ��������� � ������������ */
		ByteStream imgContainerFile;

		/* ���������� �������� ���� (���� ���������� ���������) */
		HWND hWnd = nullptr;
		
		/* � ���������� ���� ��������� */
		HWND hProgressBar = nullptr;

		/* ��� ���������� */
		wstring readerName = L"";

		/* ������ ������� ��������� */
		MSG msg = { 0 };
		PeekMessage(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

		/* �������������, ��� ������� �������� ������ ��������� */
		SetEvent(lpParameter);

		/* ���� ��������� ��������� ���� */
		while (GetMessage(&msg, nullptr, 0, 0)) {
			/* ���� ������ ������� */
			if (msg.message == WM_COMMAND) {
				/* ����� ��� �������, � ������ �������� ������, ����������� ���������� */
				try {
					/* ���� ������ ��������� ��������� �������� ���� */
					if (msg.wParam == MainWindow::ThreadActions::SendMainHWND) {
						hWnd = (HWND)msg.lParam;
					}
					/* ���� ������ ��������� ��������� ���� ��������� */
					else if (msg.wParam == MainWindow::ThreadActions::SendProgHWND) {
						hProgressBar = (HWND)msg.lParam;
					}
					else if (msg.wParam == MainWindow::ThreadActions::GetReaders) {
						/* �������� ������ ��������� ������� ����������� */
						vector<wstring> readersList = cardReader.getReadersList();

						/* ��������� ������ ������ ���������� ���� ������� */
						for (const auto& reader : readersList) {
							SendMessage((HWND)msg.lParam, CB_ADDSTRING, 0, (LPARAM)reader.c_str());
						}

						/* ������������� ������ ����� �� ��������� */
						SendMessage((HWND)msg.lParam, CB_SETCURSEL, 0, 0);
					}
					else if (msg.wParam == MainWindow::ThreadActions::SetReader) {
						/* ������ ������ ���������� ������; ���������� � ������ � ���� ��������� */
						DWORD readerIndex = SendMessage((HWND)msg.lParam, CB_GETCURSEL, 0, 0);

						/* ����� ������ ��� �������� (� ��������) */
						DWORD readerNameSize = SendMessage((HWND)msg.lParam, CB_GETLBTEXTLEN, readerIndex, 0);

						/* ��������� ������ */
						readerName.resize(readerNameSize);

						/* �������� �������� ������ � ������ */
						SendMessage((HWND)msg.lParam, CB_GETLBTEXT, 0, (LPARAM)readerName.data());
					}
					/* ���� ������ ��������� �� ���������� � ��������� */
					else if (msg.wParam == MainWindow::ThreadActions::Connect) {
						/* ������� ������������ � ����� */
						passport.connectPassport(cardReader, readerName);

						/* �������� �� ����������� ��������� ��������� �� ������ */
						/* � ������� ��������� ������ MRZ ������ */
						char* mrzLine2Raw = (char*)msg.lParam;
						string mrzLine2 = mrzLine2Raw;

						/* ��������� �������������� �� BAC */
						passport.perfomBAC(cardReader, mrzLine2);
					}
					/* ���� ������ ��������� � ������ ������ ������ ������ */
					else if (msg.wParam == MainWindow::ThreadActions::ReadDG1) {
						/* ������ ������ ������ ������ */
						passport.readDG(cardReader, Passport::DGFILES::DG1, hProgressBar);

						/* �������� ������, ���� ���� �������� MRZ ��� */
						char* mrzStrRaw = (char*)msg.lParam;

						/* �������� ��� MRZ ��� */
						ByteVec mrzCode = passport.getPassportMRZ();

						/* �������� MRZ �� ������� � ������ */
						memcpy(mrzStrRaw, mrzCode.data(), mrzCode.size());

						/* ���������� ��������� � ���, ��� �� ������������ ��������� */
						if (!hWnd) { throw std::exception("�� ��� ������� hWnd ��� ������ ��������"); }
						SendMessage(hWnd, WM_COMMAND, MainWindow::ThreadResponces::ReadDG1End, 0);
					}
					/* ���� ������ ��������� � ���, ��� ���� ������� ������ ������ ������ */
					else if (msg.wParam == MainWindow::ThreadActions::ReadDG2) {
						/* ��������� ������ ������ ������ */
						passport.readDG(cardReader, Passport::DGFILES::DG2, hProgressBar);

						/* �������� ������ ���� 5F2E ��� 7F2E */
						ByteVec rawImage = passport.getPassportRawImage();

						/* ���������� ������ ����� ���� ���� ��������� ����������� */
					#ifdef _DEBUG
						imgContainerFile.open("imgContainer.bin", ios::in | ios::out | ios::binary | ios::trunc);
					#else
						/* ������ ��������� ����, ���� ������� ��������� */
						imgContainerFile = ByteStream(tmpfile());
					#endif
						imgContainerFile.write(rawImage.data(), rawImage.size());
						imgContainerFile.flush();
						imgContainerFile.seekg(0);

						/* � ���������� ��������� � ���, ��� �� ���������, ���� ���������� ��������� ����� */
						if (!hWnd) { throw std::exception("�� ��� ������� hWnd ��� ������ ��������"); }
						SendMessage(hWnd, WM_COMMAND, MainWindow::ThreadResponces::ReadDG2End, (LPARAM)&imgContainerFile);
					}
					/* ���� ������ ��������� �� ���������� �������� */
					else if (msg.wParam == MainWindow::ThreadActions::Disconnect) {
						/* ����������, ��������� */
						passport.disconnectPassport(cardReader);
					}
				}
				catch (std::exception& e) {
					/* � ������ �����-���� ������������ - ��������� ������� � ������ ����� ������ */
					passport.disconnectPassport(cardReader);
					MessageBoxA(hWnd, e.what(), "������", MB_OK | MB_ICONERROR);
				}
			}
		}

		return 0;
	}
}