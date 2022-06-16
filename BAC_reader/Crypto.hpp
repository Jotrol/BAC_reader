#pragma once

#define WIN_LEAN_AND_MEAN
#include <Windows.h>
#include <bcrypt.h>
#include <vector>
#include <exception>
#include <iostream>
#include <memory>
#pragma comment(lib, "bcrypt.lib")

#define BCRYPT_SUCCESS(status) (status >= 0)

namespace Crypto {
	/* ������ ��� ����� �������� ���������� ���������� ���� �� ��������� SHA-1 */
	class SHA1 {
	private:
		/* ���������� ��������� */
		BCRYPT_ALG_HANDLE hAlg;

		/* ���������� ��������� ���� */
		BCRYPT_HASH_HANDLE hHash;

		/* ������ ��������� ���� */
		DWORD cbHashObject;

		/* ����� ���� */
		DWORD cbHashSize;

		/* ���������� ��� �������� � ������� � ��������� �������� */
		DWORD cbData;

		/* ������ ��� �������� ��������� ���� */
		std::unique_ptr<BYTE> pbHashObject;
	public:

		/* ����������� */
		SHA1() : hAlg(nullptr), hHash(nullptr), pbHashObject() {
			/* �������� ���������� ��������� - ��������������� ��� */
			NTSTATUS ntStatus = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA1_ALGORITHM, nullptr, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw std::exception("������: �� ������� ���������������� �������� �����������");
			}

			/* �������� ������ ��������� ���� */
			ntStatus = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (LPBYTE)&cbHashObject, sizeof(DWORD), &cbData, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw std::exception("������: ������ �������� ������ ����������� �������");
			}

			/* �������� ����� ����, ��������������� ���������� */
			ntStatus = BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (LPBYTE)&cbHashSize, sizeof(DWORD), &cbData, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw std::exception("������: �� ������� �������� ����� ����");
			}

			/* �������� ������ ��� �������� ���� */
			pbHashObject.reset(new BYTE[cbHashObject]);

			/* �������������� �������� ���� */
			this->clearHash();
		}

		/* ������� ������� ��������� ���� */
		void clearHash() {
			/* ������� ������ �������� */
			BCryptDestroyHash(hHash);

			/* �������� ����� � �� �� ������� ������, ��� � ��� ������ */
			NTSTATUS ntStatus = BCryptCreateHash(hAlg, &hHash, pbHashObject.get(), cbHashObject, nullptr, 0, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw std::exception("������: �� ������� ������� ���������� ��������");
			}
		}

		/* ������� ���������� ���� �� ���������� ������ � �� ������� */
		std::vector<BYTE> hash(LPBYTE data, INT dataSize) {
			/* ���������� ����������� */
			NTSTATUS ntStatus = BCryptHashData(hHash, data, dataSize, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				std::cerr << "�� ������� ������������ ������" << std::endl;
				return {};
			}

			/* �������� ������ ��� ������ ���� */
			/* ������ ����������� ��� ������������ � �������� ���������� ������� */
			std::vector<BYTE> hashedData(cbHashSize, 0);

			/* ��������� ������ ���� � ��������� ��������� � ���������� ����� ������� */
			ntStatus = BCryptFinishHash(hHash, hashedData.data(), cbHashSize, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				std::cerr << "�� ������� ��������� �����������" << std::endl;
				return {};
			}

			/* ���������� �� ������� ��� � ������� */
			return hashedData;
		}

		/* ���������� ��� ������ */
		~SHA1() {
			/* ��������� �������� */
			BCryptCloseAlgorithmProvider(hAlg, 0);

			/* ������� �������� ����; ������ �� ��� ���� ��������� ������������� */
			BCryptDestroyHash(hHash);
		}
	};
}

/* �������� �� "��������������� ������ ������" */
/* ����� ��������� ��� �������� �� 7 �����, ������� � ����� ��� ���� �������� */
unsigned char genParityBit(unsigned char x) {
	unsigned char y = x ^ (x >> 1);
	y = y ^ (y >> 2);
	y = y ^ (y >> 4);
	y = y ^ (y >> 7);
	return (x & 0xFE) | ((y >> 1) & 1);
}
int encryptingSample() {
	const BYTE plaintext[] = "0123456789ABCDEF";
	const INT plaintextLen = sizeof(plaintext) - 1;

	BYTE DESkey[] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF };
	const INT DESkeyLen = sizeof(DESkey);
	for (int i = 0; i < DESkeyLen; i += 1) {
		DESkey[i] = genParityBit(DESkey[i]);
	}

	BCRYPT_ALG_HANDLE hTDES = BCRYPT_3DES_112_CBC_ALG_HANDLE;
	BCRYPT_KEY_HANDLE hKey = nullptr;

	NTSTATUS ntStatus = -1;

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
	if (!BCRYPT_SUCCESS(ntStatus)) {
		cerr << "������: �� ������� �������� ������ �������" << endl;
		return 1;
	}

	ntStatus = BCryptGetProperty(hTDES, BCRYPT_BLOCK_LENGTH, (LPBYTE)&cbBlockLen, sizeof(DWORD), &cbData, 0);
	if (!BCRYPT_SUCCESS(ntStatus)) {
		cerr << "������: �� ������� �������� ������ �����" << endl;
		return 1;
	}

	pbKeyObject = new BYTE[cbKeyObject];
	pbIV = new BYTE[cbBlockLen]{ 0 };

	ntStatus = BCryptGenerateSymmetricKey(hTDES, &hKey, pbKeyObject, cbKeyObject, (LPBYTE)DESkey, DESkeyLen, 0);
	if (!BCRYPT_SUCCESS(ntStatus)) {
		cerr << "������: �� ������� ������������� ������������ ����" << endl;
		delete[] pbKeyObject;
		delete[] pbIV;
		return 1;
	}

	pbPlainText = new BYTE[plaintextLen];
	memcpy(pbPlainText, plaintext, plaintextLen);

	ntStatus = BCryptEncrypt(hKey, pbPlainText, plaintextLen, nullptr, pbIV, cbBlockLen, nullptr, 0, &cbCipherText, BCRYPT_BLOCK_PADDING);
	if (!BCRYPT_SUCCESS(ntStatus)) {
		cerr << "������: �� ������� ��������� ����������� ������� ������������� ������" << endl;
		BCryptDestroyKey(hKey);
		delete[] pbKeyObject;
		delete[] pbIV;
		delete[] pbPlainText;
		return 1;
	}

	pbCipherText = new BYTE[cbCipherText];
	ntStatus = BCryptEncrypt(hKey, pbPlainText, plaintextLen, nullptr, pbIV, cbBlockLen, pbCipherText, cbCipherText, &cbData, BCRYPT_BLOCK_PADDING);
	if (!BCRYPT_SUCCESS(ntStatus)) {
		cerr << "������: �� ������� ��������� ���������� ������" << endl;
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