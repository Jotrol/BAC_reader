#include <Windows.h>

#include "Crypto.hpp"
#include "CardReader.hpp"
#include "MRZDecoder.hpp"

#include "FreeImagePlus.h"
#pragma comment(lib, "FreeImagePlus.lib")

#include <fstream>
#include <memory>
using namespace std;

#define IS_LITTLE_ENDIAN  ('ABCD'==0x41424344UL) //41 42 43 44 = 'ABCD' hex ASCII code
#define IS_BIG_ENDIAN     ('ABCD'==0x44434241UL) //44 43 42 41 = 'DCBA' hex ASCII code
#define IS_UNKNOWN_ENDIAN (IS_LITTLE_ENDIAN == IS_BIG_ENDIAN)

//LRESULT CALLBACK ImageWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
//class ImageWindow {
//private:
//	fipImage image;
//	BITMAPINFO bmi;
//	HWND hWnd;
//	
//	const wchar_t* ImageClassName = L"ImageWindow";
//	const DWORD ImageWindowStyle = WS_CHILD | WS_OVERLAPPED;
//
//	bool RegisterImageWindowClass(HINSTANCE hInst) {
//		WNDCLASS wc = { 0 };
//		wc.hInstance = hInst;
//		wc.lpfnWndProc = ImageWindowProc;
//		wc.lpszClassName = ImageClassName;
//
//		return RegisterClass(&wc);
//	}
//public:
//	ImageWindow(HINSTANCE hInst, HWND hParentWnd, INT x, INT y, const char* filename) {
//		if (!image.load(filename)) {
//			throw std::exception("Ошибка: не удалось загрузить фотографию");
//		}
//		if (!image.convertTo24Bits()) {
//			throw std::exception("Ошибка: не удалось конвертировать фотографию");
//		}
//
//		DWORD w = image.getWidth();
//		DWORD h = image.getHeight();
//		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
//		bmi.bmiHeader.biWidth = w;
//		bmi.bmiHeader.biHeight = h;
//		bmi.bmiHeader.biPlanes = 1;
//		bmi.bmiHeader.biCompression = BI_RGB;
//		bmi.bmiHeader.biBitCount = image.getBitsPerPixel();
//
//		hWnd = CreateWindow(ImageClassName, L"", ImageWindowStyle, x, y, w, h, hParentWnd, nullptr, hInst, nullptr);
//		if (!hWnd) {
//			if (!RegisterImageWindowClass(hInst)) {
//				throw std::exception("Ошибка: не удалось зарегистрировать класс окна для изображений");
//			}
//			hWnd = CreateWindow(ImageClassName, L"", ImageWindowStyle, x, y, w, h, hParentWnd, nullptr, hInst, nullptr);
//			if (!hWnd) {
//				throw std::exception("Ошибка: не удалось создать окно");
//			}
//		}
//
//		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)this);
//		UpdateWindow(hWnd);
//		ShowWindow(hWnd, 1);
//	}
//
//	bool reloadImage(const char* filename) {
//		image.clear();
//
//		if (!image.load(filename)) {
//			return false;
//		}
//		if (!image.convertTo24Bits()) {
//			image.clear();
//			return false;
//		}
//
//		DWORD w = image.getWidth();
//		DWORD h = image.getHeight();
//		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
//		bmi.bmiHeader.biWidth = w;
//		bmi.bmiHeader.biHeight = h;
//		bmi.bmiHeader.biPlanes = 1;
//		bmi.bmiHeader.biCompression = BI_RGB;
//		bmi.bmiHeader.biBitCount = image.getBitsPerPixel();
//
//		MoveWindow(hWnd, 0, 0, w, h, false);
//		InvalidateRect(hWnd, nullptr, true);
//		return true;
//	}
//	
//	LRESULT CALLBACK handleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
//		switch (message) {
//		case WM_PAINT:
//			PAINTSTRUCT ps;
//			HDC hdc = BeginPaint(hWnd, &ps);
//			FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
//			SetDIBitsToDevice(hdc, 0, 0, image.getWidth(), image.getHeight(), 0, 0, 0, image.getHeight(), image.accessPixels(), &bmi, DIB_RGB_COLORS);
//			EndPaint(hWnd, &ps);
//			return 0;
//		}
//
//		return DefWindowProc(hWnd, message, wParam, lParam);
//	}
//};
//LRESULT CALLBACK ImageWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
//	ImageWindow* imageWindow = (ImageWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
//
//	switch (message) {
//	case WM_DESTROY:
//		PostQuitMessage(0);
//		return 0;
//	}
//
//	return (imageWindow ? imageWindow->handleMessage(hWnd, message, wParam, lParam)
//		: DefWindowProc(hWnd, message, wParam, lParam));
//}
//
//ImageWindow* image = nullptr;
//LRESULT CALLBACK ParentProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
//	switch (message) {
//	case WM_DESTROY:
//		PostQuitMessage(0);
//		return 0;
//	case WM_PAINT:
//		PAINTSTRUCT ps = { 0 };
//		HDC hdc = BeginPaint(hWnd, &ps);
//		FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
//		EndPaint(hWnd, &ps);
//		return 0;
//	}
//	return DefWindowProc(hWnd, message, wParam, lParam);
//}
//
//HWND createParentWindow(HINSTANCE hInst) {
//	WNDCLASS wc = {};
//	wc.lpfnWndProc = ParentProc;
//	wc.hInstance = hInst;
//	wc.lpszClassName = L"ParentWindow";
//	if (!RegisterClass(&wc)) {
//		MessageBox(nullptr, L"Не удалось зарегистрировать класс окна", L"Ошибка", MB_OK | MB_ICONERROR);
//		return nullptr;
//	}
//
//	HWND hWnd = CreateWindow(L"ParentWindow", L"Parent Window", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 500, 500, nullptr, nullptr, hInst, nullptr);
//	if (!hWnd) {
//		MessageBox(nullptr, L"Не удалось создать окно", L"Ошибка", MB_OK | MB_ICONERROR);
//		return nullptr;
//	}
//
//	UpdateWindow(hWnd);
//	ShowWindow(hWnd, 1);
//	return hWnd;
//}
//
//int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, PSTR pCmdLine, int nCmdShow) {
//	HWND parentWindow = createParentWindow(hInstance);
//	image = new ImageWindow(hInstance, parentWindow, 0, 0, "Faceimage.jp2");
//
//	MSG msg = { 0 };
//	while (GetMessage(&msg, NULL, 0, 0)) {
//		TranslateMessage(&msg);
//		DispatchMessage(&msg);
//	}
//
//	return 0;
//}


/* Для декодирования заголовоков изображения используется стандарт ISO/IEC 19794-5-2006, а не 2013 */
class Image_ISO19794_5_2006 {

};

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

/* TODO: дописать разбор заголовка изображения */
/* TODO: специализировать шаблоны */
/* Функция разворота байтов; пока своя, позже специализирую шаблон на все встречающиеся типы встроенными в MSVC функциями */
template<typename T>
T changeEndiannes(T val) {
	T out = 0;
#if IS_LITTLE_ENDIAN
	char* reversed = (char*)&out;
	char* valBytes = (char*)&val;
	for (int i = 0; i < sizeof(T); i += 1) {
		reversed[sizeof(T) - 1 - i] = valBytes[i];
	}
#elif IS_BIG_ENDIAN
	out = val;
#endif
	return out;
}

FaceImageHeader readFaceImageHeader(ifstream& file) {
	FaceImageHeader faceImageHeader = { 0 };
	file.read((char*)&faceImageHeader, sizeof(FaceImageHeader));

	faceImageHeader.entryLen = changeEndiannes(faceImageHeader.entryLen);
	faceImageHeader.facesCount = changeEndiannes(faceImageHeader.facesCount);

	return faceImageHeader;
}
FaceInformation readFaceInformation(ifstream& file) {
	FaceInformation faceInfo = { 0 };
	file.read((char*)&faceInfo, sizeof(FaceInformation));

	faceInfo.faceDataLen = changeEndiannes(faceInfo.faceDataLen);
	faceInfo.controlPointsCount = changeEndiannes(faceInfo.controlPointsCount);
	faceInfo.faceLook = changeEndiannes(faceInfo.faceLook);

	return faceInfo;
}
vector<ControlPoint> readControlPoints(ifstream& file, UINT16 controlPointsCount) {
	vector<ControlPoint> controlPoints(controlPointsCount);

	ControlPoint point = { 0 };
	for (int i = 0; i < controlPointsCount; i += 1) {
		file.read((char*)&point, sizeof(ControlPoint));

		point.coordX = changeEndiannes(point.coordX);
		point.coordY = changeEndiannes(point.coordY);
		point.coordZ = changeEndiannes(point.coordZ);

		controlPoints.push_back(point);
	}

	return controlPoints;
}

#pragma pack(pop)

int main() {
	ifstream file("EncodedPhotoSmirnova.bin", ios::binary | ios::in);
	FaceImageHeader faceImageHeader = readFaceImageHeader(file);
	FaceInformation faceInfo = readFaceInformation(file);
	vector<ControlPoint> controlPoints = readControlPoints(file, faceInfo.controlPointsCount);


	file.close();

	//BYTE faceImageBlockLen[4] = { 0 };
	//file.read((char*)faceImageBlockLen, 4);

	//UINT32 blockLen = (faceImageBlockLen[0] << 24) | (faceImageBlockLen[1] << 16) | (faceImageBlockLen[2] << 8) | (faceImageBlockLen[3] << 0);
	//streampos imageBlockStart = file.tellg();

	//BYTE tmp[2] = { 0 };
	//file.read((char*)tmp, 2);
	//file.seekg(14, ios::cur);
	//
	//UINT16 numPP = (tmp[0] << 8) | (tmp[1]);
	//file.seekg(numPP * 8, ios::cur);
	//file.seekg(1, ios::cur);

	//BYTE imgType = file.get();
	//file.seekg(10, ios::cur);

	//streampos faceImageBlockEnd = file.tellg();
	//UINT32 photoLen = blockLen - (faceImageBlockEnd - imageBlockStart) - 4;

	//unique_ptr<BYTE> photo(new BYTE[photoLen]{0});
	//file.read((char*)photo.get(), photoLen);
	//file.close();

	//ofstream photoFile("fotka.jpg", ios::binary | ios::out);
	//photoFile.write((char*)photo.get(), photoLen);
	//photoFile.close();
	return 0;
}