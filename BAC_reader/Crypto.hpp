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
	};

	typedef SymmetricEncryptionAlgorithm<SymmetricAlgName::TripleDES, SymmetricAlgMode::CBC> Des3;
	typedef HashingAlgorithm<HashAlgName::SHA1> Sha1;
}

#ifdef TEST_CRYPTO
#include <cassert>

/* ����� �� �������������� ���������� 6 ����� 1 ���� 2 ��������� 9303 */
void testParityCorrect() {
	/* ������-�������, ������� ��������� ������������ �� ���������� ������������������ ����� � ��������� */
	auto testParity = [](vector<BYTE> key, vector<BYTE> expected) {
		assert(correctParityBits(key) == expected);
	};

	/* ���� ������� �� �������� ������ */
	testParity({ 0xAB, 0x94, 0xFC, 0xED, 0xF2, 0x66, 0x4E, 0xDF }, { 0xAB, 0x94, 0xFD, 0xEC, 0xF2, 0x67, 0x4F, 0xDF });
	testParity({ 0xB9, 0xB2, 0x91, 0xF8, 0x5D, 0x7F, 0x77, 0xF2 }, { 0xB9, 0xB3, 0x91, 0xF8, 0x5D, 0x7F, 0x76, 0xF2 });
	testParity({ 0x78, 0x62, 0xD9, 0xEC, 0xE0, 0x3C, 0x1B, 0xCD }, { 0x79, 0x62, 0xD9, 0xEC, 0xE0, 0x3D, 0x1A, 0xCD });
	testParity({ 0x4D, 0x77, 0x08, 0x9D, 0xCF, 0x13, 0x14, 0x42 }, { 0x4C, 0x76, 0x08, 0x9D, 0xCE, 0x13, 0x15, 0x43 });

	cout << "�������� ������������� ����� �������� ��������� �������!" << endl;
}

/* ����� ����� � Wikipedia */
void testHashingSha1() {
	/* ����� ��� ���������� ����������� */
	Crypto::Sha1 sha1;

	/* ������� ���������� ������������ ����������� */
	auto testHashing = [&](string dataStr, vector<BYTE> expected) {
		/* ��������� ������ �� string � vector */
		vector<BYTE> data(dataStr.size(), 0);
		copy(dataStr.c_str(), dataStr.c_str() + dataStr.size(), data.begin());

		/* ������� ��� */
		vector<BYTE> output = sha1.getHash(data);
		assert(output == expected);
	};

	/* ������� �������� ������ */
	testHashing("The quick brown fox jumps over the lazy dog", { 0x2f, 0xd4, 0xe1, 0xc6, 0x7a, 0x2d, 0x28, 0xfc, 0xed, 0x84, 0x9e, 0xe1, 0xbb, 0x76, 0xe7, 0x39, 0x1b, 0x93, 0xeb, 0x12 });
	testHashing("sha", { 0xd8, 0xf4, 0x59, 0x03, 0x20, 0xe1, 0x34, 0x3a, 0x91, 0x5b, 0x63, 0x94, 0x17, 0x06, 0x50, 0xa8, 0xf3, 0x5d, 0x69, 0x26 });
	testHashing("Sha", { 0xba, 0x79, 0xba, 0xeb, 0x9f, 0x10, 0x89, 0x6a, 0x46, 0xae, 0x74, 0x71, 0x52, 0x71, 0xb7, 0xf5, 0x86, 0xe7, 0x46, 0x40 });
	testHashing("", { 0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d, 0x32, 0x55, 0xbf, 0xef, 0x95, 0x60, 0x18, 0x90, 0xaf, 0xd8, 0x07, 0x09 });
}

/* ���� �������������� �� �������������� ���������� 6 ����� 1 ���� 2 ��������� 9303  */
void testAuthProcess() {
	vector<BYTE> expectedCmdData = { 0x72, 0xC2, 0x9C, 0x23, 0x71, 0xCC, 0x9B, 0xDB, 0x65, 0xB7, 0x79, 0xB8, 0xE8, 0xD3, 0x7B, 0x29, 0xEC, 0xC1, 0x54, 0xAA, 0x56, 0xA8, 0x79, 0x9F, 0xAE, 0x2F, 0x49, 0x8F, 0x76, 0xED, 0x92, 0xF2, 0x5F, 0x14, 0x48, 0xEE, 0xA8, 0xAD, 0x90, 0xA7 };

	Crypto::Sha1 sha1;
	Crypto::Des3 des3;
	Crypto::RetailMAC mac;

	vector<BYTE> dEnc = { 0x23, 0x9A, 0xB9, 0xCB, 0x28, 0x2D, 0xAF, 0x66, 0x23, 0x1D, 0xC5, 0xA4, 0xDF, 0x6B, 0xFB, 0xAE, 0x00, 0x00, 0x00, 0x01 };
	vector<BYTE> dMac = { 0x23, 0x9A, 0xB9, 0xCB, 0x28, 0x2D, 0xAF, 0x66, 0x23, 0x1D, 0xC5, 0xA4, 0xDF, 0x6B, 0xFB, 0xAE, 0x00, 0x00, 0x00, 0x02 };

	vector<BYTE> dEncHash = sha1.getHash(dEnc); dEncHash.resize(16);
	vector<BYTE> dMacHash = sha1.getHash(dMac); dMacHash.resize(16);

	vector<BYTE> kEnc = Crypto::correctParityBits(dEncHash);
	vector<BYTE> kMac = Crypto::correctParityBits(dMacHash);

	vector<BYTE> S = { 0x78, 0x17, 0x23, 0x86, 0x0C, 0x06, 0xC2, 0x26, 0x46, 0x08, 0xF9, 0x19, 0x88, 0x70, 0x22, 0x12, 0x0B, 0x79, 0x52, 0x40, 0xCB, 0x70, 0x49, 0xB0, 0x1C, 0x19, 0xB3, 0x3E, 0x32, 0x80, 0x4F, 0x0B };
	vector<BYTE> eIFD = des3.encrypt(S, kEnc); eIFD.resize(32);

	vector<BYTE> mIFD = mac.getMAC(eIFD, kMac);
	vector<BYTE> cmdData(eIFD); cmdData.insert(cmdData.end(), mIFD.begin(), mIFD.end());

	assert(cmdData == expectedCmdData);
}
#endif