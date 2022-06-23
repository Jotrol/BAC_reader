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
	/* Основано на "Алгоритмических трюках хакера" */
	/* Здесь считается бит чётности от 7 битов, поэтому в конце ещё одно смещение */
	/* Чётное кол-во единичных битов - 1, нечётное - 0 */
	unsigned char getByteWithParityBit(unsigned char x) {
		/* Используя побитовый сдвиг, последний бит выполнят xor со всеми предыдущими, предпоследний - со всеми, кроме последнего и т.д */
		unsigned char y = x ^ (x >> 1);
		y = y ^ (y >> 2);
		y = y ^ (y >> 4);

		/* Здесь освобождаем последний бит исходного числа, а из полученного */
		/* вытягиваем предпоследний бит (он был xor'ут со всеми, кроме последнего бита) */
		/* инвертируем его и берём при помощи И последний бит получившегося числа*/
		return (x & 0xFE) | (~(y >> 1) & 1);
	}

	/* Функция для коррекции битов чётности переданного вектора */
	vector<BYTE> correctParityBits(const vector<BYTE>& data) {
		/* Копируем исходные данные */
		vector<BYTE> dataCopy(data);

		/* Корректируем биты чётности каждого байта */
		for (size_t i = 0; i < dataCopy.size(); i += 1) {
			dataCopy[i] = getByteWithParityBit(dataCopy[i]);
		}

		/* Возвращаем новый вектор */
		return dataCopy;
	}

	/* Функция заполнения по методу заполнения 2 стандарта ИСО/МЭК 9797-1 */
	vector<BYTE> fillISO9797(const vector<BYTE>& data, int blockLen) {
		/* Копируем исходные данные в изменяемый вектор */
		vector<BYTE> dataCopy(data);

		/* Заполняем по методу заполнения 2 стандарта ИСО/МЭК 9797-1 */
		dataCopy.push_back(0x80); /* Добавляем единичный бит в конец данных, 128 = 0b10000000 */

		/* Заполняем нулями пока не кратно размеру блоку */
		while (dataCopy.size() % blockLen != 0) {
			dataCopy.push_back(0);
		}

		return dataCopy;
	}

	const enum HashAlgName { SHA1 };
	const map<HashAlgName, LPCWSTR> HashingAlgNameStr = {
		{ HashAlgName::SHA1, BCRYPT_SHA1_ALGORITHM }
	};

	/* Класс, реализующий хэширование */
	template<HashAlgName hashAlgName>
	class HashingAlgorithm {
	protected:
		/* Дескриптор алгоритма */
		BCRYPT_ALG_HANDLE hHashAlg;

		/* Дескриптор контекста хэша */
		BCRYPT_HASH_HANDLE hHash;

		/* Размер контекста хэша */
		DWORD cbHashObject;

		/* Длина хэша */
		DWORD cbHashSize;

		/* Переменная для передачи в функции и получения значений */
		DWORD cbDataHash;
	public:
		HashingAlgorithm() : hHash(nullptr) {
			/* Получаем имя алгоритма из словаря */
			LPCWSTR algName = HashingAlgNameStr.find(hashAlgName)->second;

			/* Получаем дескриптор алгоритма - инцициализируем его */
			NTSTATUS ntStatus = BCryptOpenAlgorithmProvider(&hHashAlg, algName, nullptr, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw exception("Ошибка: не удалось инициализировать алгоритм хэширования");
			}

			/* Получаем размер контекста хэша */
			ntStatus = BCryptGetProperty(hHashAlg, BCRYPT_OBJECT_LENGTH, (LPBYTE)&cbHashObject, sizeof(DWORD), &cbDataHash, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw exception("Ошибка: не удалось получить размер хэширующего объекта");
			}

			/* Получаем длину хэша, вырабатываемого алгоритмом */
			ntStatus = BCryptGetProperty(hHashAlg, BCRYPT_HASH_LENGTH, (LPBYTE)&cbHashSize, sizeof(DWORD), &cbDataHash, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw exception("Ошибка: не удалось получить длину хэша");
			}
		}

		/* Функция вычисления хэша от переданных данных и их размера */
		vector<BYTE> getHash(const vector<BYTE>& data) {
			/* Выделяем память под контекст хэша */
			unique_ptr<BYTE> pbHashObject(new BYTE[cbHashObject]);
			vector<BYTE> copyData(data);

			/* Инициализируем новый контекс хэша */
			NTSTATUS ntStatus = BCryptCreateHash(hHashAlg, &hHash, pbHashObject.get(), cbHashObject, nullptr, 0, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw exception("Ошибка: не удалось создать хэширующий алгоритм");
			}

			/* Выполнение хэширования */
			ntStatus = BCryptHashData(hHash, copyData.data(), (ULONG)copyData.size(), 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				cerr << "Ошибка: не удалось захэшировать данные" << endl;
				BCryptDestroyHash(hHash);
				return {};
			}

			/* Выделяем вектор под размер хэша */
			/* Вектор использован для лаконичности и простоты управления памятью */
			vector<BYTE> hashedData(cbHashSize, 0);

			/* Завершаем обсчёт хэша и сохраняем результат в выделенном ранее векторе */
			ntStatus = BCryptFinishHash(hHash, hashedData.data(), cbHashSize, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				cerr << "Ошибка: не удалось завершить хэширование" << endl;
				BCryptDestroyHash(hHash);
				return {};
			}

			/* Чистим конекст хэша в памяти */
			BCryptDestroyHash(hHash);

			/* Возвращаем из функции хэш в векторе */
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

	/* Класс, реализующий симметричные шифрование и дешифрование */
	template<SymmetricAlgName symmetricAlgName, SymmetricAlgMode symmetricAlgMode>
	class SymmetricEncryptionAlgorithm {
	protected:
		/* Дескриптор алгоритма */
		BCRYPT_ALG_HANDLE hSymmetricAlg;

		/* Размер контекста ключа */
		DWORD cbKeyObject;

		/* Размер блока шифрования */
		DWORD cbBlockLen;

		/* Переменная для возврата из функций */
		DWORD cbSymmetricData;

		/* Указатель на память для контекста ключа */
		unique_ptr<BYTE> pbKeyObject;

	private:
		/* Перечисление действий, доступных классу: шифрование и дешифрование */
		enum SymmetricAlgAction { ENCRYPT, DECRYPT };

		/* Функции шифрования/расшифрования */
		function<NTSTATUS(BCRYPT_KEY_HANDLE, PUCHAR, ULONG, void*, PUCHAR, ULONG, PUCHAR, ULONG, ULONG* pcbResult, ULONG)> algFunctions[2];

		/* Функиця совершения действия: либо шифрование, либо дешифрование */
		vector<BYTE> perfomAction(const vector<BYTE>& data, const vector<BYTE>& key, SymmetricAlgAction algAction) {
			/* Генерируем ключ шифрования; создаём копию, чтобы не поменять данные */
			vector<BYTE> keyCopy(key);

			BCRYPT_KEY_HANDLE hKey = nullptr;
			NTSTATUS ntStatus = BCryptGenerateSymmetricKey(hSymmetricAlg, &hKey, pbKeyObject.get(), cbKeyObject, keyCopy.data(), (ULONG)keyCopy.size(), 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				cerr << "Ошибка: не удалось сгенерировать ключ шифрования" << endl;
				return {};
			}

			/* Создаём начальный вектор инициализации, заполненный нулями */
			unique_ptr<BYTE> pbIV(symmetricAlgMode == SymmetricAlgMode::ECB ? nullptr : new BYTE[cbBlockLen]{ 0 });

			/* Узнаем размер для выходных данных функии */
			ULONG cbOutput = 0;
			vector<BYTE> dataCopy(data);
			ntStatus = algFunctions[algAction](hKey, dataCopy.data(), (ULONG)dataCopy.size(), nullptr, pbIV.get(), cbBlockLen, nullptr, 0, &cbOutput, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				cerr << "Ошибка: не удалось узнать размер дешифрованных данных" << endl;
				BCryptDestroyKey(hKey);
				return {};
			}

			/* Выполняем непосредственно шифрование/дешифрование */
			vector<BYTE> pbOutput(cbOutput, 0);
			ntStatus = algFunctions[algAction](hKey, dataCopy.data(), (ULONG)dataCopy.size(), nullptr, pbIV.get(), cbBlockLen, pbOutput.data(), cbOutput, &cbSymmetricData, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				cerr << "Ошибка: не удалось выполнить дешифрование" << endl;
				BCryptDestroyKey(hKey);
				return {};
			}

			BCryptDestroyKey(hKey);
			return pbOutput;
		}
	public:
		SymmetricEncryptionAlgorithm() {
			/* Получаем имя алгоритма и его режим для BCrypt */
			LPCWSTR algName = SymmetricAlgNameStr.find(symmetricAlgName)->second;
			LPCWSTR algMode = SymmetricAlgModeStr.find(symmetricAlgMode)->second;

			/* Получаем дескриптор алгоритма шифрования */
			NTSTATUS ntStatus = BCryptOpenAlgorithmProvider(&hSymmetricAlg, algName, nullptr, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw exception("Ошибка: не удалось инициализировать алгоритм шифрования");
			}

			/* Узнаем размер контекста для ключа */
			ntStatus = BCryptGetProperty(hSymmetricAlg, BCRYPT_OBJECT_LENGTH, (LPBYTE)&cbKeyObject, sizeof(DWORD), &cbSymmetricData, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw exception("Ошибка: не удалось получить размер шифрующего объекта");
			}

			/* Узнаем длину блока алгоритма шифрования */
			ntStatus = BCryptGetProperty(hSymmetricAlg, BCRYPT_BLOCK_LENGTH, (LPBYTE)&cbBlockLen, sizeof(DWORD), &cbSymmetricData, 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw exception("Ошибка: не удалось получить размер блока шифрования");
			}

			/* Меняем режим шифрования на зацепление блоков (CBC) */
			ntStatus = BCryptSetProperty(hSymmetricAlg, BCRYPT_CHAINING_MODE, (LPBYTE)algMode, sizeof(algMode), 0);
			if (!BCRYPT_SUCCESS(ntStatus)) {
				throw exception("Ошибка: не удалось изменить тип зацепления блоков шифрования");
			}

			/* Выделяем память для контекста ключа */
			pbKeyObject.reset(new BYTE[cbKeyObject]);

			/* Заполняем автоматически функии расшифрования/шифрования, чтобы при смене слов местами */
			/* Не поменялись местами действия */
			algFunctions[SymmetricAlgAction::ENCRYPT] = BCryptEncrypt;
			algFunctions[SymmetricAlgAction::DECRYPT] = BCryptDecrypt;
		}

		/* Раздельные функции для выполнения шифрования и дешифрования */
		std::vector<BYTE> encrypt(const vector<BYTE>& data, const vector<BYTE>& key) { return perfomAction(data, key, SymmetricAlgAction::ENCRYPT); }
		std::vector<BYTE> decrypt(const vector<BYTE>& data, const vector<BYTE>& key) { return perfomAction(data, key, SymmetricAlgAction::DECRYPT); }

		~SymmetricEncryptionAlgorithm() {
			BCryptCloseAlgorithmProvider(hSymmetricAlg, 0);
		}
	};

	/* Алгоритм вычисления Retail MAC использует DES, а поэтому его наследует */
	class RetailMAC : private SymmetricEncryptionAlgorithm<SymmetricAlgName::DES, SymmetricAlgMode::ECB> {
	public:
		RetailMAC() : SymmetricEncryptionAlgorithm() {}

		/* Вычислить MAC значение */
		vector<BYTE> getMAC(const vector<BYTE>& data, const vector<BYTE>& key) {
			/* Если размер ключа не равен 16 байтам, то беда */
			if (key.size() != 16) {
				cerr << "Ошибка: длина ключа для Retail MAC должна быть равна 16 байтам" << endl;
				return {};
			}
			
			/* Заполняем данные по стандарту */
			vector<BYTE> paddedData = fillISO9797(data, cbBlockLen);

			/* Выровняв данные, можно приступать к вычислению MAC */
			/* Сначала выделим два подключа */
			vector<BYTE> key0(key.begin(), key.begin() + cbBlockLen);
			vector<BYTE> key1(key.begin() + cbBlockLen, key.begin() + 2 * cbBlockLen);

			/* Затем разметим область памяти под временные блоки */
			vector<BYTE> block(cbBlockLen, 0);
			vector<BYTE> output(cbBlockLen, 0);

			/* В цикле обернёмся столько раз, сколько умещается размеров блока в размере выровненных данных */
			for (size_t i = 0; i < paddedData.size() / cbBlockLen; i += 1) {
				/* Выполняем XOR временного блока данных и выходного блока */
				for (size_t j = 0; j < cbBlockLen; j += 1) {
					output[j] = output[j] ^ paddedData[i * cbBlockLen + j];
				}

				/* Проксорив блоки, можно выполнить этап шифрования */
				output = encrypt(output, key0);
			}

			/* После выполнения всех циклов шифрования, завершающий этап */
			output = decrypt(output, key1);
			output = encrypt(output, key0);

			/* И возврат исходных данных */
			return output;
		}
	};

	typedef SymmetricEncryptionAlgorithm<SymmetricAlgName::TripleDES, SymmetricAlgMode::CBC> Des3;
	typedef HashingAlgorithm<HashAlgName::SHA1> Sha1;
}

#ifdef TEST_CRYPTO
#include <cassert>

/* Тесты из Информативного дополнения 6 части 1 тома 2 документа 9303 */
void testParityCorrect() {
	/* Лямбда-функция, которая выполняет тестирование на совпадение откорректированных битов с ожидаемым */
	auto testParity = [](vector<BYTE> key, vector<BYTE> expected) {
		assert(correctParityBits(key) == expected);
	};

	/* Сами прогоны на тестовых данных */
	testParity({ 0xAB, 0x94, 0xFC, 0xED, 0xF2, 0x66, 0x4E, 0xDF }, { 0xAB, 0x94, 0xFD, 0xEC, 0xF2, 0x67, 0x4F, 0xDF });
	testParity({ 0xB9, 0xB2, 0x91, 0xF8, 0x5D, 0x7F, 0x77, 0xF2 }, { 0xB9, 0xB3, 0x91, 0xF8, 0x5D, 0x7F, 0x76, 0xF2 });
	testParity({ 0x78, 0x62, 0xD9, 0xEC, 0xE0, 0x3C, 0x1B, 0xCD }, { 0x79, 0x62, 0xD9, 0xEC, 0xE0, 0x3D, 0x1A, 0xCD });
	testParity({ 0x4D, 0x77, 0x08, 0x9D, 0xCF, 0x13, 0x14, 0x42 }, { 0x4C, 0x76, 0x08, 0x9D, 0xCE, 0x13, 0x15, 0x43 });

	cout << "Проверка корректировки битов чётности выполнена успешно!" << endl;
}

/* Тесты взяты с Wikipedia */
void testHashingSha1() {
	/* Класс для проведения хэширования */
	Crypto::Sha1 sha1;

	/* Функция проведения тестирования хэширования */
	auto testHashing = [&](string dataStr, vector<BYTE> expected) {
		/* Переводит строку из string в vector */
		vector<BYTE> data(dataStr.size(), 0);
		copy(dataStr.c_str(), dataStr.c_str() + dataStr.size(), data.begin());

		/* Считает хэш */
		vector<BYTE> output = sha1.getHash(data);
		assert(output == expected);
	};

	/* Прогоны тестовых данных */
	testHashing("The quick brown fox jumps over the lazy dog", { 0x2f, 0xd4, 0xe1, 0xc6, 0x7a, 0x2d, 0x28, 0xfc, 0xed, 0x84, 0x9e, 0xe1, 0xbb, 0x76, 0xe7, 0x39, 0x1b, 0x93, 0xeb, 0x12 });
	testHashing("sha", { 0xd8, 0xf4, 0x59, 0x03, 0x20, 0xe1, 0x34, 0x3a, 0x91, 0x5b, 0x63, 0x94, 0x17, 0x06, 0x50, 0xa8, 0xf3, 0x5d, 0x69, 0x26 });
	testHashing("Sha", { 0xba, 0x79, 0xba, 0xeb, 0x9f, 0x10, 0x89, 0x6a, 0x46, 0xae, 0x74, 0x71, 0x52, 0x71, 0xb7, 0xf5, 0x86, 0xe7, 0x46, 0x40 });
	testHashing("", { 0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d, 0x32, 0x55, 0xbf, 0xef, 0x95, 0x60, 0x18, 0x90, 0xaf, 0xd8, 0x07, 0x09 });
}

/* Тест аутентификации из Информативного дополнения 6 части 1 тома 2 документа 9303  */
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