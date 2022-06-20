#include <Windows.h>

#include "Crypto.hpp"
#include "CardReader.hpp"
#include "MRZDecoder.hpp"
#include "BerTLV.hpp"

#include <fstream>
using namespace std;

//INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, INT nCmdShow) {
int main() {
	srand(time(NULL));
	setlocale(LC_ALL, "rus");

	ifstream file("Datagroup2.bin", ios::binary | ios::ate);
	size_t file_size = file.tellg();
	vector<BYTE> fileData(file_size, 0);
	file.seekg(ios::beg);
	file.read((char*)fileData.data(), file_size);
	file.close();

	BerTLV ber = BerTLV::decodeBerTLV(fileData);
	auto root = ber.findChild(0x7f61);
	if (!root) {
		cerr << "Tag 0x7f61 not found" << endl;
		return 0;
	}

	auto group = root->findChild(0x7f60);
	if (!group) {
		cerr << "Tag 0x7f60 not found" << endl;
		return 0;
	}

	auto photo = group->findChild(0x5F2E);
	if (!photo) {
		cerr << "Tag 0x5F2E not found" << endl;
		return 0;
	}

	auto& photoData = photo->getValue();

	cout << "Encoded photo found!" << endl;
	ofstream outPhoto("EncodedPhoto.bin", ios::binary | ios::out);
	outPhoto.write((char*)photoData.data(), photoData.size());
	outPhoto.close();

	return 0;
}