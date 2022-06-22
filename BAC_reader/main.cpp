#include <Windows.h>

#include "Crypto.hpp"
#include "CardReader.hpp"
#include "MRZDecoder.hpp"
#include "BerTLV.hpp"

#include <fstream>
#include <sstream>
using namespace std;

//INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, INT nCmdShow) {
int main() {
	setlocale(LC_ALL, "RUS");

	/* Открываем поток байтов и узнаем его размер */
	ifstream file("Datagroup2.bin", ios::binary | ios::ate);
	UINT32 fileSize = file.tellg();
	file.seekg(0);

	/* Выполняем декодирование */
	BerTLV::BerTLVDecoder decoder;
	UINT16 rootIndex = decoder.decode(file);

	/* Выполняем поиск требуемых тегов */
	UINT16 tag0x7F61 = decoder.getChildByTag(rootIndex, 0x7F61);
	if (tag0x7F61 == 0) {
		cerr << "Нет тега 0x7F61" << endl;
		return 1;
	}

	UINT16 tag0x7F60 = decoder.getChildByTag(tag0x7F61, 0x7F60);
	if (tag0x7F60 == 0) {
		cerr << "Нет тега 0x7F60" << endl;
		return 1;
	}

	UINT16 tag0x5F2E = decoder.getChildByTag(tag0x7F60, 0x5F2E);
	if (tag0x5F2E == 0) {
		cerr << "Нет тега 0x5F2E" << endl;
		return 1;
	}

	BerTLV::BerTLVDecoderToken* imageToken = decoder.getTokenPtr(tag0x5F2E);
	auto dataImage = imageToken->getData(file);
	
	ofstream dataFile("data.bin", ios::binary);
	dataFile.write((char*)dataImage.get(), imageToken->getDataLen());
	dataFile.close();

	return 0;
}