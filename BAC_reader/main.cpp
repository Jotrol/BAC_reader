#include <Windows.h>

#include "Crypto.hpp"
#include "CardReader.hpp"
#include "MRZDecoder.hpp"
#include "FreeImage.h"
#pragma comment(lib, "FreeImage.lib")
using namespace std;


//INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, INT nCmdShow) {
int main() {
	srand(time(NULL));
	setlocale(LC_ALL, "rus");

	FreeImage_Initialise();
	FIBITMAP* bitmap = FreeImage_Allocate(640, 480, 24);
	RGBQUAD color = { 0 };
	for (int i = 0; i < 640; i += 1) {
		for (int j = 0; j < 480; j += 1) {
			color.rgbRed = 0;
			color.rgbGreen = (double)i / 640 * 255.0;
			color.rgbBlue = (double)j / 480 * 255.0;
			FreeImage_SetPixelColor(bitmap, i, j, &color);
		}
	}

	FreeImage_Save(FIF_PNG, bitmap, "filename.png", 0);
	FreeImage_DeInitialise();

	return 0;
}