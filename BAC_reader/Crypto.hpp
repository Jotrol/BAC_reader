#pragma once

#define WIN_LEAN_AND_MEAN
#include <Windows.h>
#include <bcrypt.h>
#include <vector>
#include <exception>
#include <iostream>
#include <memory>
#include <functional>
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
	
	
	static enum SymmetricAlgName { DES, TripleDES };
	static enum SymmetricAlgMode { ECB, CBC };
	static enum SymmetricAlgActions { ENCRYPT, DECRYPT };

	template<SymmetricAlgName algName, SymmetricAlgMode algMode>
	class SymmetricEncryptionAlgorithm {
	private:
		/* ���������� ��������� */
		BCRYPT_ALG_HANDLE hAlg;

		/* ������ ��������� ����� */
		DWORD cbKeyObject;

		/* ������ ����� ���������� */
		DWORD cbBlockLen;

		/* ���������� ��� �������� �� ������� */
		DWORD cbData;

		/* ��������� �� ������ ��� ��������� ����� */
		std::unique_ptr<BYTE> pbKeyObject;

		/* ������� ����������/������������� */
		std::function<NTSTATUS(BCRYPT_KEY_HANDLE, PUCHAR, ULONG, void*, PUCHAR, ULONG, PUCHAR, ULONG, ULONG* pcbResult, ULONG)> algFunctions[2];

		const static LPCWSTR pszAlgName[] = { BCRYPT_DES_ALGORITHM, BCRYPT_3DES_112_ALGORITHM };
		const static LPCWSTR pszAlgMode[] = { BCRYPT_CHAIN_MODE_ECB, BCRYPT_CHAIN_MODE_CBC };
	public:
		
		
		SymmetricEncryptionAlgorithm() {
			/* �������� ���������� ��������� ���������� */
			NTSTATUS ntStatus = BCryptOpenAlgorithmProvider(&hAlg, pszAlgName[algName], nullptr, 0);
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
			ntStatus = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (LPBYTE)pszAlgMode[algMode], sizeof(pszAlgMode[algMode]), 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw std::exception("������: �� ������� �������� ��� ���������� ������ ����������");
			}

			/* �������� ������ ��� ��������� ����� */
			pbKeyObject.reset(new BYTE[cbKeyObject]);

			/* ��������� ������������� ������ �������������/����������, ����� ��� ����� ���� ������� */
			/* �� ���������� ������� �������� */
			algFunctions[AlgActions::ENCRYPT] = BCryptEncrypt;
			algFunctions[AlgActions::DECRYPT] = BCryptDecrypt;
		}

		std::vector<BYTE> enc_dec(const std::vector<BYTE>& data, const std::vector<BYTE>& key, AlgActions algAction) {
			/* ���������� ���� ����������; ������ �����, ����� �� �������� ������ */
			std::vector<BYTE> keyCopy(key);
			
			BCRYPT_KEY_HANDLE hKey = nullptr;
			NTSTATUS ntStatus = BCryptGenerateSymmetricKey(hAlg, &hKey, pbKeyObject.get(), cbKeyObject, keyCopy.data(), keyCopy.size(), 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				std::cerr << "������: �� ������� ������������� ���� ����������" << std::endl;
				return {};
			}

			/* ������ ��������� ������ �������������, ����������� ������ */
			std::unique_ptr<BYTE> pbIV(algMode == AlgMode::ECB ? nullptr : new BYTE[cbBlockLen]{0});

			/* ������ ������ ��� �������� ������ ������ */
			DWORD cbOutput = 0;
			std::vector<BYTE> dataCopy(data);
			ntStatus = algFunctions[algAction](hKey, dataCopy.data(), dataCopy.size(), nullptr, pbIV.get(), cbBlockLen, nullptr, 0, &cbOutput, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				std::cerr << "������: �� ������� ������ ������ ������������� ������" << std::endl;
				BCryptDestroyKey(hKey);
				return {};
			}

			/* ��������� ��������������� ����������/������������ */
			std::vector<BYTE> pbOutput(cbOutput, 0);
			ntStatus = algFunctions[algAction](hKey, dataCopy.data(), dataCopy.size(), nullptr, pbIV.get(), cbBlockLen, pbOutput.data(), cbOutput, &cbData, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				std::cerr << "������: �� ������� ��������� ������������" << std::endl;
				BCryptDestroyKey(hKey);
				return {};
			}

			BCryptDestroyKey(hKey);
			return pbOutput;
		}

		~SymmetricEncryptionAlgorithm() {
			BCryptCloseAlgorithmProvider(hAlg, 0);
		}
	};

	class RetailMAC {
	private:
		/* ���������� ��������� DES */
		BCRYPT_ALG_HANDLE hAlg;

		/* ���������� ����� DES */
		BCRYPT_KEY_HANDLE hKey;

		/* ������ ��������� ����� */
		DWORD cbKeyObject;

		/* ������ ����� ���������� */
		DWORD cbBlockLen;

		/* ������������ �������� �� ������� Bcrypt */
		DWORD cbData;

		/* ���� ������ ������� ����� */
		std::unique_ptr<BYTE> pbKeyObject;
		
		/* ���� �� �������� ������ �����, ��� ������������ ��� ������ ������� ������ ~100 ����� ��� ���������� � ������������ � ���� ������� */
		enum DES_ACTION {ENCRYPT, DECRYPT};
		std::function<NTSTATUS(BCRYPT_KEY_HANDLE, PUCHAR, ULONG, void*, PUCHAR, ULONG, PUCHAR, ULONG, ULONG* pcbResult, ULONG)> desActions[2];

		/* ������� ����������/������������, ����� ������� ������������� */
		std::vector<BYTE> DES_enc_dec(std::vector<BYTE> data, std::vector<BYTE> key, DES_ACTION desAction) {
			/* ���������� ���� ��� ����������/����������� */
			NTSTATUS ntStatus = BCryptGenerateSymmetricKey(hAlg, &hKey, pbKeyObject.get(), cbKeyObject, key.data(), key.size(), 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				std::cerr << "������: �� ������� ������������� ���� ����������" << std::endl;
				return {};
			}

			/* ������ ������ ��� �������� ������ ������ */
			DWORD cbOutput = 0;
			std::vector<BYTE> dataCopy(data);
			ntStatus = desActions[desAction](hKey, dataCopy.data(), dataCopy.size(), nullptr, nullptr, cbBlockLen, nullptr, 0, &cbOutput, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				std::cerr << "������: �� ������� ������ ������ ������������� ������" << std::endl;
				return {};
			}

			/* ��������� ��������������� ����������/������������ */
			std::vector<BYTE> pbOutput(cbOutput, 0);
			ntStatus = desActions[desAction](hKey, dataCopy.data(), dataCopy.size(), nullptr, nullptr, cbBlockLen, pbOutput.data(), cbOutput, &cbData, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				std::cerr << "������: �� ������� ��������� ������������" << std::endl;
				return {};
			}

			/* ����� ��������� */
			return pbOutput;
		}
	public:
		RetailMAC() : hAlg(nullptr), hKey(nullptr), pbKeyObject() {
			/* �������� ���������� ��������� ���������� */
			NTSTATUS ntStatus = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_DES_ALGORITHM, nullptr, 0);
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
			ntStatus = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (LPBYTE)BCRYPT_CHAIN_MODE_ECB, sizeof(BCRYPT_CHAIN_MODE_ECB), 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw std::exception("������: �� ������� �������� ��� ���������� ������ ����������");
			}

			/* �������� ������ ��� ��������� ����� */
			pbKeyObject.reset(new BYTE[cbKeyObject]);

			/* ������ ���, � �� ��������� ��� ���������� ������, ��� ENCRYPT � DECRYPT ���� ��� �������� ������� */
			/* �� ����� ������, ��� � ������� ���� ������� �������� ����. ��� ��� ��������� ������ ���� ����������� ������� */
			desActions[DES_ACTION::ENCRYPT] = BCryptEncrypt;
			desActions[DES_ACTION::DECRYPT] = BCryptDecrypt;
		}

		/* ��������� MAC �������� */
		std::vector<BYTE> getMAC(const std::vector<BYTE>& data, const std::vector<BYTE>& key) {
			/* ���� ������ ����� �� ����� 16 ������, �� ���� */
			if (key.size() != 16) {
				std::cerr << "������: ����� ����� ��� Retail MAC ������ ���� ����� 16 ������" << std::endl;
				return {};
			}
			
			/* �������� �������� ������ � ���������� ������ */
			std::vector<BYTE> dataCopy(data);

			/* ��������� �� ������ ���������� 2 ��������� ���/��� 9797-1 */
			dataCopy.push_back(128); /* ��������� ��������� ��� � ����� ������, 128 = 0b10000000 */

			/* ��������� ������ ���� �� ������ ������� ����� */
			while (dataCopy.size() % cbBlockLen != 0) {
				dataCopy.push_back(0);
			}

			/* �������� ������, ����� ���������� � ���������� MAC */
			/* ������� ������� ��� �������� */
			std::vector<BYTE> key0(key.begin(), key.begin() + cbBlockLen);
			std::vector<BYTE> key1(key.begin() + cbBlockLen, key.begin() + 2 * cbBlockLen);

			/* ����� �������� ������� ������ ��� ��������� ����� */
			std::vector<BYTE> block(cbBlockLen, 0);
			std::vector<BYTE> output(cbBlockLen, 0);

			/* � ����� �������� ������� ���, ������� ��������� �������� ����� � ������� ����������� ������ */
			for (size_t i = 0; i < dataCopy.size() / cbBlockLen; i += 1) {
				/* �������� ����� ������ � ���� */
				auto dataBlockBegin = dataCopy.data() + i * cbBlockLen;
				memcpy(block.data(), dataBlockBegin, cbBlockLen);

				/* ��������� XOR ���������� ����� ������ � ��������� ����� */
				for (size_t j = 0; j < block.size(); j += 1) {
					output[j] = output[j] ^ block[j];
				}

				/* ��������� �����, ����� ��������� ���� ���������� */
				output = DES_enc_dec(output, key0, DES_ACTION::ENCRYPT);
			}

			/* ����� ���������� ���� ������ ����������, ����������� ���� */
			output = DES_enc_dec(output, key1, DES_ACTION::DECRYPT);
			output = DES_enc_dec(output, key0, DES_ACTION::ENCRYPT);

			/* � ������� �������� ������ */
			return output;
		}
	};
}

/* ����� �� �������������� ���������� 6 ����� 1 ���� 2 ��������� 9303 */
#ifdef TEST_PARITY
#include <cassert>

void testParityCorrect() {
	/* ������-�������, ������� ��������� ������������ �� ���������� ������������������ ����� � ��������� */
	auto testParity = [](std::vector<BYTE> key, std::vector<BYTE> expected) {
		for (size_t i = 0; i < key.size(); i += 1) {
			key[i] = Crypto::genByteWithParityBit(key[i]);
		}
		assert(key == expected);
	};

	/* ���� ������� �� �������� ������ */
	testParity({ 0xAB, 0x94, 0xFC, 0xED, 0xF2, 0x66, 0x4E, 0xDF }, { 0xAB, 0x94, 0xFD, 0xEC, 0xF2, 0x67, 0x4F, 0xDF });
	testParity({ 0xB9, 0xB2, 0x91, 0xF8, 0x5D, 0x7F, 0x77, 0xF2 }, { 0xB9, 0xB3, 0x91, 0xF8, 0x5D, 0x7F, 0x76, 0xF2 });
	testParity({ 0x78, 0x62, 0xD9, 0xEC, 0xE0, 0x3C, 0x1B, 0xCD }, { 0x79, 0x62, 0xD9, 0xEC, 0xE0, 0x3D, 0x1A, 0xCD });
	testParity({ 0x4D, 0x77, 0x08, 0x9D, 0xCF, 0x13, 0x14, 0x42 }, { 0x4C, 0x76, 0x08, 0x9D, 0xCE, 0x13, 0x15, 0x43 });

	std::cout << "�������� ������������� ����� �������� ��������� �������!" << std::endl;
}

#endif