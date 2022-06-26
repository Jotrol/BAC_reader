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

	/* ��� ������������� ����������� ����������� ������������ �������� ISO/IEC 19794-5-2006, � �� 2013 */
	/* ���� �����������, �� ���� ���������� ����� ����� ���������� */
	class Image_ISO19794_5_2006 {
	private:
		/* ��������� ������ ���������� ����������� */
		/* ��������� �� 1 �����, ��� �������� */
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

		/* ������� ������ ������� �� ������ */
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

			/* ���������� ��������� �������� ������ ���������� �� ����� ����������� */
			streampos faceInfoStart = file.tellg();
			faceInformation = readFaceInformation(file);
			imgControlPoints = readControlPoints(file, faceInformation.controlPointsCount);
			imageInfo = readImageInfo(file);

			/* ������� ������ ������ � ���� � ���������� � �������� �� ���������� ������� */
			imgDataSize = faceInformation.faceDataLen - (file.tellg() - faceInfoStart);

			/* �������� ������ ��� ������ ����������� � ��������� ��� */
			imgData.reset(new BYTE[imgDataSize]);
			file.read(imgData.get(), imgDataSize);
		}

		BYTE* getRawImage() const {
			return imgData.get();
		}
		UINT32 getRawImageSize() const {
			return imgDataSize;
		}
		
		/* ����� �������� ������� ��� ��������� ��������� ������ �� ����������� */
	};

	typedef Image_ISO19794_5_2006 Image;

	/* ����������������, ����� ��������� � ������ ������������ ���, ������, GUI, ��� � ����� ��� ������ */
	/* ������� ������� ��������� ��� ���������� ���� */
	LRESULT CALLBACK ImageWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	/* ������ ���� ��������� ����������� */
	class ImageWindow {
	private:
		/* ����� ����������� �� ���������� FreeImage */
		fipImage image;

		/* ���������� �� ����������� ��� ����������� � GDI */
		BITMAPINFO bmi;

		/* ����� ���� ��������� */
		HWND hWnd;

		/* �������� ������ ��������� */
		const wchar_t* ImageClassName = L"ImageWindow";

		/* ������� ����������� ������ ���� */
		bool RegisterImageWindowClass(HINSTANCE hInst) {
			WNDCLASS wc = { 0 };
			wc.hInstance = hInst;
			wc.lpfnWndProc = ImageWindowProc;
			wc.lpszClassName = ImageClassName;

			return RegisterClass(&wc);
		}
	public:
		/* ����������� ������ ��������� ����������� */
		ImageWindow(HINSTANCE hInst, const Image& isoImage, HWND hParentWnd = nullptr, INT x = 0, INT y = 0) {
			/* �� ����������� ���������� ����������� �������� ����������� � ��������� ��� � ����� ��� FreeImage */
			fipMemoryIO tempImage(isoImage.getRawImage(), isoImage.getRawImageSize());

			/* ��������� �� ������ ����������� */
			if (!image.loadFromMemory(tempImage)) {
				throw std::exception("������: �� ������� ��������� ����������");
			}

			/* ������� ����� */
			tempImage.close();

			/* ������������ ����������� � 24-� ������ ������ */
			/* ����� Windows ���� ���������� ��������� ��������� ����������� */
			if (!image.convertTo24Bits()) {
				throw std::exception("������: �� ������� �������������� ����������");
			}

			/* ������������ ������ �� ����������� */
			DWORD w = image.getWidth();
			DWORD h = image.getHeight();
			bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bmi.bmiHeader.biWidth = w;
			bmi.bmiHeader.biHeight = h;
			bmi.bmiHeader.biPlanes = 1;
			bmi.bmiHeader.biCompression = BI_RGB;
			bmi.bmiHeader.biBitCount = image.getBitsPerPixel();

			/* ����������� ������ ���� */
			if (!RegisterImageWindowClass(hInst)) {
				throw std::exception("������: �� ������� ���������������� ����� ���� ��� �����������");
			}

			/* ����������� ����� ���� ���������: ���� ���������������, ���� �������� */
			DWORD imageWindowStyle = WS_OVERLAPPEDWINDOW;
			if (hParentWnd) {
				imageWindowStyle = WS_CHILD | WS_OVERLAPPED;
			}

			/* ������ ���� */
			hWnd = CreateWindow(ImageClassName, L"", imageWindowStyle, x, y, w, h, hParentWnd, nullptr, hInst, nullptr);
			if (!hWnd) {
				throw std::exception("������: �� ������� ������� ����");
			}

			/* � ���������� ���������������� ������ ���� ���������� ��������� �� ����� */
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)this);

			/* ��������� ���� */
			UpdateWindow(hWnd);

			/* ������ ���� ������� */
			ShowWindow(hWnd, 1);

			/* ������� ����� ���� */
			UnregisterClass(ImageClassName, hInst);
		}

		/* ������� ��������� ����������� � ���� */
		bool reloadImage(const Image& isoImage) {
			/* ������� ������� ������ �������� ����������� */
			image.clear();

			/* ��������� �� ������ ������� � ������ FreeImage */
			fipMemoryIO tempImage(isoImage.getRawImage(), isoImage.getRawImageSize());

			/* ��������� �� ������ FreeImage � ����������� */
			if (!image.loadFromMemory(tempImage)) {
				return false;
			}

			/* �������� ������ FreeImage */
			tempImage.close();

			/* �������������� ����������� � 24-� ������ ������ */
			if (!image.convertTo24Bits()) {
				image.clear();
				return false;
			}

			/* ��������� ��������� ����������� ������ */
			DWORD w = image.getWidth();
			DWORD h = image.getHeight();
			bmi.bmiHeader.biWidth = w;
			bmi.bmiHeader.biHeight = h;
			bmi.bmiHeader.biBitCount = image.getBitsPerPixel();

			/* �������� ������ ���� ��� ����� ������ ����������� */
			MoveWindow(hWnd, 0, 0, w, h, false);

			/* ������� ����������� ���� */
			InvalidateRect(hWnd, nullptr, true);
			return true;
		}

		/* ������� ��������� ��������� ����, ������� ������ ��������� ���������� */
		LRESULT CALLBACK handleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
			switch (message) {
			case WM_PAINT:
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hWnd, &ps);

				/* ����������� �� ���� ����� ������ */
				FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));

				/* ������� ����������� */
				SetDIBitsToDevice(hdc, 0, 0, image.getWidth(), image.getHeight(), 0, 0, 0, image.getHeight(), image.accessPixels(), &bmi, DIB_RGB_COLORS);
				EndPaint(hWnd, &ps);
				return 0;
			}

			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	};

	/* ������� ��������� ��������� ����, ����� ��� ���� ImageWindow */
	LRESULT CALLBACK ImageWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
		/* �������� ��������� �� ImageWindow �� ���������������� ������ */
		ImageWindow* imageWindow = (ImageWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

		/* ������������ ����� ��������� */
		switch (message) {
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		}

		/* �� ������ ����, ������� �� �������� ��������� ��� ���, ����������, ���� ������ �������������� ��������� */
		return (imageWindow ? imageWindow->handleMessage(hWnd, message, wParam, lParam)
			: DefWindowProc(hWnd, message, wParam, lParam));
	}
}