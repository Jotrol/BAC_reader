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
	
	
	static enum SymmetricAlgName { DES, TripleDES };
	static enum SymmetricAlgMode { ECB, CBC };
	static enum SymmetricAlgActions { ENCRYPT, DECRYPT };

	template<SymmetricAlgName algName, SymmetricAlgMode algMode>
	class SymmetricEncryptionAlgorithm {
	private:
		/* Дескриптор алгоритма */
		BCRYPT_ALG_HANDLE hAlg;

		/* Размер контекста ключа */
		DWORD cbKeyObject;

		/* Размер блока шифрования */
		DWORD cbBlockLen;

		/* Переменная для возврата из функций */
		DWORD cbData;

		/* Указатель на память для контекста ключа */
		std::unique_ptr<BYTE> pbKeyObject;

		/* Функции шифрования/расшифрования */
		std::function<NTSTATUS(BCRYPT_KEY_HANDLE, PUCHAR, ULONG, void*, PUCHAR, ULONG, PUCHAR, ULONG, ULONG* pcbResult, ULONG)> algFunctions[2];

		const static LPCWSTR pszAlgName[] = { BCRYPT_DES_ALGORITHM, BCRYPT_3DES_112_ALGORITHM };
		const static LPCWSTR pszAlgMode[] = { BCRYPT_CHAIN_MODE_ECB, BCRYPT_CHAIN_MODE_CBC };
	public:
		
		
		SymmetricEncryptionAlgorithm() {
			/* Получаем дескриптор алгоритма шифрования */
			NTSTATUS ntStatus = BCryptOpenAlgorithmProvider(&hAlg, pszAlgName[algName], nullptr, 0);
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
			ntStatus = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (LPBYTE)pszAlgMode[algMode], sizeof(pszAlgMode[algMode]), 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw std::exception("Ошибка: не удалось изменить тип зацепления блоков шифрования");
			}

			/* Выделяем память для контекста ключа */
			pbKeyObject.reset(new BYTE[cbKeyObject]);

			/* Заполняем автоматически функии расшифрования/шифрования, чтобы при смене слов местами */
			/* Не поменялись местами действия */
			algFunctions[AlgActions::ENCRYPT] = BCryptEncrypt;
			algFunctions[AlgActions::DECRYPT] = BCryptDecrypt;
		}

		std::vector<BYTE> enc_dec(const std::vector<BYTE>& data, const std::vector<BYTE>& key, AlgActions algAction) {
			/* Генерируем ключ шифрования; создаём копию, чтобы не поменять данные */
			std::vector<BYTE> keyCopy(key);
			
			BCRYPT_KEY_HANDLE hKey = nullptr;
			NTSTATUS ntStatus = BCryptGenerateSymmetricKey(hAlg, &hKey, pbKeyObject.get(), cbKeyObject, keyCopy.data(), keyCopy.size(), 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				std::cerr << "Ошибка: не удалось сгенерировать ключ шифрования" << std::endl;
				return {};
			}

			/* Создаём начальный вектор инициализации, заполненный нулями */
			std::unique_ptr<BYTE> pbIV(algMode == AlgMode::ECB ? nullptr : new BYTE[cbBlockLen]{0});

			/* Узнаем размер для выходных данных функии */
			DWORD cbOutput = 0;
			std::vector<BYTE> dataCopy(data);
			ntStatus = algFunctions[algAction](hKey, dataCopy.data(), dataCopy.size(), nullptr, pbIV.get(), cbBlockLen, nullptr, 0, &cbOutput, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				std::cerr << "Ошибка: не удалось узнать размер дешифрованных данных" << std::endl;
				BCryptDestroyKey(hKey);
				return {};
			}

			/* Выполняем непосредственно шифрование/дешифрование */
			std::vector<BYTE> pbOutput(cbOutput, 0);
			ntStatus = algFunctions[algAction](hKey, dataCopy.data(), dataCopy.size(), nullptr, pbIV.get(), cbBlockLen, pbOutput.data(), cbOutput, &cbData, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				std::cerr << "Ошибка: не удалось выполнить дешифрование" << std::endl;
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
		/* Дескриптор алгоритма DES */
		BCRYPT_ALG_HANDLE hAlg;

		/* Дескриптор ключа DES */
		BCRYPT_KEY_HANDLE hKey;

		/* Размер контекста ключа */
		DWORD cbKeyObject;

		/* Размер блока шифрования */
		DWORD cbBlockLen;

		/* Возвращаемое значение из функций Bcrypt */
		DWORD cbData;

		/* Сама память объекта ключа */
		std::unique_ptr<BYTE> pbKeyObject;
		
		/* Пока не придумал ничего лучше, чем использовать вот такого монстра вместо ~100 строк для шифрования и дешифрования у двух функций */
		enum DES_ACTION {ENCRYPT, DECRYPT};
		std::function<NTSTATUS(BCRYPT_KEY_HANDLE, PUCHAR, ULONG, void*, PUCHAR, ULONG, PUCHAR, ULONG, ULONG* pcbResult, ULONG)> desActions[2];

		/* Функция шифрования/дешифрования, режим задаётся перечислением */
		std::vector<BYTE> DES_enc_dec(std::vector<BYTE> data, std::vector<BYTE> key, DES_ACTION desAction) {
			/* Генерируем ключ для шифрования/расшифровки */
			NTSTATUS ntStatus = BCryptGenerateSymmetricKey(hAlg, &hKey, pbKeyObject.get(), cbKeyObject, key.data(), key.size(), 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				std::cerr << "Ошибка: не удалось сгенерировать ключ шифрования" << std::endl;
				return {};
			}

			/* Узнаем размер для выходных данных функии */
			DWORD cbOutput = 0;
			std::vector<BYTE> dataCopy(data);
			ntStatus = desActions[desAction](hKey, dataCopy.data(), dataCopy.size(), nullptr, nullptr, cbBlockLen, nullptr, 0, &cbOutput, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				std::cerr << "Ошибка: не удалось узнать размер дешифрованных данных" << std::endl;
				return {};
			}

			/* Выполняем непосредственно шифрование/дешифрование */
			std::vector<BYTE> pbOutput(cbOutput, 0);
			ntStatus = desActions[desAction](hKey, dataCopy.data(), dataCopy.size(), nullptr, nullptr, cbBlockLen, pbOutput.data(), cbOutput, &cbData, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				std::cerr << "Ошибка: не удалось выполнить дешифрование" << std::endl;
				return {};
			}

			/* Отдаём результат */
			return pbOutput;
		}
	public:
		RetailMAC() : hAlg(nullptr), hKey(nullptr), pbKeyObject() {
			/* Получаем дескриптор алгоритма шифрования */
			NTSTATUS ntStatus = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_DES_ALGORITHM, nullptr, 0);
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
			ntStatus = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (LPBYTE)BCRYPT_CHAIN_MODE_ECB, sizeof(BCRYPT_CHAIN_MODE_ECB), 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw std::exception("Ошибка: не удалось изменить тип зацепления блоков шифрования");
			}

			/* Выделяем память для контекста ключа */
			pbKeyObject.reset(new BYTE[cbKeyObject]);

			/* Задано так, а не константо при объявлении потому, что ENCRYPT и DECRYPT если кто поменяет местами */
			/* То может забыть, что и функции надо местами поменять тоже. Так что программа пускай сама расставляет индексы */
			desActions[DES_ACTION::ENCRYPT] = BCryptEncrypt;
			desActions[DES_ACTION::DECRYPT] = BCryptDecrypt;
		}

		/* Вычислить MAC значение */
		std::vector<BYTE> getMAC(const std::vector<BYTE>& data, const std::vector<BYTE>& key) {
			/* Если размер ключа не равен 16 байтам, то беда */
			if (key.size() != 16) {
				std::cerr << "Ошибка: длина ключа для Retail MAC должна быть равна 16 байтам" << std::endl;
				return {};
			}
			
			/* Копируем исходные данные в изменяемый вектор */
			std::vector<BYTE> dataCopy(data);

			/* Заполняем по методу заполнения 2 стандарта ИСО/МЭК 9797-1 */
			dataCopy.push_back(128); /* Добавляем единичный бит в конец данных, 128 = 0b10000000 */

			/* Заполняем нулями пока не кратно размеру блоку */
			while (dataCopy.size() % cbBlockLen != 0) {
				dataCopy.push_back(0);
			}

			/* Выровняв данные, можно приступать к вычислению MAC */
			/* Сначала выделим два подключа */
			std::vector<BYTE> key0(key.begin(), key.begin() + cbBlockLen);
			std::vector<BYTE> key1(key.begin() + cbBlockLen, key.begin() + 2 * cbBlockLen);

			/* Затем разметим область памяти под временные блоки */
			std::vector<BYTE> block(cbBlockLen, 0);
			std::vector<BYTE> output(cbBlockLen, 0);

			/* В цикле обернёмся столько раз, сколько умещается размеров блока в размере выровненных данных */
			for (size_t i = 0; i < dataCopy.size() / cbBlockLen; i += 1) {
				/* Копируем часть данных в блок */
				auto dataBlockBegin = dataCopy.data() + i * cbBlockLen;
				memcpy(block.data(), dataBlockBegin, cbBlockLen);

				/* Выполняем XOR временного блока данных и выходного блока */
				for (size_t j = 0; j < block.size(); j += 1) {
					output[j] = output[j] ^ block[j];
				}

				/* Проксорив блоки, можно выполнить этап шифрования */
				output = DES_enc_dec(output, key0, DES_ACTION::ENCRYPT);
			}

			/* После выполнения всех циклов шифрования, завершающий этап */
			output = DES_enc_dec(output, key1, DES_ACTION::DECRYPT);
			output = DES_enc_dec(output, key0, DES_ACTION::ENCRYPT);

			/* И возврат исходных данных */
			return output;
		}
	};
}

/* Тесты из Информативного дополнения 6 части 1 тома 2 документа 9303 */
#ifdef TEST_PARITY
#include <cassert>

void testParityCorrect() {
	/* Лямбда-функция, которая выполняет тестирование на совпадение откорректированных битов с ожидаемым */
	auto testParity = [](std::vector<BYTE> key, std::vector<BYTE> expected) {
		for (size_t i = 0; i < key.size(); i += 1) {
			key[i] = Crypto::genByteWithParityBit(key[i]);
		}
		assert(key == expected);
	};

	/* Сами прогоны на тестовых данных */
	testParity({ 0xAB, 0x94, 0xFC, 0xED, 0xF2, 0x66, 0x4E, 0xDF }, { 0xAB, 0x94, 0xFD, 0xEC, 0xF2, 0x67, 0x4F, 0xDF });
	testParity({ 0xB9, 0xB2, 0x91, 0xF8, 0x5D, 0x7F, 0x77, 0xF2 }, { 0xB9, 0xB3, 0x91, 0xF8, 0x5D, 0x7F, 0x76, 0xF2 });
	testParity({ 0x78, 0x62, 0xD9, 0xEC, 0xE0, 0x3C, 0x1B, 0xCD }, { 0x79, 0x62, 0xD9, 0xEC, 0xE0, 0x3D, 0x1A, 0xCD });
	testParity({ 0x4D, 0x77, 0x08, 0x9D, 0xCF, 0x13, 0x14, 0x42 }, { 0x4C, 0x76, 0x08, 0x9D, 0xCE, 0x13, 0x15, 0x43 });

	std::cout << "Проверка корректировки битов чётности выполнена успешно!" << std::endl;
}

#endif