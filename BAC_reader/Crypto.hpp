#pragma once

#define WIN_LEAN_AND_MEAN
#include <Windows.h>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

#include <map>
#include <vector>
#include <memory>
#include <iostream>
#include <exception>
#include <functional>
using namespace std;

namespace Crypto {
	/* �������� �� "��������������� ������ ������" */
	/* ����� ��������� ��� �������� �� 7 �����, ������� � ����� ��� ���� �������� */
	/* ׸���� ���-�� ��������� ����� - 1, �������� - 0 */
	unsigned char getByteWithParityBit(unsigned char x) {
		/* ��������� ��������� �����, ��������� ��� �������� xor �� ����� �����������, ������������� - �� �����, ����� ���������� � �.� */
		unsigned char y = x ^ (x >> 1);
		y = y ^ (y >> 2);
		y = y ^ (y >> 4);

		/* ����� ����������� ��������� ��� ��������� �����, � �� ����������� */
		/* ���������� ������������� ��� (�� ��� xor'�� �� �����, ����� ���������� ����) */
		/* ����������� ��� � ���� ��� ������ � ��������� ��� ������������� �����*/
		return (x & 0xFE) | (~(y >> 1) & 1);
	}

	/* ������� ��� ��������� ����� �������� ����������� ������� */
	vector<BYTE> correctParityBits(const vector<BYTE>& data) {
		/* �������� �������� ������ */
		vector<BYTE> dataCopy(data);

		/* ������������ ���� �������� ������� ����� */
		for (size_t i = 0; i < dataCopy.size(); i += 1) {
			dataCopy[i] = getByteWithParityBit(dataCopy[i]);
		}

		/* ���������� ����� ������ */
		return dataCopy;
	}

	/* ������� ���������� �� ������ ���������� 2 ��������� ���/��� 9797-1 */
	vector<BYTE> fillISO9797(const vector<BYTE>& data, int blockLen) {
		/* �������� �������� ������ � ���������� ������ */
		vector<BYTE> dataCopy(data);

		/* ��������� �� ������ ���������� 2 ��������� ���/��� 9797-1 */
		dataCopy.push_back(0x80); /* ��������� ��������� ��� � ����� ������, 128 = 0b10000000 */

		/* ��������� ������ ���� �� ������ ������� ����� */
		while (dataCopy.size() % blockLen != 0) {
			dataCopy.push_back(0);
		}

		return dataCopy;
	}

	const enum HashAlgName { SHA1 };
	const map<HashAlgName, LPCWSTR> HashingAlgNameStr = {
		{ HashAlgName::SHA1, BCRYPT_SHA1_ALGORITHM }
	};

	/* �����, ����������� ����������� */
	template<HashAlgName hashAlgName>
	class HashingAlgorithm {
	protected:
		/* ���������� ��������� */
		BCRYPT_ALG_HANDLE hHashAlg;

		/* ���������� ��������� ���� */
		BCRYPT_HASH_HANDLE hHash;

		/* ������ ��������� ���� */
		DWORD cbHashObject;

		/* ����� ���� */
		DWORD cbHashSize;

		/* ���������� ��� �������� � ������� � ��������� �������� */
		DWORD cbDataHash;
	public:
		HashingAlgorithm() : hHash(nullptr) {
			/* �������� ��� ��������� �� ������� */
			LPCWSTR algName = HashingAlgNameStr.find(hashAlgName)->second;

			/* �������� ���������� ��������� - ��������������� ��� */
			NTSTATUS ntStatus = BCryptOpenAlgorithmProvider(&hHashAlg, algName, nullptr, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw exception("������: �� ������� ���������������� �������� �����������");
			}

			/* �������� ������ ��������� ���� */
			ntStatus = BCryptGetProperty(hHashAlg, BCRYPT_OBJECT_LENGTH, (LPBYTE)&cbHashObject, sizeof(DWORD), &cbDataHash, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw exception("������: �� ������� �������� ������ ����������� �������");
			}

			/* �������� ����� ����, ��������������� ���������� */
			ntStatus = BCryptGetProperty(hHashAlg, BCRYPT_HASH_LENGTH, (LPBYTE)&cbHashSize, sizeof(DWORD), &cbDataHash, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw exception("������: �� ������� �������� ����� ����");
			}
		}

		/* ������� ���������� ���� �� ���������� ������ � �� ������� */
		vector<BYTE> getHash(const vector<BYTE>& data) {
			/* �������� ������ ��� �������� ���� */
			unique_ptr<BYTE> pbHashObject(new BYTE[cbHashObject]);
			vector<BYTE> copyData(data);

			/* �������������� ����� ������� ���� */
			NTSTATUS ntStatus = BCryptCreateHash(hHashAlg, &hHash, pbHashObject.get(), cbHashObject, nullptr, 0, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw exception("������: �� ������� ������� ���������� ��������");
			}

			/* ���������� ����������� */
			ntStatus = BCryptHashData(hHash, copyData.data(), (ULONG)copyData.size(), 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				cerr << "������: �� ������� ������������ ������" << endl;
				BCryptDestroyHash(hHash);
				return {};
			}

			/* �������� ������ ��� ������ ���� */
			/* ������ ����������� ��� ������������ � �������� ���������� ������� */
			vector<BYTE> hashedData(cbHashSize, 0);

			/* ��������� ������ ���� � ��������� ��������� � ���������� ����� ������� */
			ntStatus = BCryptFinishHash(hHash, hashedData.data(), cbHashSize, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				cerr << "������: �� ������� ��������� �����������" << endl;
				BCryptDestroyHash(hHash);
				return {};
			}

			/* ������ ������� ���� � ������ */
			BCryptDestroyHash(hHash);

			/* ���������� �� ������� ��� � ������� */
			return hashedData;
		}

		~HashingAlgorithm() {
			BCryptCloseAlgorithmProvider(hHashAlg, 0);
		}
	};
	
	const enum SymmetricAlgName { DES, TripleDES };
	const enum SymmetricAlgMode { ECB, CBC };

	const map<SymmetricAlgName, LPCWSTR> SymmetricAlgNameStr = {
		{ SymmetricAlgName::DES, BCRYPT_DES_ALGORITHM },
		{ SymmetricAlgName::TripleDES, BCRYPT_3DES_112_ALGORITHM }
	};
	const map<SymmetricAlgMode, LPCWSTR> SymmetricAlgModeStr = {
		{ SymmetricAlgMode::ECB, BCRYPT_CHAIN_MODE_ECB },
		{ SymmetricAlgMode::CBC, BCRYPT_CHAIN_MODE_CBC }
	};

	/* �����, ����������� ������������ ���������� � ������������ */
	template<SymmetricAlgName symmetricAlgName, SymmetricAlgMode symmetricAlgMode>
	class SymmetricEncryptionAlgorithm {
	protected:
		/* ���������� ��������� */
		BCRYPT_ALG_HANDLE hSymmetricAlg;

		/* ������ ��������� ����� */
		DWORD cbKeyObject;

		/* ������ ����� ���������� */
		DWORD cbBlockLen;

		/* ���������� ��� �������� �� ������� */
		DWORD cbSymmetricData;

		/* ��������� �� ������ ��� ��������� ����� */
		unique_ptr<BYTE> pbKeyObject;

	private:
		/* ������������ ��������, ��������� ������: ���������� � ������������ */
		enum SymmetricAlgAction { ENCRYPT, DECRYPT };

		/* ������� ����������/������������� */
		function<NTSTATUS(BCRYPT_KEY_HANDLE, PUCHAR, ULONG, void*, PUCHAR, ULONG, PUCHAR, ULONG, ULONG* pcbResult, ULONG)> algFunctions[2];

		/* ������� ���������� ��������: ���� ����������, ���� ������������ */
		vector<BYTE> perfomAction(const vector<BYTE>& data, const vector<BYTE>& key, SymmetricAlgAction algAction) {
			/* ���������� ���� ����������; ������ �����, ����� �� �������� ������ */
			vector<BYTE> keyCopy(key);

			BCRYPT_KEY_HANDLE hKey = nullptr;
			NTSTATUS ntStatus = BCryptGenerateSymmetricKey(hSymmetricAlg, &hKey, pbKeyObject.get(), cbKeyObject, keyCopy.data(), (ULONG)keyCopy.size(), 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				cerr << "������: �� ������� ������������� ���� ����������" << endl;
				return {};
			}

			/* ������ ��������� ������ �������������, ����������� ������ */
			unique_ptr<BYTE> pbIV(symmetricAlgMode == SymmetricAlgMode::ECB ? nullptr : new BYTE[cbBlockLen]{ 0 });

			/* ������ ������ ��� �������� ������ ������ */
			ULONG cbOutput = 0;
			vector<BYTE> dataCopy(data);
			ntStatus = algFunctions[algAction](hKey, dataCopy.data(), (ULONG)dataCopy.size(), nullptr, pbIV.get(), cbBlockLen, nullptr, 0, &cbOutput, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				cerr << "������: �� ������� ������ ������ ������������� ������" << endl;
				BCryptDestroyKey(hKey);
				return {};
			}

			/* ��������� ��������������� ����������/������������ */
			vector<BYTE> pbOutput(cbOutput, 0);
			ntStatus = algFunctions[algAction](hKey, dataCopy.data(), (ULONG)dataCopy.size(), nullptr, pbIV.get(), cbBlockLen, pbOutput.data(), cbOutput, &cbSymmetricData, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				cerr << "������: �� ������� ��������� ������������" << endl;
				BCryptDestroyKey(hKey);
				return {};
			}

			BCryptDestroyKey(hKey);
			return pbOutput;
		}
	public:
		SymmetricEncryptionAlgorithm() {
			/* �������� ��� ��������� � ��� ����� ��� BCrypt */
			LPCWSTR algName = SymmetricAlgNameStr.find(symmetricAlgName)->second;
			LPCWSTR algMode = SymmetricAlgModeStr.find(symmetricAlgMode)->second;

			/* �������� ���������� ��������� ���������� */
			NTSTATUS ntStatus = BCryptOpenAlgorithmProvider(&hSymmetricAlg, algName, nullptr, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw exception("������: �� ������� ���������������� �������� ����������");
			}

			/* ������ ������ ��������� ��� ����� */
			ntStatus = BCryptGetProperty(hSymmetricAlg, BCRYPT_OBJECT_LENGTH, (LPBYTE)&cbKeyObject, sizeof(DWORD), &cbSymmetricData, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw exception("������: �� ������� �������� ������ ���������� �������");
			}

			/* ������ ����� ����� ��������� ���������� */
			ntStatus = BCryptGetProperty(hSymmetricAlg, BCRYPT_BLOCK_LENGTH, (LPBYTE)&cbBlockLen, sizeof(DWORD), &cbSymmetricData, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw exception("������: �� ������� �������� ������ ����� ����������");
			}

			/* ������ ����� ���������� �� ���������� ������ (CBC) */
			ntStatus = BCryptSetProperty(hSymmetricAlg, BCRYPT_CHAINING_MODE, (LPBYTE)algMode, sizeof(algMode), 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw exception("������: �� ������� �������� ��� ���������� ������ ����������");
			}

			/* �������� ������ ��� ��������� ����� */
			pbKeyObject.reset(new BYTE[cbKeyObject]);

			/* ��������� ������������� ������ �������������/����������, ����� ��� ����� ���� ������� */
			/* �� ���������� ������� �������� */
			algFunctions[SymmetricAlgAction::ENCRYPT] = BCryptEncrypt;
			algFunctions[SymmetricAlgAction::DECRYPT] = BCryptDecrypt;
		}

		/* ���������� ������� ��� ���������� ���������� � ������������ */
		std::vector<BYTE> encrypt(const vector<BYTE>& data, const vector<BYTE>& key) { return perfomAction(data, key, SymmetricAlgAction::ENCRYPT); }
		std::vector<BYTE> decrypt(const vector<BYTE>& data, const vector<BYTE>& key) { return perfomAction(data, key, SymmetricAlgAction::DECRYPT); }

		DWORD getBlockLen() const { return cbBlockLen; }

		~SymmetricEncryptionAlgorithm() {
			BCryptCloseAlgorithmProvider(hSymmetricAlg, 0);
		}
	};

	/* �������� ���������� Retail MAC ���������� DES, � ������� ��� ��������� */
	class RetailMAC : private SymmetricEncryptionAlgorithm<SymmetricAlgName::DES, SymmetricAlgMode::ECB> {
	public:
		RetailMAC() : SymmetricEncryptionAlgorithm() {}

		/* ��������� MAC �������� */
		vector<BYTE> getMAC(const vector<BYTE>& data, const vector<BYTE>& key) {
			/* ���� ������ ����� �� ����� 16 ������, �� ���� */
			if (key.size() != 16) {
				cerr << "������: ����� ����� ��� Retail MAC ������ ���� ����� 16 ������" << endl;
				return {};
			}
			
			/* ��������� ������ �� ��������� */
			vector<BYTE> paddedData = fillISO9797(data, cbBlockLen);

			/* �������� ������, ����� ���������� � ���������� MAC */
			/* ������� ������� ��� �������� */
			vector<BYTE> key0(key.begin(), key.begin() + cbBlockLen);
			vector<BYTE> key1(key.begin() + cbBlockLen, key.begin() + 2 * cbBlockLen);

			/* ����� �������� ������� ������ ��� ��������� ����� */
			vector<BYTE> block(cbBlockLen, 0);
			vector<BYTE> output(cbBlockLen, 0);

			/* � ����� �������� ������� ���, ������� ��������� �������� ����� � ������� ����������� ������ */
			for (size_t i = 0; i < paddedData.size() / cbBlockLen; i += 1) {
				/* ��������� XOR ���������� ����� ������ � ��������� ����� */
				for (size_t j = 0; j < cbBlockLen; j += 1) {
					output[j] = output[j] ^ paddedData[i * cbBlockLen + j];
				}

				/* ��������� �����, ����� ��������� ���� ���������� */
				output = encrypt(output, key0);
			}

			/* ����� ���������� ���� ������ ����������, ����������� ���� */
			output = decrypt(output, key1);
			output = encrypt(output, key0);

			/* � ������� �������� ������ */
			return output;
		}

		DWORD getBlockLen() const { return cbBlockLen; }
	};

	typedef SymmetricEncryptionAlgorithm<SymmetricAlgName::TripleDES, SymmetricAlgMode::CBC> Des3;
	typedef HashingAlgorithm<HashAlgName::SHA1> Sha1;

	static Des3 des3;
	static RetailMAC mac;
	static Sha1 sha1;

	Sha1& getSha1Alg() { return sha1; }
	Des3& getDes3Alg() { return des3; }
	RetailMAC& getMacAlg() { return mac; }
}