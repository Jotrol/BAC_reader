#include <Windows.h>
#include <bcrypt.h>
#include "CardReader.hpp"
using namespace std;

#pragma comment(lib, "bcrypt.lib")
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

int hashingSample() {
	BYTE msgToHash[] = "Some message to get hash of";

	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	BCRYPT_ALG_HANDLE hAlg = nullptr;
	BCRYPT_HASH_HANDLE hHash = nullptr;

	DWORD cbData = 0;
	DWORD cbHash = 0;
	DWORD cbHashObject = 0;
	LPBYTE pbHashObject = nullptr;
	LPBYTE pbHash = nullptr;

	ntStatus = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA1_ALGORITHM, nullptr, 0);
	if (!NT_SUCCESS(ntStatus)) {
		cerr << "Ошибка: не удалось инициализировать алгоритм хэширования" << endl;
		return 1;
	}

	ntStatus = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (LPBYTE)&cbHashObject, sizeof(DWORD), &cbData, 0);
	if (!NT_SUCCESS(ntStatus)) {
		cerr << "Ошибка: нельзя получить размер хэширующего объекта" << endl;
		BCryptCloseAlgorithmProvider(hAlg, 0);
		return 1;
	}

	pbHashObject = new BYTE[cbHashObject];
	ntStatus = BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (LPBYTE)&cbHash, sizeof(DWORD), &cbData, 0);
	if (!NT_SUCCESS(ntStatus)) {
		cerr << "Ошибка: нельзя получить длину хэша" << endl;
		BCryptCloseAlgorithmProvider(hAlg, 0);
		delete[] pbHashObject;
		return 1;
	}

	pbHash = new BYTE[cbHash];
	ntStatus = BCryptCreateHash(hAlg, &hHash, pbHashObject, cbHashObject, nullptr, 0, 0);
	if (!NT_SUCCESS(ntStatus)) {
		cerr << "Ошибка: не удалось создать хэширующий алгоритм" << endl;
		BCryptCloseAlgorithmProvider(hAlg, 0);
		delete[] pbHashObject;
		delete[] pbHash;
		return 1;
	}

	ntStatus = BCryptHashData(hHash, msgToHash, sizeof(msgToHash) - 1, 0);
	if (!NT_SUCCESS(ntStatus)) {
		cerr << "Ошибка: не удалось захэшировать данные" << endl;
		BCryptCloseAlgorithmProvider(hAlg, 0);
		BCryptDestroyHash(hHash);
		delete[] pbHashObject;
		delete[] pbHash;
		return 1;
	}

	ntStatus = BCryptFinishHash(hHash, pbHash, cbHash, 0);
	if (!NT_SUCCESS(ntStatus)) {
		cerr << "Ошибка: не удалось завершить хэширование данных" << endl;
		BCryptCloseAlgorithmProvider(hAlg, 0);
		BCryptDestroyHash(hHash);
		delete[] pbHashObject;
		delete[] pbHash;
		return 1;
	}

	for (int i = 0; i < cbHash; i += 1) {
		cout << hex << (unsigned int)pbHash[i];
	}

	BCryptCloseAlgorithmProvider(hAlg, 0);
	BCryptDestroyHash(hHash);
	delete[] pbHashObject;
	delete[] pbHash;
}
int encryptingSample() {
	const BYTE plaintext[] = "0123456789ABCDEF";
	const INT plaintextLen = sizeof(plaintext) - 1;

	const BYTE DESkey[] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF};
	const INT DESkeyLen = sizeof(DESkey);

	BCRYPT_ALG_HANDLE hTDES = BCRYPT_3DES_112_CBC_ALG_HANDLE;
	BCRYPT_KEY_HANDLE hKey = nullptr;

	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;

	DWORD cbKeyObject = 0;
	DWORD cbBlockLen = 0;
	DWORD cbBlob = 0;
	DWORD cbData = 0;
	DWORD cbCipherText = 0;

	LPBYTE pbCipherText = nullptr;
	LPBYTE pbPlainText = nullptr;
	LPBYTE pbKeyObject = nullptr;
	LPBYTE pbIV = nullptr;
	LPBYTE pbBlob = nullptr;

	ntStatus = BCryptGetProperty(hTDES, BCRYPT_OBJECT_LENGTH, (LPBYTE)&cbKeyObject, sizeof(DWORD), &cbData, 0);
	if (!NT_SUCCESS(ntStatus)) {
		cerr << "Ошибка: не удалось получить размер объекта" << endl;
		return 1;
	}

	ntStatus = BCryptGetProperty(hTDES, BCRYPT_BLOCK_LENGTH, (LPBYTE)&cbBlockLen, sizeof(DWORD), &cbData, 0);
	if (!NT_SUCCESS(ntStatus)) {
		cerr << "Ошибка: не удалось получить размер блока" << endl;
		return 1;
	}
	
	pbKeyObject = new BYTE[cbKeyObject];
	pbIV = new BYTE[cbBlockLen]{ 0 };

	ntStatus = BCryptGenerateSymmetricKey(hTDES, &hKey, pbKeyObject, cbKeyObject, (LPBYTE)DESkey, DESkeyLen, 0);
	if (!NT_SUCCESS(ntStatus)) {
		cerr << "Ошибка: не удалось сгенерировать симметричный ключ" << endl;
		delete[] pbKeyObject;
		delete[] pbIV;
		return 1;
	}

	pbPlainText = new BYTE[plaintextLen];
	memcpy(pbPlainText, plaintext, plaintextLen);

	ntStatus = BCryptEncrypt(hKey, pbPlainText, plaintextLen, nullptr, pbIV, cbBlockLen, nullptr, 0, &cbCipherText, BCRYPT_BLOCK_PADDING);
	if (!NT_SUCCESS(ntStatus)) {
		cerr << "Ошибка: не удалось выполнить определение размера зашифрованных данных" << endl;
		BCryptDestroyKey(hKey);
		delete[] pbKeyObject;
		delete[] pbIV;
		delete[] pbPlainText;
		return 1;
	}

	pbCipherText = new BYTE[cbCipherText];
	ntStatus = BCryptEncrypt(hKey, pbPlainText, plaintextLen, nullptr, pbIV, cbBlockLen, pbCipherText, cbCipherText, &cbData, BCRYPT_BLOCK_PADDING);
	if (!NT_SUCCESS(ntStatus)) {
		cerr << "Ошибка: не удалось выполнить шифрование данных" << endl;
		BCryptDestroyKey(hKey);
		delete[] pbKeyObject;
		delete[] pbIV;
		delete[] pbPlainText;
		delete[] pbCipherText;
		return 1;
	}

	for (int i = 0; i < cbCipherText; i += 1) {
		cout << hex << (unsigned int)pbCipherText[i];
	}
	cout << endl;

	BCryptDestroyKey(hKey);
	delete[] pbKeyObject;
	delete[] pbIV;
	delete[] pbPlainText;
	delete[] pbCipherText;
	return 0;
}

//INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, INT nCmdShow) {
int main() {
	setlocale(LC_ALL, "rus");
	encryptingSample();
	
	return 0;
}