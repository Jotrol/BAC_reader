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

//int encryptingSample() {
//	BYTE plaintext[] = "0123456789ABCDEF";
//	DWORD plaintextLen = sizeof(plaintext) - 1;
//
//	BYTE DESkey[] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF };
//	int DESkeyLen = sizeof(DESkey);
//	for (int i = 0; i < DESkeyLen; i += 1) {
//		DESkey[i] = Crypto::genByteWithParityBit(DESkey[i]);
//	}
//
//	Crypto::DES3 des;
//	std::vector<BYTE> cipher = des.encryptData(DESkey, DESkeyLen, plaintext, plaintextLen);
//
//	for (int i = 0; i < cipher.size(); i += 1) {
//		std::cout << std::hex << (int)cipher[i];
//	}
//	std::cout << std::endl;
//
//	return 0;
//}

//INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, INT nCmdShow) {
int main() {
	setlocale(LC_ALL, "rus");

	//hashingSample();
	//encryptingSample();
	//testParityCorrect();

	std::vector<BYTE> key_enc = { 0xAB, 0x94, 0xFD, 0xEC, 0xF2, 0x67, 0x4F, 0xDF, 0xB9, 0xB3, 0x91, 0xF8, 0x5D, 0x7F, 0x76, 0xF2 };
	std::vector<BYTE> data =    { 0x78, 0x17, 0x23, 0x86, 0x0C, 0x06, 0xC2, 0x26, 0x46, 0x08, 0xF9, 0x19, 0x88, 0x70, 0x22, 0x12, 0x0B, 0x79, 0x52, 0x40, 0xCB, 0x70, 0x49, 0xB0, 0x1C, 0x19, 0xB3, 0x3E, 0x32, 0x80, 0x4F, 0x0B };

	std::vector<BYTE> key_mac = { 0x79, 0x62, 0xD9, 0xEC, 0xE0, 0x3D, 0x1A, 0xCD, 0x4C, 0x76, 0x08, 0x9D, 0xCE, 0x13, 0x15, 0x43 };

	Crypto::SymmetricEncryptionAlgorithm<Crypto::SymmetricAlgName::TripleDES, Crypto::SymmetricAlgMode::CBC> des;
	auto cipher = des.enc_dec(data, key_enc, Crypto::SymmetricAlgActions::ENCRYPT);
	cipher.resize(32);
	for (int i = 0; i < cipher.size(); i += 1) {
		std::cout << std::hex << (int)cipher[i];
	}
	std::cout << std::endl;

	//Crypto::DES3 des;
	//auto cipher = des.encryptData(key_enc, data);
	//cipher.resize(32);

	//Crypto::MAC_DES mac;
	//auto mac_cipher = mac.getMAC(cipher, key_mac);
	//for (int i = 0; i < mac_cipher.size(); i += 1) {
	//	std::cout << std::hex << (int)mac_cipher[i];
	//}
	//std::cout << std::endl;
	
	return 0;
}