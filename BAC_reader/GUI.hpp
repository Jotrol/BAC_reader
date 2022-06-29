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

		/* Кисть для отрисовки заднего фона */
		HBRUSH hBgBrush;

		/* Класс, который умеет декодировать изображения и выводить их RGB данные */
		fipImage image;

		/* Положение левого верхнего угла для выравнивания по центру */
		POINT imagePos;

		/* Размеры окна для просмотра  */
		const INT INIT_WIDTH = 300;
		const INT INIT_HEIGHT = 410;

		/* Класс окна просмотра изображения */
		const wchar_t* CLASS_NAME = L"ImageWindow";
		
		/* Функция регистрации класса окна просмотра - без неё окно не создать */
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
		/* Конструктор по умолчанию необходим для правильной инициализации внутри класса */
		ImageWindow() : hWnd(nullptr), imagePos({ 0 }), hBgBrush(nullptr) {}

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

			/* Обрезаем изображение на чтобы оно по размеру было на 10 пикселей меньше выделенного окна */
			image.crop(0, 0, INIT_WIDTH - 20, INIT_HEIGHT - 25);

			/* Центрируем изображение внутри виджета */
			imagePos.x = INIT_WIDTH / 2 - image.getWidth() / 2;
			imagePos.y = INIT_HEIGHT / 2 - image.getHeight() / 2 + 5;

			/* Разрешаем показ изображения, подав указатель для обработчика */
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)this);

			/* Заставляем выполнить перерисовку из-за смены изображения */
			InvalidateRect(hWnd, nullptr, TRUE);
		}

		/* Функция отображения в окне изображения */
		void drawImage() {
			PAINTSTRUCT ps = { 0 };
			HDC hdc = BeginPaint(hWnd, &ps);
			FillRect(hdc, &ps.rcPaint, hBgBrush);

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

		/* Дескриптор для GroupBox данных паспорта */
		HWND hInfoBox;

		/* Дескриптор для ComboBox из имён доступных кардридеров */
		HWND hReadersList;

		/* Дескриптор для индикатора прогресса */
		HWND hProgressBar;

		/* Окно просмотра изображения */
		ImageWindow imageWindow;

		/* Идентификатор потока сообщения с паспортом */
		DWORD passportConnectionThread;

		/* Размеры главного окна */
		const INT WIDTH = 920;
		const INT HEIGHT = 515;

		/* Название класса окна */
		const wchar_t* CLASS_NAME = L"MainWindow";

		/* Строка "О программе" */
		const wchar_t* ABOUT_STR = L"Программа написана в рамках производственной практики\nПрактикант: Ильичев Д.А.\nНаучный руководитель: Бонч-Бруевич М.А.";

		/* Функция регистрации класса окна */
		bool RegisterMainWindowClass(HINSTANCE hInst) {
			WNDCLASS wc = { 0 };
			wc.lpszClassName = CLASS_NAME;
			wc.hbrBackground = (HBRUSH)CreateSolidBrush(RGB(240, 240, 240));
			wc.hInstance = hInst;
			wc.lpfnWndProc = MainWindowProc;

			return RegisterClass(&wc);
		}
	public:
		/* Сообщения для взаимодействия с потоком чтения паспорта */
		enum ThreadActions { SendMainHWND, SendProgHWND, GetReaders, SetReader, Connect, ReadDG1, ReadDG2, Disconnect, Stop };
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
			DWORD textStyle = WS_CHILD | WS_VISIBLE | ES_RIGHT;
			DWORD editStyle = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER | ES_READONLY;

			/* Небольшой макрос чтобы уменьшить объём кода ниже: просто создаёт текст: поле-ввода одинаковой длины */
			/* Использует вышеуказанные стили и hInst */
			#define CREATE_EDIT_FIELD(label, x, y, w, wnd, command) \
					CreateWindow(L"STATIC", label, textStyle, x, y, w, 20, wnd, nullptr, hInst, nullptr);\
					CreateWindow(L"EDIT", L"", editStyle, x + w + 10, y, w, 20, wnd, (HMENU)command, hInst, nullptr)\

			/* Создание группы данных для отображения информации о паспорте */
			hInfoBox = CreateWindow(L"BUTTON", L"Информация", WS_CHILD | WS_VISIBLE | BS_GROUPBOX | BS_CENTER, 330, 20, 560, 220, hWnd, nullptr, hInst, nullptr);
			CREATE_EDIT_FIELD(L"Тип паспорта:", 20, 30, 120, hInfoBox, IDC_PASSPORT_TYPE);
			CREATE_EDIT_FIELD(L"Код гос-ва:", 290, 30, 120, hInfoBox, IDC_COUNTRY_CODE);

			CREATE_EDIT_FIELD(L"Номер паспорта:", 20, 60, 120, hInfoBox, IDC_PASSPORT_NUM);

			CREATE_EDIT_FIELD(L"Фамилия:", 20, 100, 120, hInfoBox, IDC_PASSPORT_FAM);
			CREATE_EDIT_FIELD(L"Имя:", 290, 100, 120, hInfoBox, IDC_PASSPORT_NAME);

			CREATE_EDIT_FIELD(L"Гражданство:", 20, 140, 120, hInfoBox, IDC_PASSPORT_NAT);
			CREATE_EDIT_FIELD(L"Пол:", 290, 140, 120, hInfoBox, IDC_PASSPORT_SEX);

			CREATE_EDIT_FIELD(L"Дата рождения:", 20, 180, 120, hInfoBox, IDC_PASSPORT_BIRTH);
			CREATE_EDIT_FIELD(L"Дата истечения:", 290, 180, 120, hInfoBox, IDC_PASSPORT_EXPIRES);

			/* Размещение ComboBox для выбора кардридера */
			CreateWindow(L"BUTTON", L"Выберите кардридер", WS_CHILD | WS_VISIBLE | BS_GROUPBOX | BS_CENTER, 330, 250, 560, 60, hWnd, nullptr, hInst, nullptr);
			hReadersList = CreateWindow(WC_COMBOBOX, L"", CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_CHILD | WS_VISIBLE, 350, 270, 520, 200, hWnd, nullptr, hInst, nullptr);

			/* Поле ввода второй MRZ строки */
			CreateWindow(L"BUTTON", L"Введите вторую строку MRZ", WS_CHILD | WS_VISIBLE | BS_GROUPBOX | BS_CENTER, 330, 310, 560, 60, hWnd, nullptr, hInst, nullptr);
			CreateWindow(L"EDIT", L"", editStyle & ~ES_READONLY, 350, 330, 520, 20, hWnd, (HMENU)IDC_INPUT_MRZ, hInst, nullptr);

			/* Поле команд программы */
			CreateWindow(L"BUTTON", L"Выберите действие", WS_CHILD | WS_VISIBLE | BS_GROUPBOX | BS_CENTER, 330, 370, 560, 60, hWnd, nullptr, hInst, nullptr);
			CreateWindow(L"BUTTON", L"Считать", WS_CHILD | WS_VISIBLE | WS_BORDER, 350, 395, 160, 20, hWnd, (HMENU)IDC_BUTTON_LOAD, hInst, nullptr);
			CreateWindow(L"BUTTON", L"О программе", WS_CHILD | WS_VISIBLE | WS_BORDER, 530, 395, 160, 20, hWnd, (HMENU)IDC_BUTTON_ABOUT, hInst, nullptr);
			CreateWindow(L"BUTTON", L"Выход", WS_CHILD | WS_VISIBLE | WS_BORDER, 710, 395, 160, 20, hWnd, (HMENU)IDC_BUTTON_EXIT, hInst, nullptr);

			/* Создать окно отображения прогресса */
			hProgressBar = CreateWindow(PROGRESS_CLASS, L"", WS_CHILD | WS_VISIBLE, 20, 440, 870, 20, hWnd, nullptr, hInst, nullptr);

			/* Сохраняем в переменные окна указатель на текущий класс */
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)this);

			/* Отправляем в поток общения с паспортом главное окно и окно индикатора загрузки */
			PostThreadMessage(passportConnectionThread, WM_COMMAND, ThreadActions::SendMainHWND, (LPARAM)hWnd);
			PostThreadMessage(passportConnectionThread, WM_COMMAND, ThreadActions::SendProgHWND, (LPARAM)hProgressBar);
			PostThreadMessage(passportConnectionThread, WM_COMMAND, ThreadActions::GetReaders, (LPARAM)hReadersList);

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

			if (msg == WM_COMMAND && wParam == IDC_BUTTON_ABOUT) {
				MessageBox(hWnd, ABOUT_STR, L"О программе", MB_OK | MB_ICONASTERISK);
				return 0;
			}

			/* Если пришла команда "Закрыть" или нажали кнопку "Выход" */
			if (msg == WM_CLOSE || (msg == WM_COMMAND && wParam == IDC_BUTTON_EXIT)) {
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

				/* Выбираем кардридер */
				PostThreadMessage(passportConnectionThread, WM_COMMAND, (WPARAM)ThreadActions::SetReader, (LPARAM)hReadersList);

				/* Отправляем команды потоку чтения на соединение с картой */
				PostThreadMessage(passportConnectionThread, WM_COMMAND, (WPARAM)ThreadActions::Connect, (LPARAM)mrzStrRaw);

				/* На чтение первой группы данных - MRZ */
				PostThreadMessage(passportConnectionThread, WM_COMMAND, (WPARAM)ThreadActions::ReadDG1, (LPARAM)mrzStrRaw);

				/* На чтение второй группы данных - фото */
				PostThreadMessage(passportConnectionThread, WM_COMMAND, (WPARAM)ThreadActions::ReadDG2, 0);

				/* На отключение паспорта */
				PostThreadMessage(passportConnectionThread, WM_COMMAND, (WPARAM)ThreadActions::Disconnect, 0);
				
				/* Очистить текущее изображение */
				imageWindow.clearImage();

				/* Очистить все поля информации */
				for (UINT16 i = IDC_PASSPORT_TYPE; i <= IDC_PASSPORT_EXPIRES; i += 1) {
					SetDlgItemTextA(hInfoBox, i, "");
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
					SetDlgItemTextA(hInfoBox, IDC_PASSPORT_TYPE + i, mrzDecoder.infoVec[i].c_str());
				}
				
				/* Ставим позицию на ноль в индикаторе прогресса */
				SendMessage(hProgressBar, PBM_SETPOS, 0, 0);

				/* Обновляем окно */
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

				/* Ставим позицию на ноль в индикаторе прогресса */
				SendMessage(hProgressBar, PBM_SETPOS, 0, 0);

				/* Обновляем окно */
				UpdateWindow(hWnd);
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

		/* Имя кардридера */
		wstring readerName = L"";

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
					else if (msg.wParam == MainWindow::ThreadActions::GetReaders) {
						/* Получаем список доступных системе кардридеров */
						vector<wstring> readersList = cardReader.getReadersList();

						/* Заполняем окошко выбора названиями этих ридеров */
						for (const auto& reader : readersList) {
							SendMessage((HWND)msg.lParam, CB_ADDSTRING, 0, (LPARAM)reader.c_str());
						}

						/* Устанавливаем первый ридер по умолчанию */
						SendMessage((HWND)msg.lParam, CB_SETCURSEL, 0, 0);
					}
					else if (msg.wParam == MainWindow::ThreadActions::SetReader) {
						/* Узнаем индекс выбранного ридера; сортировка в списке в окне отключена */
						DWORD readerIndex = SendMessage((HWND)msg.lParam, CB_GETCURSEL, 0, 0);

						/* Узнаём размер его названия (в символах) */
						DWORD readerNameSize = SendMessage((HWND)msg.lParam, CB_GETLBTEXTLEN, readerIndex, 0);

						/* Расширяем строку */
						readerName.resize(readerNameSize);

						/* Получаем название ридера в строку */
						SendMessage((HWND)msg.lParam, CB_GETLBTEXT, 0, (LPARAM)readerName.data());
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
						if (!hWnd) { throw std::exception("Не был получен hWnd для потока паспорта"); }
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
						if (!hWnd) { throw std::exception("Не был получен hWnd для потока паспорта"); }
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