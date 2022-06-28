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
	/* Прототип функции-обработчика для окна просмотра изображения */
	LRESULT CALLBACK ImageWndProc(HWND, UINT, WPARAM, LPARAM);

	/* Класс окна просмотра изображения */
	/* В качестве флага "показать", "не показать" */
	/* Используется указатель на само окно просмотра */
	/* В обработчике сообщений. Если указатель есть - */
	/* Значит можем показать изображение */
	/* Иначе - изображение показать нельзя */
	class ImageWindow {
		/* Дескриптор окна */
		HWND hWnd;

		/* Класс, который умеет декодировать изображения и выводить их RGB данные */
		fipImage image;

		/* Положение левого верхнего угла для выравнивания по центру */
		POINT imagePos;

		/* Размеры окна для просмотра  */
		const INT INIT_WIDTH = 300;
		const INT INIT_HEIGHT = 400;

		/* Класс окна просмотра изображения */
		const wchar_t* CLASS_NAME = L"ImageWindow";
		
		/* Функция регистрации класса окна просмотра - без неё окно не создать */
		bool RegisterImageWindowClass(HINSTANCE hInst) {
			WNDCLASS wc = { 0 };
			wc.hInstance = hInst;
			wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
			wc.lpszClassName = CLASS_NAME;
			wc.lpfnWndProc = ImageWndProc;

			return RegisterClass(&wc);
		}
	public:
		/* Конструктор по умолчанию необходим для правильной инициализации внутри класса */
		ImageWindow() : hWnd(nullptr), imagePos({ 0 }) {}

		/* Функция создания окна просмотра */
		bool create(HINSTANCE hInst, HWND hParent, INT X, INT Y) {
			/* Регистрация класса окна для изображений */
			if (!RegisterImageWindowClass(hInst)) {
				return false;
			}

			/* Создание окна для изображений */
			hWnd = CreateWindow(CLASS_NAME, L"", WS_CHILD | WS_OVERLAPPED, X, Y, INIT_WIDTH, INIT_HEIGHT, hParent, nullptr, hInst, nullptr);
			if (!hWnd) {
				return false;
			}

			/* Добавил контейнер для визуального отображения области, в которой могу выводится фотографии */
			if (!CreateWindow(L"BUTTON", L"Фотография", WS_CHILD | WS_VISIBLE | BS_GROUPBOX | BS_CENTER, 0, 0, INIT_WIDTH, INIT_HEIGHT, hWnd, nullptr, hInst, nullptr)) {
				return false;
			}

			/* Обновляем и демонстрируем окно */
			UpdateWindow(hWnd);
			ShowWindow(hWnd, 1);

			return true;
		}

		/* Функция загрузки изображения из файла контейнера изображения */
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

			/* Центрируем изображение внутри виджета */
			imagePos.x = INIT_WIDTH / 2 - image.getWidth() / 2;
			imagePos.y = INIT_HEIGHT / 2 - image.getHeight() / 2;

			/* Если изображение превышает размера окна - обрезать */

			/* Разрешаем показ изображения, подав указатель для обработчика */
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)this);

			/* Заставляем выполнить перерисовку из-за смены изображения */
			InvalidateRect(hWnd, nullptr, TRUE);
		}

		/* Функция отображения в окне изображения */
		void drawImage() {
			PAINTSTRUCT ps = { 0 };
			HDC hdc = BeginPaint(hWnd, &ps);
			FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));

			/* Непосредственно отображение пикселей в 24-х битном формате на холсте окна */
			SetDIBitsToDevice(hdc, imagePos.x, imagePos.y, image.getWidth(), image.getHeight(), 0, 0, 0, image.getHeight(), image.accessPixels(), (BITMAPINFO*)image.getInfoHeader(), DIB_RGB_COLORS);
			EndPaint(hWnd, &ps);
		}

		/* Функция очистки изображения */
		void clearImage() {
			/* Очистить изображение */
			image.clear();

			/* Запрещаем отображение, подав нулевой указатель */
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)nullptr);

			/* Заставляем заново отрисовать окно */
			InvalidateRect(hWnd, nullptr, TRUE);
		}
	};

	/* Реализация функции обработчика */
	LRESULT CALLBACK ImageWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		/* Получаем указатель на окно просмотра */
		ImageWindow* imgWindow = (ImageWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

		if (msg == WM_CLOSE) {
			PostQuitMessage(0);
			return 0;
		}

		/* Если получен указатель и при этом команда рисования - рисуем */
		if (msg == WM_PAINT && imgWindow) {
			imgWindow->drawImage();
			return 0;
		}

		/* В остальных ситуациях принимает командование функция окна по умолчанию */
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	
	/* Идентификаторы кнопок и полей ввода */
	#define IDC_BUTTON_LOAD 0x1000
	#define IDC_BUTTON_CLEAR 0x1001
	#define IDC_EDIT_MRZ 0x1002
	#define IDC_INPUT_MRZ 0x1003

	#define IDC_PASSPORT_TYPE 0x1004
	#define IDC_COUNTRY_CODE  0x1005
	#define IDC_PASSPORT_NAME 0x1006
	#define IDC_PASSPORT_FAM 0x1007
	#define IDC_PASSPORT_NUM 0x1008
	#define IDC_PASSPORT_NAT 0x1009
	#define IDC_PASSPORT_BIRTH 0x100A
	#define IDC_PASSPORT_SEX 0x100B
	#define IDC_PASSPORT_EXPIRES 0x100C

	/* Функция-обработчик для главного окна */
	LRESULT CALLBACK MainWindowProc(HWND, UINT, WPARAM, LPARAM);

	/* Функция. которая запускается в отдельном потоке */
	/* Исполняет задачи соединения с паспортом */
	DWORD WINAPI CardConnectionThread(LPVOID lpParameter);

	/* Класс главного окна */
	class MainWindow {
	private:
		/* Дескриптор созданного окна */
		HWND hWnd;

		/* Дескриптор для индикатора прогресса */
		HWND hProgressBar;

		/* Окно просмотра изображения */
		ImageWindow imageWindow;

		/* Идентификатор потока сообщения с паспортом */
		DWORD passportConnectionThread;

		/* Размеры главного окна */
		const INT WIDTH = 745;
		const INT HEIGHT = 505;

		/* Название класса окна */
		const wchar_t* CLASS_NAME = L"MainWindow";

		/* Функция регистрации класса окна */
		bool RegisterMainWindowClass(HINSTANCE hInst) {
			WNDCLASS wc = { 0 };
			wc.lpszClassName = CLASS_NAME;
			wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
			wc.hInstance = hInst;
			wc.lpfnWndProc = MainWindowProc;

			return RegisterClass(&wc);
		}
	public:
		/* Сообщения для взаимодействия с потоком чтения паспорта */
		enum ThreadActions { SendMainHWND, SendProgHWND, Connect, ReadDG1, ReadDG2, Disconnect, Stop };
		enum ThreadResponces { ReadDG1End, ReadDG2End };

		MainWindow(HINSTANCE hInst) : hWnd(nullptr) {
			/* Регистрация класса окна для WinaAPI */
			if (!RegisterMainWindowClass(hInst)) {
				throw std::exception("Не удалось зарегистрировать класс главного окна");
			}

			/* Создаётся событие, которое должно выполниться создании потока ниже */
			HANDLE hEvent = CreateEvent(nullptr, true, false, L"ThreadStarted");
			if (!hEvent) {
				throw std::exception("Ошибка: не удалось создать hEvent");
			}

			/* Создаётся поток, который будет получать данные от паспорта */
			CreateThread(nullptr, 0, CardConnectionThread, hEvent, 0, &passportConnectionThread);

			/* Ожадаем событие создания потока сообщения с паспортом */
			WaitForSingleObject(hEvent, INFINITE);

			/* Создаём главное окно */
			hWnd = CreateWindow(CLASS_NAME, L"Главное окно", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, WIDTH, HEIGHT, nullptr, nullptr, hInst, nullptr);
			if (!hWnd) {
				throw std::exception("Ошибка: не удалось создать главное окно");
			}
			
			/* Создаём окно просмотра */
			if (!imageWindow.create(hInst, hWnd, 20, 20)) {
				throw std::exception("Ошибка: не удалось создать окно просмотра");
			}

			/* Вывода информации; игнорирую возможные ошибки не созданных окон */
			DWORD widgetStyle = WS_CHILD | WS_VISIBLE | WS_BORDER;

			CreateWindow(L"BUTTON", L"Информация", WS_CHILD | WS_VISIBLE | BS_GROUPBOX | BS_CENTER, 330, 20, 385, 308, hWnd, nullptr, hInst, nullptr);

			CreateWindow(L"STATIC", L"Тип паспорта", widgetStyle, 360, 48, 93, 20, hWnd, nullptr, hInst, nullptr);
			CreateWindow(L"EDIT", L"", widgetStyle, 360, 78, 93, 20, hWnd, (HMENU)IDC_PASSPORT_TYPE, hInst, nullptr);

			CreateWindow(L"STATIC", L"Код гос-ва", widgetStyle, 483, 48, 76, 20, hWnd, nullptr, hInst, nullptr);
			CreateWindow(L"EDIT", L"", widgetStyle, 483, 78, 76, 20, hWnd, (HMENU)IDC_COUNTRY_CODE, hInst, nullptr);

			CreateWindow(L"STATIC", L"Номер паспорта", widgetStyle, 583, 48, 110, 20, hWnd, nullptr, hInst, nullptr);
			CreateWindow(L"EDIT", L"", widgetStyle, 583, 78, 110, 20, hWnd, (HMENU)IDC_PASSPORT_NUM, hInst, nullptr);

			CreateWindow(L"STATIC", L"Фамилия", widgetStyle, 360, 118, 150, 20, hWnd, nullptr, hInst, nullptr);
			CreateWindow(L"EDIT", L"", widgetStyle, 360, 148, 150, 20, hWnd, (HMENU)IDC_PASSPORT_FAM, hInst, nullptr);

			CreateWindow(L"STATIC", L"Имя", widgetStyle, 540, 118, 150, 20, hWnd, nullptr, hInst, nullptr);
			CreateWindow(L"EDIT", L"", widgetStyle, 540, 148, 150, 20, hWnd, (HMENU)IDC_PASSPORT_NAME, hInst, nullptr);

			CreateWindow(L"STATIC", L"Гражданство", widgetStyle, 360, 188, 93, 20, hWnd, nullptr, hInst, nullptr);
			CreateWindow(L"EDIT", L"", widgetStyle, 360, 218, 93, 20, hWnd, (HMENU)IDC_PASSPORT_NAT, hInst, nullptr);
			
			CreateWindow(L"STATIC", L"Пол", widgetStyle, 483, 188, 40, 20, hWnd, nullptr, hInst, nullptr);
			CreateWindow(L"EDIT", L"", widgetStyle, 483, 218, 40, 20, hWnd, (HMENU)IDC_PASSPORT_SEX, hInst, nullptr);

			CreateWindow(L"STATIC", L"Дата рождения", widgetStyle, 360, 258, 110, 20, hWnd, nullptr, hInst, nullptr);
			CreateWindow(L"EDIT", L"", widgetStyle, 360, 288, 110, 20, hWnd, (HMENU)IDC_PASSPORT_BIRTH, hInst, nullptr);

			CreateWindow(L"STATIC", L"Дата истечения", widgetStyle, 500, 258, 110, 20, hWnd, nullptr, hInst, nullptr);
			CreateWindow(L"EDIT", L"", widgetStyle, 500, 288, 110, 20, hWnd, (HMENU)IDC_PASSPORT_EXPIRES, hInst, nullptr);

			/* Деактивируем все поля информации */
			SendDlgItemMessage(hWnd, IDC_PASSPORT_TYPE, EM_SETREADONLY, TRUE, 0);
			SendDlgItemMessage(hWnd, IDC_COUNTRY_CODE, EM_SETREADONLY, TRUE, 0);
			SendDlgItemMessage(hWnd, IDC_PASSPORT_NUM, EM_SETREADONLY, TRUE, 0);
			SendDlgItemMessage(hWnd, IDC_PASSPORT_FAM, EM_SETREADONLY, TRUE, 0);
			SendDlgItemMessage(hWnd, IDC_PASSPORT_NAME, EM_SETREADONLY, TRUE, 0);
			SendDlgItemMessage(hWnd, IDC_PASSPORT_NAT, EM_SETREADONLY, TRUE, 0);
			SendDlgItemMessage(hWnd, IDC_PASSPORT_SEX, EM_SETREADONLY, TRUE, 0);
			SendDlgItemMessage(hWnd, IDC_PASSPORT_BIRTH, EM_SETREADONLY, TRUE, 0);
			SendDlgItemMessage(hWnd, IDC_PASSPORT_EXPIRES, EM_SETREADONLY, TRUE, 0);

			/* Поле ввода второй MRZ строки, кнопки считывания и очистки */
			CreateWindow(L"BUTTON", L"Ввод", WS_CHILD | WS_VISIBLE | BS_GROUPBOX | BS_CENTER, 330, 338, 385, 82, hWnd, nullptr, hInst, nullptr);
			CreateWindow(L"EDIT", L"", widgetStyle, 340, 358, 363, 20, hWnd, (HMENU)IDC_INPUT_MRZ, hInst, nullptr);

			CreateWindow(L"BUTTON", L"Считать", widgetStyle | BS_CENTER, 340, 388, 90, 20, hWnd, (HMENU)IDC_BUTTON_LOAD, hInst, nullptr);
			CreateWindow(L"BUTTON", L"Очистить", widgetStyle | BS_CENTER, 450, 388, 90, 20, hWnd, (HMENU)IDC_BUTTON_CLEAR, hInst, nullptr);

			/* Создать окно отображения прогресса */
			hProgressBar = CreateWindow(PROGRESS_CLASS, L"", widgetStyle, 20, 435, 695, 20, hWnd, nullptr, hInst, nullptr);

			/* Настройка элементов управления */
			SendDlgItemMessage(hWnd, IDC_EDIT_MRZ, EM_SETREADONLY, TRUE, 0);

			/* Сохраняем в переменные окна указатель на текущий класс */
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)this);

			/* Отправляем в поток общения с паспортом главное окно и окно индикатора загрузки */
			PostThreadMessage(passportConnectionThread, WM_COMMAND, ThreadActions::SendMainHWND, (LPARAM)hWnd);
			PostThreadMessage(passportConnectionThread, WM_COMMAND, ThreadActions::SendProgHWND, (LPARAM)hProgressBar);

			/* Обновляем окно и делаем его видимым */
			UpdateWindow(hWnd);
			ShowWindow(hWnd, 1);
		}

		/* Основной цикл программы */
		void run() {
			MSG msg = { 0 };
			while (GetMessage(&msg, nullptr, 0, 0)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		/* Функция обработки сообщений конкретного окна - переопределяет статическую функцию-обработчик */
		LRESULT handleMessages(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
			/* Выделяем массив на 100 символов для хранения введённого MRZ */
			static char mrzStrRaw[100] = { 0 };

			/* Если пришла команда "Закрыть" */
			if (msg == WM_CLOSE) {
				/* Сначала отправить в поток команду закрытия */
				PostThreadMessage(passportConnectionThread, WM_COMMAND, (WPARAM)ThreadActions::Disconnect, 0);

				/* Затем уже закрываться самому */
				PostQuitMessage(0);
				return 0;
			}

			/* Если была нажата кнопка "Считать" */
			if (msg == WM_COMMAND && wParam == IDC_BUTTON_LOAD) {
				/* Считываем MRZ код из строки ввода */
				/* Но сначала - удалить предыдущий MRZ */
				memset(mrzStrRaw, 0, sizeof(mrzStrRaw));

				/* Затем выполнить чтение ввдённых данных */
				/* 'A' потому что используются обычные строки */
				LRESULT mrzLen = GetDlgItemTextA(hWnd, IDC_INPUT_MRZ, mrzStrRaw, sizeof(mrzStrRaw));

				/* Если количество символов меньше 44, то ввели MRZ код не полностью (или вообще ввели) */
				if (mrzLen < 44) {
					MessageBox(hWnd, L"Вы ввели недостаточно символов.\nПожалуйста, проверьте ваш ввод", L"Ошибка", MB_OK | MB_ICONERROR);
					return 0;
				}

				/* Отправляем команды потоку чтения на соединение с картой */
				PostThreadMessage(passportConnectionThread, WM_COMMAND, (WPARAM)ThreadActions::Connect, (LPARAM)mrzStrRaw);

				/* На чтение первой группы данных - MRZ */
				PostThreadMessage(passportConnectionThread, WM_COMMAND, (WPARAM)ThreadActions::ReadDG1, (LPARAM)mrzStrRaw);

				/* На чтение второй группы данных - фото */
				//PostThreadMessage(passportConnectionThread, WM_COMMAND, (WPARAM)ThreadActions::ReadDG2, 0);

				/* На отключение паспорта */
				PostThreadMessage(passportConnectionThread, WM_COMMAND, (WPARAM)ThreadActions::Disconnect, 0);

				/* Очистить все поля */
				SendMessage(hWnd, WM_COMMAND, IDC_BUTTON_CLEAR, 0);
				return 0;
			}

			if (msg == WM_COMMAND && wParam == IDC_BUTTON_CLEAR) {
				/* Очистить текущее изображение */
				imageWindow.clearImage();

				/* Очистить все поля информации */
				for (UINT16 i = IDC_PASSPORT_TYPE; i <= IDC_PASSPORT_EXPIRES; i += 1) {
					SetDlgItemTextA(hWnd, i, "");
				}

				return 0;
			}

			/* Эти команды может отсылать поток сообщения с паспортом */
			/* Их приход означает, что соответствующая команда была выполнена */
			/* Эта команда означает, что считывание DG1 закончилось*/
			if (msg == WM_COMMAND && wParam == ThreadResponces::ReadDG1End) {
				string mrzLine = mrzStrRaw;
				FullMRZDecoder mrzDecoder(mrzLine);

				/* 'A' в конце потому что строки в ASCII */
				for (INT i = 0; i < mrzDecoder.infoVec.size(); i += 1) {
					SetDlgItemTextA(hWnd, IDC_PASSPORT_TYPE + i, mrzDecoder.infoVec[i].c_str());
				}
				
				UpdateWindow(hWnd);
			}

			/* Эта команда означает, что функция сообщения с паспорта закончила считывание DG2 */
			if (msg == WM_COMMAND && wParam == ThreadResponces::ReadDG2End) {
				/* В качестве параметра она передаёт дескриптор открытого файла */
				/* В котором содержится контейнер с изображением */
				ByteStream* imgContainerFile = (ByteStream*)lParam;

				/* Загружаем изображение из этого контейнера */
				imageWindow.loadImage(*imgContainerFile);

				/* И закрываем контейнер */
				imgContainerFile->close();
			}

			return DefWindowProc(hWnd, msg, wParam, lParam);
		}
	};

	/* Функция-заглушка для обработки сообщения */
	/* Если есть указатель на окно - сообщение отдаём ему */
	/* Если указателя нет - справится функция по умолчанию */
	LRESULT CALLBACK MainWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		MainWindow* mainWindow = (MainWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		return (mainWindow ? mainWindow->handleMessages(hWnd, msg, wParam, lParam) : DefWindowProc(hWnd, msg, wParam, lParam));
	}

	/* Функция, выполняющая соединение и общение с паспортом */
	DWORD WINAPI CardConnectionThread(LPVOID lpParameter) {
		/* Содержит в себе классы паспорта, кард-ридера */
		Passport passport;
		CardReader cardReader;

		/* Файл, который будет содержать контейнер с изображением */
		ByteStream imgContainerFile;

		/* Дескриптор главного окна (куда отправлять сообщения) */
		HWND hWnd = nullptr;
		
		/* И дескриптор окна прогресса */
		HWND hProgressBar = nullptr;

		/* Выбираем кард-ридер по умолчанию */
		/* TODO: сделать так, чтобы пользователь мог ридер выбрать */
		auto& readers = cardReader.getReadersList();
		wstring readerName = readers[0];

		/* Создаём очередь сообщений */
		MSG msg = { 0 };
		PeekMessage(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

		/* Сигнализируем, что событие создание потока наступило */
		SetEvent(lpParameter);

		/* Цикл обработки сообщений окна */
		while (GetMessage(&msg, nullptr, 0, 0)) {
			/* Если пришла команда */
			if (msg.message == WM_COMMAND) {
				/* Почти все функции, в случае неверной работы, выбрасывают исключения */
				try {
					/* Если пришло сообщение получения главного окна */
					if (msg.wParam == MainWindow::ThreadActions::SendMainHWND) {
						hWnd = (HWND)msg.lParam;
					}
					/* Если пришло сообщение получения окна прогресса */
					else if (msg.wParam == MainWindow::ThreadActions::SendProgHWND) {
						hProgressBar = (HWND)msg.lParam;
					}
					/* Если пришло сообщение на соединение с паспортом */
					else if (msg.wParam == MainWindow::ThreadActions::Connect) {
						/* Пробуем подключиться к карте */
						passport.connectPassport(cardReader, readerName);

						/* Получаем из переданного параметра указатель на массив */
						/* В котором находится вторая MRZ строка */
						char* mrzLine2Raw = (char*)msg.lParam;
						string mrzLine2 = mrzLine2Raw;

						/* Выполняем аутентификацию по BAC */
						passport.perfomBAC(cardReader, mrzLine2);
					}
					/* Если пришло сообщение о чтении первой группы данных */
					else if (msg.wParam == MainWindow::ThreadActions::ReadDG1) {
						/* Читаем первую группу данных */
						passport.readDG(cardReader, Passport::DGFILES::DG1, hProgressBar);

						/* Получаем массив, куда надо записать MRZ код */
						char* mrzStrRaw = (char*)msg.lParam;

						/* Получаем сам MRZ код */
						ByteVec mrzCode = passport.getPassportMRZ();

						/* Копируем MRZ из вектора в массив */
						memcpy(mrzStrRaw, mrzCode.data(), mrzCode.size());

						/* Отправляем сообщение о том, что всё благополучно считалось */
						SendMessage(hWnd, WM_COMMAND, MainWindow::ThreadResponces::ReadDG1End, 0);
					}
					/* Если пришло сообщение о том, что надо считать вторую группу данных */
					else if (msg.wParam == MainWindow::ThreadActions::ReadDG2) {
						/* Считываем вторую группу данных */
						passport.readDG(cardReader, Passport::DGFILES::DG2, hProgressBar);

						/* Получаем данные тега 5F2E или 7F2E */
						ByteVec rawImage = passport.getPassportRawImage();

						/* Записываем данные этого тега файл контейнра изображения */
					#ifdef _DEBUG
						imgContainerFile.open("imgContainer.bin", ios::in | ios::out | ios::binary | ios::trunc);
					#else
						/* Создаём временный файл, куда запишем контейнер */
						imgContainerFile = ByteStream(tmpfile());
					#endif
						imgContainerFile.write(rawImage.data(), rawImage.size());
						imgContainerFile.flush();
						imgContainerFile.seekg(0);

						/* И отправляем сообщение о том, что всё считалось, плюс дескриптор открытого файла */
						if (!hWnd) { throw std::exception("Ошибка: не был получен hWnd для потока паспорта"); }
						SendMessage(hWnd, WM_COMMAND, MainWindow::ThreadResponces::ReadDG2End, (LPARAM)&imgContainerFile);
					}
					/* Если пришло сообщение об отключении паспорта */
					else if (msg.wParam == MainWindow::ThreadActions::Disconnect) {
						/* Собственно, отключаем */
						passport.disconnectPassport(cardReader);
					}
				}
				catch (std::exception& e) {
					/* В случае каких-либо происшествий - отключить паспорт и выдать текст ошибки */
					passport.disconnectPassport(cardReader);
					MessageBoxA(hWnd, e.what(), "Ошибка", MB_OK | MB_ICONERROR);
				}
			}
		}

		return 0;
	}
}