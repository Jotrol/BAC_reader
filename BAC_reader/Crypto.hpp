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
	/* Основано на "Алгоритмических трюках хакера" */
	/* Здесь считается бит чётности от 7 битов, поэтому в конце ещё одно смещение */
	/* Чётное кол-во единичных битов - 1, нечётное - 0 */
	unsigned char genByteWithParityBit(unsigned char x) {
		/* Используя побитовый сдвиг, последний бит выполнят xor со всеми предыдущими, предпоследний - со всеми, кроме последнего и т.д */
		unsigned char y = x ^ (x >> 1);
		y = y ^ (y >> 2);
		y = y ^ (y >> 4);

		/* Здесь освобождаем последний бит исходного числа, а из полученного */
		/* вытягиваем предпоследний бит (он был xor'ут со всеми, кроме последнего бита) */
		/* инвертируем его и берём при помощи И последний бит получившегося числа*/
		return (x & 0xFE) | (~(y >> 1) & 1);
	}

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
				throw std::exception("Ошибка: не удалось получить размер хэширующего объекта");
			}

			/* Получаем длину хэша, вырабатываемого алгоритмом */
			ntStatus = BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (LPBYTE)&cbHashSize, sizeof(DWORD), &cbData, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw std::exception("Ошибка: не удалось получить длину хэша");
			}

			/* Выделяем память под контекст хэша */
			pbHashObject.reset(new BYTE[cbHashObject]);
		}

		/* Функция вычисления хэша от переданных данных и их размера */
		std::vector<BYTE> hash(LPBYTE data, DWORD dataSize) {
			/* Инициализируем новый контекс хэша */
			NTSTATUS ntStatus = BCryptCreateHash(hAlg, &hHash, pbHashObject.get(), cbHashObject, nullptr, 0, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw std::exception("Ошибка: не удалось создать хэширующий алгоритм");
			}

			/* Выполнение хэширования */
			ntStatus = BCryptHashData(hHash, data, dataSize, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				std::cerr << "Ошибка: не удалось захэшировать данные" << std::endl;
				return {};
			}

			/* Выделяем вектор под размер хэша */
			/* Вектор использован для лаконичности и простоты управления памятью */
			std::vector<BYTE> hashedData(cbHashSize, 0);

			/* Завершаем обсчёт хэша и сохраняем результат в выделенном ранее векторе */
			ntStatus = BCryptFinishHash(hHash, hashedData.data(), cbHashSize, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				std::cerr << "Ошибка: не удалось завершить хэширование" << std::endl;
				return {};
			}

			/* Возвращаем из функции хэш в векторе */
			return hashedData;
		}

		/* Перегрузка функции для работы с вектором */
		std::vector<BYTE> hash(std::vector<BYTE>& data) {
			return this->hash(data.data(), (DWORD)data.size());
		}

		/* Деструктор для обёртки */
		~SHA1() {
			/* Закрываем алгоритм */
			BCryptCloseAlgorithmProvider(hAlg, 0);

			/* Очищаем контекст хэша; память из под него очистится автоматически */
			BCryptDestroyHash(hHash);
		}
	};

	/* Обёртка для более простого интерфейса шифрования по алгоритму 3DES */
	class DES3 {
	private:
		/* Дескриптор алгоритма */
		BCRYPT_ALG_HANDLE hAlg;

		/* Дескриптор ключа */
		BCRYPT_KEY_HANDLE hKey;

		/* Размер контекста ключа */
		DWORD cbKeyObject;

		/* Размер блока шифрования */
		DWORD cbBlockLen;

		/* Переменная для возврата из функций */
		DWORD cbData;

		/* Указатель на память для контекста ключа */
		std::unique_ptr<BYTE> pbKeyObject;
	public:
		/* Инициализируем начальные значения нулями */
		DES3() : hAlg(nullptr), hKey(nullptr), pbKeyObject() {
			/* Получаем дескриптор алгоритма шифрования */
			NTSTATUS ntStatus = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_3DES_112_ALGORITHM, nullptr, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw std::exception("Ошибка: не удалось инициализировать алгоритм шифрования");
			}

			/* Узнаем размер контекста для ключа */
			ntStatus = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (LPBYTE)&cbKeyObject, sizeof(DWORD), &cbData, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw std::exception("Ошибка: не удалось получить размер шифрующего объекта");
			}

			/* Узнаем длину блока алгоритма шифрования */
			ntStatus = BCryptGetProperty(hAlg, BCRYPT_BLOCK_LENGTH, (LPBYTE)&cbBlockLen, sizeof(DWORD), &cbData, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw std::exception("Ошибка: не удалось получить размер блока шифрования");
			}

			/* Меняем режим шифрования на зацепление блоков (CBC) */
			ntStatus = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (LPBYTE)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw std::exception("Ошибка: не удалось изменить тип зацепления блоков шифрования");
			}

			/* Выделяем память для контекста ключа */
			pbKeyObject.reset(new BYTE[cbKeyObject]);
		}

		/* Низкоуровневая функция шифрования; предполагается, что ключ содержит биты чётности */
		std::vector<BYTE> encryptData(LPBYTE pbKey, DWORD cbKey, LPBYTE pbData, DWORD cbData) {
			/* Генерируем ключ шифрования */
			NTSTATUS ntStatus = BCryptGenerateSymmetricKey(hAlg, &hKey, pbKeyObject.get(), cbKeyObject, pbKey, cbKey, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				std::cerr << "Ошибка: не удалось сгенерировать ключ шифрования" << std::endl;
				return {};
			}

			/* Создаём начальный вектор инициализации, заполненный нулями */
			std::unique_ptr<BYTE> pbIV(new BYTE[cbBlockLen]{ 0 });

			/* Копируем шифруемые данные */
			DWORD cbPlainText = cbData;
			std::unique_ptr<BYTE> pbPlainText(new BYTE[cbData]);
			std::memcpy(pbPlainText.get(), pbData, cbData);

			/* Узнаем размер зашифрованных данных */
			DWORD cbCipherText = 0;
			ntStatus = BCryptEncrypt(hKey, pbPlainText.get(), cbPlainText, nullptr, pbIV.get(), cbBlockLen, nullptr, 0, &cbCipherText, BCRYPT_BLOCK_PADDING);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				std::cerr << "Ошибка: не удалось узнать размер зашифрованных данных" << std::endl;
				return {};
			}

			/* Выделяем место в памяти и выполняем шифрование */
			std::vector<BYTE> pbCipherText(cbCipherText, 0);
			ntStatus = BCryptEncrypt(hKey, pbPlainText.get(), cbPlainText, nullptr, pbIV.get(), cbBlockLen, pbCipherText.data(), cbCipherText, &cbData, BCRYPT_BLOCK_PADDING);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				std::cerr << "Ошибка: не удалось выполнить шифрование" << std::endl;
				return {};
			}

			return pbCipherText;
		}

		/* Функция шифрования для работы с контейнерами */
		std::vector<BYTE> encryptData(std::vector<BYTE> key, std::vector<BYTE> data) {
			return this->encryptData(key.data(), (DWORD)key.size(), data.data(), (DWORD)data.size());
		}

		/* Деструктор */
		~DES3() {
			/* Закрываем алогоритм шифрования */
			BCryptCloseAlgorithmProvider(hAlg, 0);

			/* Очищаем ключ; контекст ключа будет очищен самостоятельно */
			BCryptDestroyKey(hKey);
		}
	};
}

/* Тесты из Информативного дополнения 6 части 1 тома 2 документа 9303 */
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