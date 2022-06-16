#pragma once

#define WIN_LEAN_AND_MEAN
#include <Windows.h>
#include <bcrypt.h>
#include <vector>
#include <exception>
#include <iostream>
#include <memory>
#pragma comment(lib, "bcrypt.lib")

namespace Crypto {
	/* �������� �� "��������������� ������ ������" */
	/* ����� ��������� ��� �������� �� 7 �����, ������� � ����� ��� ���� �������� */
	/* ׸���� ���-�� ��������� ����� - 1, �������� - 0 */
	unsigned char genByteWithParityBit(unsigned char x) {
		/* ��������� ��������� �����, ��������� ��� �������� xor �� ����� �����������, ������������� - �� �����, ����� ���������� � �.� */
		unsigned char y = x ^ (x >> 1);
		y = y ^ (y >> 2);
		y = y ^ (y >> 4);

		/* ����� ����������� ��������� ��� ��������� �����, � �� ����������� */
		/* ���������� ������������� ��� (�� ��� xor'�� �� �����, ����� ���������� ����) */
		/* ����������� ��� � ���� ��� ������ � ��������� ��� ������������� �����*/
		return (x & 0xFE) | (~(y >> 1) & 1);
	}

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
				throw std::exception("������: �� ������� �������� ������ ����������� �������");
			}

			/* �������� ����� ����, ��������������� ���������� */
			ntStatus = BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (LPBYTE)&cbHashSize, sizeof(DWORD), &cbData, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw std::exception("������: �� ������� �������� ����� ����");
			}

			/* �������� ������ ��� �������� ���� */
			pbHashObject.reset(new BYTE[cbHashObject]);
		}

		/* ������� ���������� ���� �� ���������� ������ � �� ������� */
		std::vector<BYTE> hash(LPBYTE data, DWORD dataSize) {
			/* �������������� ����� ������� ���� */
			NTSTATUS ntStatus = BCryptCreateHash(hAlg, &hHash, pbHashObject.get(), cbHashObject, nullptr, 0, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw std::exception("������: �� ������� ������� ���������� ��������");
			}

			/* ���������� ����������� */
			ntStatus = BCryptHashData(hHash, data, dataSize, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				std::cerr << "������: �� ������� ������������ ������" << std::endl;
				return {};
			}

			/* �������� ������ ��� ������ ���� */
			/* ������ ����������� ��� ������������ � �������� ���������� ������� */
			std::vector<BYTE> hashedData(cbHashSize, 0);

			/* ��������� ������ ���� � ��������� ��������� � ���������� ����� ������� */
			ntStatus = BCryptFinishHash(hHash, hashedData.data(), cbHashSize, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				std::cerr << "������: �� ������� ��������� �����������" << std::endl;
				return {};
			}

			/* ���������� �� ������� ��� � ������� */
			return hashedData;
		}

		/* ���������� ������� ��� ������ � �������� */
		std::vector<BYTE> hash(std::vector<BYTE>& data) {
			return this->hash(data.data(), (DWORD)data.size());
		}

		/* ���������� ��� ������ */
		~SHA1() {
			/* ��������� �������� */
			BCryptCloseAlgorithmProvider(hAlg, 0);

			/* ������� �������� ����; ������ �� ��� ���� ��������� ������������� */
			BCryptDestroyHash(hHash);
		}
	};

	/* ������ ��� ����� �������� ���������� ���������� �� ��������� 3DES */
	class DES3 {
	private:
		/* ���������� ��������� */
		BCRYPT_ALG_HANDLE hAlg;

		/* ���������� ����� */
		BCRYPT_KEY_HANDLE hKey;

		/* ������ ��������� ����� */
		DWORD cbKeyObject;

		/* ������ ����� ���������� */
		DWORD cbBlockLen;

		/* ���������� ��� �������� �� ������� */
		DWORD cbData;

		/* ��������� �� ������ ��� ��������� ����� */
		std::unique_ptr<BYTE> pbKeyObject;
	public:
		/* �������������� ��������� �������� ������ */
		DES3() : hAlg(nullptr), hKey(nullptr), pbKeyObject() {
			/* �������� ���������� ��������� ���������� */
			NTSTATUS ntStatus = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_3DES_112_ALGORITHM, nullptr, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw std::exception("������: �� ������� ���������������� �������� ����������");
			}

			/* ������ ������ ��������� ��� ����� */
			ntStatus = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (LPBYTE)&cbKeyObject, sizeof(DWORD), &cbData, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw std::exception("������: �� ������� �������� ������ ���������� �������");
			}

			/* ������ ����� ����� ��������� ���������� */
			ntStatus = BCryptGetProperty(hAlg, BCRYPT_BLOCK_LENGTH, (LPBYTE)&cbBlockLen, sizeof(DWORD), &cbData, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw std::exception("������: �� ������� �������� ������ ����� ����������");
			}

			/* ������ ����� ���������� �� ���������� ������ (CBC) */
			ntStatus = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (LPBYTE)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw std::exception("������: �� ������� �������� ��� ���������� ������ ����������");
			}

			/* �������� ������ ��� ��������� ����� */
			pbKeyObject.reset(new BYTE[cbKeyObject]);
		}

		/* �������������� ������� ����������; ��������������, ��� ���� �������� ���� �������� */
		std::vector<BYTE> encryptData(LPBYTE pbKey, DWORD cbKey, LPBYTE pbData, DWORD cbData) {
			/* ���������� ���� ���������� */
			NTSTATUS ntStatus = BCryptGenerateSymmetricKey(hAlg, &hKey, pbKeyObject.get(), cbKeyObject, pbKey, cbKey, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				std::cerr << "������: �� ������� ������������� ���� ����������" << std::endl;
				return {};
			}

			/* ������ ��������� ������ �������������, ����������� ������ */
			std::unique_ptr<BYTE> pbIV(new BYTE[cbBlockLen]{ 0 });

			/* �������� ��������� ������ */
			DWORD cbPlainText = cbData;
			std::unique_ptr<BYTE> pbPlainText(new BYTE[cbData]);
			std::memcpy(pbPlainText.get(), pbData, cbData);

			/* ������ ������ ������������� ������ */
			DWORD cbCipherText = 0;
			ntStatus = BCryptEncrypt(hKey, pbPlainText.get(), cbPlainText, nullptr, pbIV.get(), cbBlockLen, nullptr, 0, &cbCipherText, BCRYPT_BLOCK_PADDING);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				std::cerr << "������: �� ������� ������ ������ ������������� ������" << std::endl;
				return {};
			}

			/* �������� ����� � ������ � ��������� ���������� */
			std::vector<BYTE> pbCipherText(cbCipherText, 0);
			ntStatus = BCryptEncrypt(hKey, pbPlainText.get(), cbPlainText, nullptr, pbIV.get(), cbBlockLen, pbCipherText.data(), cbCipherText, &cbData, BCRYPT_BLOCK_PADDING);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				std::cerr << "������: �� ������� ��������� ����������" << std::endl;
				return {};
			}

			return pbCipherText;
		}

		/* ������� ���������� ��� ������ � ������������ */
		std::vector<BYTE> encryptData(std::vector<BYTE> key, std::vector<BYTE> data) {
			return this->encryptData(key.data(), (DWORD)key.size(), data.data(), (DWORD)data.size());
		}

		/* ���������� */
		~DES3() {
			/* ��������� ��������� ���������� */
			BCryptCloseAlgorithmProvider(hAlg, 0);

			/* ������� ����; �������� ����� ����� ������ �������������� */
			BCryptDestroyKey(hKey);
		}
	};
}

/* ����� �� �������������� ���������� 6 ����� 1 ���� 2 ��������� 9303 */
#ifdef TEST_PARITY
#include <cassert>

void testParityCorrect1() {
	std::vector<BYTE> key = { 0xAB, 0x94, 0xFC, 0xED, 0xF2, 0x66, 0x4E, 0xDF };
	std::vector<BYTE> expected = { 0xAB, 0x94, 0xFD, 0xEC, 0xF2, 0x67, 0x4F, 0xDF };
	std::vector<BYTE> out(8, 0);

	for (size_t i = 0; i < key.size(); i += 1) {
		out[i] = Crypto::genByteWithParityBit(key[i]);
	}

	assert(out == expected);
}
void testParityCorrect2() {
	std::vector<BYTE> key = { 0xB9, 0xB2, 0x91, 0xF8, 0x5D, 0x7F, 0x77, 0xF2 };
	std::vector<BYTE> expected = { 0xB9, 0xB3, 0x91, 0xF8, 0x5D, 0x7F, 0x76, 0xF2 };
	std::vector<BYTE> out(8, 0);

	for (size_t i = 0; i < key.size(); i += 1) {
		out[i] = Crypto::genByteWithParityBit(key[i]);
	}

	assert(out == expected);
}
void testParityCorrect3() {
	std::vector<BYTE> key = { 0x78, 0x62, 0xD9, 0xEC, 0xE0, 0x3C, 0x1B, 0xCD };
	std::vector<BYTE> expected = { 0x79, 0x62, 0xD9, 0xEC, 0xE0, 0x3D, 0x1A, 0xCD };
	std::vector<BYTE> out(8, 0);

	for (size_t i = 0; i < key.size(); i += 1) {
		out[i] = Crypto::genByteWithParityBit(key[i]);
	}

	assert(out == expected);
}
void testParityCorrect4() {
	std::vector<BYTE> key = { 0x4D, 0x77, 0x08, 0x9D, 0xCF, 0x13, 0x14, 0x42 };
	std::vector<BYTE> expected = { 0x4C, 0x76, 0x08, 0x9D, 0xCE, 0x13, 0x15, 0x43 };
	std::vector<BYTE> out(8, 0);

	for (size_t i = 0; i < key.size(); i += 1) {
		out[i] = Crypto::genByteWithParityBit(key[i]);
	}

	assert(out == expected);
}
#endif