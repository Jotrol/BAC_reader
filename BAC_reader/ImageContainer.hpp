#pragma once

#include "Util.hpp"

#include <Windows.h>
#include "FreeImagePlus.h"
#pragma comment(lib, "FreeImagePlus.lib")

#include <fstream>
#include <vector>
using namespace std;

namespace ImageContainer {
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
		FaceImageHeader readFaceImageHeader(ByteStream& file) {
			FaceImageHeader faceImageHeader = { 0 };
			file.read((UINT8*)&faceImageHeader, sizeof(FaceImageHeader));

			faceImageHeader.entryLen = Util::changeEndiannes(faceImageHeader.entryLen);
			faceImageHeader.facesCount = Util::changeEndiannes(faceImageHeader.facesCount);

			return faceImageHeader;
		}
		FaceInformation readFaceInformation(ByteStream& file) {
			FaceInformation faceInfo = { 0 };
			file.read((UINT8*)&faceInfo, sizeof(FaceInformation));

			faceInfo.faceDataLen = Util::changeEndiannes(faceInfo.faceDataLen);
			faceInfo.controlPointsCount = Util::changeEndiannes(faceInfo.controlPointsCount);
			faceInfo.faceLook = Util::changeEndiannes(faceInfo.faceLook);

			return faceInfo;
		}
		vector<ControlPoint> readControlPoints(ByteStream& file, UINT16 controlPointsCount) {
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
		ImageInfo readImageInfo(ByteStream& file) {
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
		Image_ISO19794_5_2006(ByteStream& file) {
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
			file.read((UINT8*)imgData.get(), imgDataSize);
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
}