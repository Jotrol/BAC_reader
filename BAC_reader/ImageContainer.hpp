#pragma once

#include "Util.hpp"

#include <Windows.h>
#include "FreeImagePlus.h"
#pragma comment(lib, "FreeImagePlus.lib")

#include <fstream>
#include <vector>
using namespace std;

namespace ImageContainer {
	typedef basic_fstream<unsigned char> ImageStream;

	/* Для декодирования заголовоков изображения используется стандарт ISO/IEC 19794-5-2006, а не 2013 */
	/* Если понадобится, от всех заголовков можно будет избавиться */
	class Image_ISO19794_5_2006 {
	private:
		/* Структуры блоков контейнера изображений */
		/* Выровнены по 1 байту, для удобства */
#pragma pack(push, 1)
		struct FaceImageHeader {
			BYTE formatID[4];
			BYTE standartNum[4];
			UINT32 entryLen;
			UINT16 facesCount;
		};
		struct FaceInformation {
			UINT32 faceDataLen;
			UINT16 controlPointsCount;
			BYTE sex;
			BYTE eyesColor;
			BYTE hairColor;
			BYTE propertyMask[3];
			UINT16 faceLook;
			BYTE angleCoords[3];
			BYTE angleCoordsError[3];
		};
		struct ControlPoint {
			BYTE controlPointType;
			BYTE controlPointCode;
			UINT16 coordX;
			UINT16 coordY;
			UINT16 coordZ;
		};
		struct ImageInfo {
			BYTE imageTypeInfo;
			BYTE imageDataType;
			UINT16 imageHorizontalSize;
			UINT16 imageVerticalSize;
			BYTE imageColorSpace;
			BYTE imageSourceInfo;
			UINT16 imageCaptureType;
			UINT16 imageQuality;
		};
#pragma pack(pop)

		/* Функции чтения каждого из блоков */
		FaceImageHeader readFaceImageHeader(ImageStream& file) {
			FaceImageHeader faceImageHeader = { 0 };
			file.read((UINT8*)&faceImageHeader, sizeof(FaceImageHeader));

			faceImageHeader.entryLen = Util::changeEndiannes(faceImageHeader.entryLen);
			faceImageHeader.facesCount = Util::changeEndiannes(faceImageHeader.facesCount);

			return faceImageHeader;
		}
		FaceInformation readFaceInformation(ImageStream& file) {
			FaceInformation faceInfo = { 0 };
			file.read((UINT8*)&faceInfo, sizeof(FaceInformation));

			faceInfo.faceDataLen = Util::changeEndiannes(faceInfo.faceDataLen);
			faceInfo.controlPointsCount = Util::changeEndiannes(faceInfo.controlPointsCount);
			faceInfo.faceLook = Util::changeEndiannes(faceInfo.faceLook);

			return faceInfo;
		}
		vector<ControlPoint> readControlPoints(ImageStream& file, UINT16 controlPointsCount) {
			vector<ControlPoint> controlPoints(controlPointsCount);

			ControlPoint point = { 0 };
			for (int i = 0; i < controlPointsCount; i += 1) {
				file.read((UINT8*)&point, sizeof(ControlPoint));

				point.coordX = Util::changeEndiannes(point.coordX);
				point.coordY = Util::changeEndiannes(point.coordY);
				point.coordZ = Util::changeEndiannes(point.coordZ);

				controlPoints.push_back(point);
			}

			return controlPoints;
		}
		ImageInfo readImageInfo(ImageStream& file) {
			ImageInfo imgInfo = { 0 };
			file.read((UINT8*)&imgInfo, sizeof(ImageInfo));

			imgInfo.imageHorizontalSize = Util::changeEndiannes(imgInfo.imageHorizontalSize);
			imgInfo.imageVerticalSize = Util::changeEndiannes(imgInfo.imageVerticalSize);
			imgInfo.imageCaptureType = Util::changeEndiannes(imgInfo.imageCaptureType);
			imgInfo.imageQuality = Util::changeEndiannes(imgInfo.imageQuality);

			return imgInfo;
		}

		UINT32 imgDataSize;
		unique_ptr<BYTE> imgData;
		vector<ControlPoint> imgControlPoints;

		FaceImageHeader faceImageHeader;
		FaceInformation faceInformation;
		ImageInfo imageInfo;
	public:
		Image_ISO19794_5_2006(ImageStream& file) {
			faceImageHeader = readFaceImageHeader(file);

			/* Необходимо запомнить смещение начала информации об блоке изображения */
			streampos faceInfoStart = file.tellg();
			faceInformation = readFaceInformation(file);
			imgControlPoints = readControlPoints(file, faceInformation.controlPointsCount);
			imageInfo = readImageInfo(file);

			/* Считаем размер данних о лице с заголовком и вычитаем из ожидаемого размера */
			imgDataSize = faceInformation.faceDataLen - (file.tellg() - faceInfoStart);

			/* Выделяем память под данные изображения и считываем его */
			imgData.reset(new BYTE[imgDataSize]);
			file.read(imgData.get(), imgDataSize);
		}

		BYTE* getRawImage() const {
			return imgData.get();
		}
		UINT32 getRawImageSize() const {
			return imgDataSize;
		}
		
		/* Можно добавить функции для получения различных данных из субструктур */
	};

	typedef Image_ISO19794_5_2006 Image;

	/* Предположительно, лучше перенести в другое пространство имён, скажем, GUI, где и будет вся логика */
	/* Функция оконной процедуры для отдельного окна */
	LRESULT CALLBACK ImageWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	/* Класса окна просмотра изображений */
	class ImageWindow {
	private:
		/* Хэндл изображения из библиотеки FreeImage */
		fipImage image;

		/* Информация об изображении для отображения в GDI */
		BITMAPINFO bmi;

		/* Хэндл окна просмотра */
		HWND hWnd;

		/* Название класса просмотра */
		const wchar_t* ImageClassName = L"ImageWindow";

		/* Функция регистрации класса окна */
		bool RegisterImageWindowClass(HINSTANCE hInst) {
			WNDCLASS wc = { 0 };
			wc.hInstance = hInst;
			wc.lpfnWndProc = ImageWindowProc;
			wc.lpszClassName = ImageClassName;

			return RegisterClass(&wc);
		}
	public:
		/* Конструктор класса просмотра изображения */
		ImageWindow(HINSTANCE hInst, const Image& isoImage, HWND hParentWnd = nullptr, INT x = 0, INT y = 0) {
			/* Из переданного контейнера изображения получаем изображение и обрамляем его в класс для FreeImage */
			fipMemoryIO tempImage(isoImage.getRawImage(), isoImage.getRawImageSize());

			/* Загружаем из памяти изображение */
			if (!image.loadFromMemory(tempImage)) {
				throw std::exception("Ошибка: не удалось загрузить фотографию");
			}

			/* Очищаем буфер */
			tempImage.close();

			/* Конвертируем изображение в 24-х битный формат */
			/* Чтобы Windows сама определяла некоторые параметры изображения */
			if (!image.convertTo24Bits()) {
				throw std::exception("Ошибка: не удалось конвертировать фотографию");
			}

			/* Конструируем данные об изображении */
			DWORD w = image.getWidth();
			DWORD h = image.getHeight();
			bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bmi.bmiHeader.biWidth = w;
			bmi.bmiHeader.biHeight = h;
			bmi.bmiHeader.biPlanes = 1;
			bmi.bmiHeader.biCompression = BI_RGB;
			bmi.bmiHeader.biBitCount = image.getBitsPerPixel();

			/* Регистрация класса окна */
			if (!RegisterImageWindowClass(hInst)) {
				throw std::exception("Ошибка: не удалось зарегистрировать класс окна для изображений");
			}

			/* Настраиваем стили окна просмотра: либо самостоятельное, либо дочернее */
			DWORD imageWindowStyle = WS_OVERLAPPEDWINDOW;
			if (hParentWnd) {
				imageWindowStyle = WS_CHILD | WS_OVERLAPPED;
			}

			/* Создаём окно */
			hWnd = CreateWindow(ImageClassName, L"", imageWindowStyle, x, y, w, h, hParentWnd, nullptr, hInst, nullptr);
			if (!hWnd) {
				throw std::exception("Ошибка: не удалось создать окно");
			}

			/* В переменную пользовательских данных окна записываем указатель на класс */
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)this);

			/* Обновляем окно */
			UpdateWindow(hWnd);

			/* Делаем окно видимым */
			ShowWindow(hWnd, 1);

			/* Удаляем класс окна */
			UnregisterClass(ImageClassName, hInst);
		}

		/* Функция изменения изображения в окне */
		bool reloadImage(const Image& isoImage) {
			/* Сначала удалить данные прошлого изображения */
			image.clear();

			/* Загрузить из памяти обычной в память FreeImage */
			fipMemoryIO tempImage(isoImage.getRawImage(), isoImage.getRawImageSize());

			/* Загрузить из памяти FreeImage в изображение */
			if (!image.loadFromMemory(tempImage)) {
				return false;
			}

			/* Очистить память FreeImage */
			tempImage.close();

			/* Конвертировать изображение в 24-х битный формат */
			if (!image.convertTo24Bits()) {
				image.clear();
				return false;
			}

			/* Заполнить структуру изображения заново */
			DWORD w = image.getWidth();
			DWORD h = image.getHeight();
			bmi.bmiHeader.biWidth = w;
			bmi.bmiHeader.biHeight = h;
			bmi.bmiHeader.biBitCount = image.getBitsPerPixel();

			/* Изменить размер окна под стать новому изображению */
			MoveWindow(hWnd, 0, 0, w, h, false);

			/* Вызвать перерисовку окна */
			InvalidateRect(hWnd, nullptr, true);
			return true;
		}

		/* Функция обработки сообщений окна, которая меняет поведение приложения */
		LRESULT CALLBACK handleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
			switch (message) {
			case WM_PAINT:
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hWnd, &ps);

				/* Закрашиваем всё окно белым цветом */
				FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));

				/* Выводим изображение */
				SetDIBitsToDevice(hdc, 0, 0, image.getWidth(), image.getHeight(), 0, 0, 0, image.getHeight(), image.accessPixels(), &bmi, DIB_RGB_COLORS);
				EndPaint(hWnd, &ps);
				return 0;
			}

			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	};

	/* Функиця обработки сообщений окна, общей для всех ImageWindow */
	LRESULT CALLBACK ImageWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
		/* Получаем указатель на ImageWindow из пользовательских данных */
		ImageWindow* imageWindow = (ImageWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

		/* Обрабатываем общие сообщения */
		switch (message) {
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		}

		/* На основе того, удалось ли получить указатель или нет, определяем, кому отдать необработанное сообщение */
		return (imageWindow ? imageWindow->handleMessage(hWnd, message, wParam, lParam)
			: DefWindowProc(hWnd, message, wParam, lParam));
	}
}