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
	/* Обёртка для более простого интерфейса вычисления хэша по алгоритму SHA-1 */
	class SHA1 {
	private:
		/* Дескриптор алгоритма */
		BCRYPT_ALG_HANDLE hAlg;

		/* Дескриптор контекста хэша */
		BCRYPT_HASH_HANDLE hHash;

		/* Размер контекста хэша */
		DWORD cbHashObject;

		/* Длина хэша */
		DWORD cbHashSize;

		/* Переменная для передачи в функции и получения значений */
		DWORD cbData;

		/* Массив для хранения контекста хэша */
		std::unique_ptr<BYTE> pbHashObject;
	public:

		/* Конструктор */
		SHA1() : hAlg(nullptr), hHash(nullptr), pbHashObject() {
			/* Получаем дескриптор алгоритма - инцициализируем его */
			NTSTATUS ntStatus = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA1_ALGORITHM, nullptr, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw std::exception("Ошибка: не удалось инициализировать алгоритм хэширования");
			}

			/* Получаем размер контекста хэша */
			ntStatus = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (LPBYTE)&cbHashObject, sizeof(DWORD), &cbData, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw std::exception("Ошибка: нельзя получить размер хэширующего объекта");
			}

			/* Получаем длину хэша, вырабатываемого алгоритмом */
			ntStatus = BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (LPBYTE)&cbHashSize, sizeof(DWORD), &cbData, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw std::exception("Ошибка: не удалось получить длину хэша");
			}

			/* Выделяем память под контекст хэша */
			pbHashObject.reset(new BYTE[cbHashObject]);

			/* Инициализируем контекст хэша */
			this->clearHash();
		}

		/* Функция очистки контекста хэша */
		void clearHash() {
			/* Удаляем старый контекст */
			BCryptDestroyHash(hHash);

			/* Получаем новый в ту же область памяти, где и был старый */
			NTSTATUS ntStatus = BCryptCreateHash(hAlg, &hHash, pbHashObject.get(), cbHashObject, nullptr, 0, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw std::exception("Ошибка: не удалось создать хэширующий алгоритм");
			}
		}

		/* Функция вычисления хэша от переданных данных и их размера */
		std::vector<BYTE> hash(LPBYTE data, INT dataSize) {
			/* Выполнение хэширования */
			NTSTATUS ntStatus = BCryptHashData(hHash, data, dataSize, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				std::cerr << "Не удалось захэшировать данные" << std::endl;
				return {};
			}

			/* Выделяем вектор под размер хэша */
			/* Вектор использован для лаконичности и простоты управления памятью */
			std::vector<BYTE> hashedData(cbHashSize, 0);

			/* Завершаем обсчёт хэша и сохраняем результат в выделенном ранее векторе */
			ntStatus = BCryptFinishHash(hHash, hashedData.data(), cbHashSize, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				std::cerr << "Не удалось завершить хэширование" << std::endl;
				return {};
			}

			/* Возвращаем из функции хэш в векторе */
			return hashedData;
		}

		/* Деструктор для обёртки */
		~SHA1() {
			/* Закрываем алгоритм */
			BCryptCloseAlgorithmProvider(hAlg, 0);

			/* Очищаем контекст хэша; память из под него очистится автоматически */
			BCryptDestroyHash(hHash);
		}
	};
}

/* Основано на "Алгоритмических трюках хакера" */
/* Здесь считается бит чётности от 7 битов, поэтому в конце ещё одно смещение */
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
		cerr << "Ошибка: не удалось получить размер объекта" << endl;
		return 1;
	}

	ntStatus = BCryptGetProperty(hTDES, BCRYPT_BLOCK_LENGTH, (LPBYTE)&cbBlockLen, sizeof(DWORD), &cbData, 0);
	if (!BCRYPT_SUCCESS(ntStatus)) {
		cerr << "Ошибка: не удалось получить размер блока" << endl;
		return 1;
	}

	pbKeyObject = new BYTE[cbKeyObject];
	pbIV = new BYTE[cbBlockLen]{ 0 };

	ntStatus = BCryptGenerateSymmetricKey(hTDES, &hKey, pbKeyObject, cbKeyObject, (LPBYTE)DESkey, DESkeyLen, 0);
	if (!BCRYPT_SUCCESS(ntStatus)) {
		cerr << "Ошибка: не удалось сгенерировать симметричный ключ" << endl;
		delete[] pbKeyObject;
		delete[] pbIV;
		return 1;
	}

	pbPlainText = new BYTE[plaintextLen];
	memcpy(pbPlainText, plaintext, plaintextLen);

	ntStatus = BCryptEncrypt(hKey, pbPlainText, plaintextLen, nullptr, pbIV, cbBlockLen, nullptr, 0, &cbCipherText, BCRYPT_BLOCK_PADDING);
	if (!BCRYPT_SUCCESS(ntStatus)) {
		cerr << "Ошибка: не удалось выполнить определение размера зашифрованных данных" << endl;
		BCryptDestroyKey(hKey);
		delete[] pbKeyObject;
		delete[] pbIV;
		delete[] pbPlainText;
		return 1;
	}

	pbCipherText = new BYTE[cbCipherText];
	ntStatus = BCryptEncrypt(hKey, pbPlainText, plaintextLen, nullptr, pbIV, cbBlockLen, pbCipherText, cbCipherText, &cbData, BCRYPT_BLOCK_PADDING);
	if (!BCRYPT_SUCCESS(ntStatus)) {
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