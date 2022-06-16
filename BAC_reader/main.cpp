#include <Windows.h>

#define TEST_PARITY
#include "Crypto.hpp"
#include "CardReader.hpp"
using namespace std;

int hashingSample() {
	BYTE msgToHash[] = "Some message to get hash of";

	Crypto::SHA1 sha1;
	vector<BYTE> pbHash1 = sha1.hash(msgToHash, sizeof(msgToHash) - 1);
	for (int i = 0; i < pbHash1.size(); i += 1) {
		cout << hex << (unsigned int)pbHash1[i];
	}
	cout << endl;

	vector<BYTE> pbHash2 = sha1.hash(msgToHash, sizeof(msgToHash) - 2);
	for (int i = 0; i < pbHash2.size(); i += 1) {
		cout << hex << (unsigned int)pbHash2[i];
	}
	cout << endl;

	return 1;
}

int encryptingSample() {
	BYTE plaintext[] = "0123456789ABCDEF";
	DWORD plaintextLen = sizeof(plaintext) - 1;

	BYTE DESkey[] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF };
	int DESkeyLen = sizeof(DESkey);
	for (int i = 0; i < DESkeyLen; i += 1) {
		DESkey[i] = Crypto::genByteWithParityBit(DESkey[i]);
	}

	Crypto::DES3 des;
	std::vector<BYTE> cipher = des.encryptData(DESkey, DESkeyLen, plaintext, plaintextLen);

	for (int i = 0; i < cipher.size(); i += 1) {
		std::cout << std::hex << (int)cipher[i];
	}
	std::cout << std::endl;

	return 0;
}

//INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, INT nCmdShow) {
int main() {
	setlocale(LC_ALL, "rus");

	hashingSample();
	encryptingSample();
	testParityCorrect();
	
	return 0;
}