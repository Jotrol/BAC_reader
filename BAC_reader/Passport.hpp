#pragma once

#include "APDU.hpp"
#include "CardReader.hpp"
#include "MRZDecoder.hpp"
#include "BerTLV.hpp"
#include "Crypto.hpp"

#include <vector>
#include <fstream>
#include <random>
#include <ctime>
#include <iostream>
using namespace std;

class Passport {
private:
	/* Константы и "полу-константы" */
	const APDU::APDU SelectApp = APDU::APDU(0x00, 0xA4, 0x04, 0x0C, { 0x07, 0xA0, 0x00, 0x00, 0x02, 0x47, 0x10, 0x01 });
	const APDU::APDU GetICC = APDU::APDU(0x00, 0x84, 0x00, 0x00, {}, 0x08);
	const APDU::APDU ExtAuth = APDU::APDU(0x00, 0x82, 0x00, 0x00, {}, 0x28);
	const APDU::APDU SelectFile = APDU::APDU(0x00, 0xA4, 0x02, 0x0C);
	const APDU::APDU ReadBinary = APDU::APDU(0x00, 0xB0, 0x00, 0x00);

	Crypto::Des3& des3 = Crypto::getDes3Alg();
	Crypto::RetailMAC& mac = Crypto::getMacAlg();
	Crypto::Sha1 sha1 = Crypto::getSha1Alg();

public:
	typedef basic_fstream<UINT8> ByteStream;

	enum DGFILES { EFCOM, DG1, DG2, DG_COUNT };

private:
	bool isConnectedViaBAC;
	SCARDHANDLE hCard;
	UINT64 SSC;

	ByteVec kEnc;
	ByteVec kMac;

	ByteStream dgFile[DGFILES::DG_COUNT];
	ByteVec dgID[DGFILES::DG_COUNT];
	UINT8 dgTag[DGFILES::DG_COUNT];

	pair<ByteVec, ByteVec> generateKeys(const ByteVec& kSeed) {
		ByteVec dEnc = kSeed;
		dEnc.insert(dEnc.end(), { 0x00, 0x00, 0x00, 0x01 });

		ByteVec dMac = kSeed;
		dMac.insert(dMac.end(), { 0x00, 0x00, 0x00, 0x02 });

		dEnc = sha1.getHash(dEnc);
		dEnc.resize(16);

		dMac = sha1.getHash(dMac);
		dMac.resize(16);

		return make_pair(dEnc, dMac);
	}
	ByteVec generateBytes(UINT32 size) {
		ByteVec output(size, 0);
		for (UINT i = 0; i < size; i += 1) {
			output[i] = rand() % 255;
		}
		return output;
	}
	ByteVec encryptS(const ByteVec& rndIFD, const ByteVec& rndICC, const ByteVec& kIFD) {
		/* Конкатенируем всё для S */
		ByteVec S = {};
		S.insert(S.end(), rndIFD.begin(), rndIFD.end());
		S.insert(S.end(), rndICC.begin(), rndICC.end());
		S.insert(S.end(), kIFD.begin(), kIFD.end());

		/* Шифруем в итоге rndIFD | rndICC | kIFD */
		S = des3.encrypt(S, kEnc);

		/* Размер может получится больше, а нам нужны только 32 байта */
		//S.resize(32);
		return S;
	}
public:
	Passport() {
		/* Создаём временные файлы для каждой группы данных */
		/* Может быть пределано в создание временных файлов */
		dgFile[DGFILES::DG1] = ByteStream("DG1.bin", ios::binary | ios::in | ios::out | ios::trunc);
		dgFile[DGFILES::DG2] = ByteStream("DG2.bin", ios::binary | ios::in | ios::out | ios::trunc);

		/* Заполняем идентификаторы файлов */
		dgID[DGFILES::DG1] = { 0x01, 0x01 };
		dgID[DGFILES::DG2] = { 0x01, 0x02 };

		/* Заполняем стартовые теги файлов */
		dgTag[DGFILES::DG1] = 0x61;
		dgTag[DGFILES::DG2] = 0x75;

		/* Заполняем идентификаторы файлов EF */
		dgFile[DGFILES::EFCOM] = ByteStream("EF_COM.bin", ios::binary | ios::in | ios::out | ios::trunc);
		dgID[DGFILES::EFCOM] = { 0x01, 0x1E };
		dgTag[DGFILES::EFCOM] = 0x60;

		/* Флаг говорящий о том, подключен соединен ли паспорт по BAC */
		isConnectedViaBAC = false;

		/* Дескриптор карты по умолчанию */
		hCard = 0;
	}

	/* Функция соединения паспорта с кардридером, не BAC! */
	bool connectPassport(const CardReader& reader, const wstring& readerName) {
		hCard = reader.cardConnect(readerName);
		if (hCard == 0) {
			cerr << "Ошибка: не удалось соединиться с картой" << endl;
			return false;
		}
		return true;
	}

	/* Функция отключения паспорта от ридера */
	bool disconnectPassport(const CardReader& reader) {
		/* Ставим флажок, чтобы нельзя было считать данные */
		isConnectedViaBAC = false;

		/* Закрыываем все файлы групп данных */
		for (UINT i = 0; i < DGFILES::DG_COUNT; i += 1) {
			dgFile[i].close();
		}

		/* Отключаем паспорт */
		return reader.cardDisconnect(hCard);
	}

	/* Функция выполнения соединения по протоколу Basic Access Control */
	bool perfomBAC(const CardReader& reader, string mrzLine2) {
		/* Временные переменные для хранения данных */
		/* Необходимых для подключения по BAC */
		APDU::APDU tempAPDU;
		APDU::RAPDU tempRAPDU;

		ByteVec tempVector1 = {};
		ByteVec tempVector2 = {};

		/* Необходимо для получения SSC */
		ByteVec rICC = {};
		ByteVec rIFD = {};
		ByteVec kIFD = {};

		/* Генерируем инициализирующий хэш из данных MRZ */
		DecoderMRZLine2 decoderMRZLine2(mrzLine2);
		tempVector1 = sha1.getHash(decoderMRZLine2.ComposeData());
		tempVector1.resize(16);

		/* Генерируем ключи для обмена */
		auto keyPair = generateKeys(tempVector1);
		kEnc = keyPair.first;
		kMac = keyPair.second;

		/* Выбираем приложение */
		tempVector1 = reader.sendCommand(hCard, SelectApp.getCommand());
		tempRAPDU = APDU::RAPDU(tempVector1);
		if (!tempRAPDU.isResponceOK()) {
			cerr << "Ошибка: произошла ошибка в SelectApp: " << tempRAPDU.report() << endl;
			return false;
		}

		/* Получаем случайное число */
		tempVector1 = reader.sendCommand(hCard, GetICC.getCommand());
		tempRAPDU = APDU::RAPDU(tempVector1);
		if (!tempRAPDU.isResponceOK()) {
			cerr << "Ошибка: не удалось получить RICC: " << tempRAPDU.report() << endl;
			return false;
		}

		/* Получив случайное число от МСПД, сами генерируем случайные числа */
		/* И, добавив случайное число от МСПД, шифруем результат */
		/* В tempRAPDU.data содержится случайное число от МСПД */
		rIFD = generateBytes(8);			/* Это rndIFD */
		kIFD = generateBytes(16);			/* Это kIFD */
		rICC = tempRAPDU.getResponceData(); /* Это rICC */

		tempVector1 = encryptS(rIFD, rICC, kIFD);

		/* Генрируем MAC от зашифрованного */
		tempVector2 = mac.getMAC(tempVector1, kMac);

		/* Добавим к E_IFD 8 байт MAC[kMac](E_IFD) - получится 40 байт */
		tempVector1.insert(tempVector1.end(), tempVector2.begin(), tempVector2.end());

		/* Посылаем cmdData в виде APDU команды EXTERNAL_AUTHENTICATE */
		/* Копируем APDU команду чтобы её поменять */
		tempAPDU = ExtAuth;

		/* Заполняем данные этой команды */
		tempAPDU.appendCommandData({ 0x28 });
		tempAPDU.appendCommandData(tempVector1);

		/* Отправляем команду на карту и получаем ответ*/
		tempVector1 = reader.sendCommand(hCard, tempAPDU.getCommand());
		tempRAPDU = APDU::RAPDU(tempVector1);
		if (!tempRAPDU.isResponceOK()) {
			cerr << "Ошибка: не удалось отправить CmdData: " << tempRAPDU.report() << endl;
			return false;
		}

		/* Проверка ответа на команду EXTERNAL_AUTHENTICATE */
		/* Получаем данные ответа */
		tempVector1 = tempRAPDU.getResponceData();

		/* Выделяем из ответа нижние и верхние 32 байта */
		tempVector2 = vector<UINT8>(tempVector1.begin() + 32, tempVector1.end());
		tempVector1.resize(32);

		/* Считаем MAC от верхних 32 байт */
		tempVector1 = mac.getMAC(tempVector1, kMac);
		if (tempVector1 != tempVector2) {
			cerr << "Ошибка: не совпал MAC в respData: " << endl;
			return false;
		}

		/* Получаем данные ответа заново */
		tempVector1 = tempRAPDU.getResponceData();

		/* Нужны только верхние 32 байта */
		tempVector1.resize(32);

		/* Дешифруем эти 32 байта */
		tempVector1 = des3.decrypt(tempVector1, kEnc);

		/* Проверяем, что получили то же rndIFD */
		tempVector2 = ByteVec(tempVector1.begin() + 8, tempVector1.begin() + 16);
		if (tempVector2 != rIFD) {
			cerr << "Ошибка: rndIFD не одинаков" << endl;
			return false;
		}

		/* Выделяем новый kSeed  */
		tempVector2 = ByteVec(tempVector1.begin() + 16, tempVector1.end());
		for (UINT i = 0; i < tempVector2.size(); i += 1) {
			tempVector2[i] = tempVector2[i] ^ kIFD[i];
		}

		/* Генерируем новые ключи по полученному kSeed */
		keyPair = generateKeys(tempVector2);
		kEnc = keyPair.first;
		kMac = keyPair.second;

		/* Получаем начальное значение счётчика */
		SSC = ((UINT64)rICC[4] << 56) | ((UINT64)rICC[5] << 48) | ((UINT64)rICC[6] << 40) | ((UINT64)rICC[7] << 32) | ((UINT64)rIFD[4] << 24) | ((UINT64)rIFD[5] << 16) | ((UINT64)rIFD[6] << 8) | rIFD[7];

		/* Ура, BAC состоялся */
		isConnectedViaBAC = true;
		return true;
	}

	bool readDG(const CardReader& reader, DGFILES file) {
		/* Если не было установлено соединение - то нельзя и считать данные */
		if (!isConnectedViaBAC) {
			cerr << "Ошибка: нет соединения по BAC" << endl;
			return false;
		}

		APDU::APDU apdu;
		APDU::RAPDU rapdu;

		APDU::SecureAPDU secAPDU;
		APDU::SecureRAPDU secRAPDU;

		ByteVec tempVector1 = {};

		UINT16 fileLen = 0x00;
		UINT16 fileOff = 0x00;

		/* Выбираем файл */
		apdu = SelectFile;
		apdu.appendCommandData(dgID[file]);

		/* Конструируем защищённую команду и отправляем */
		secAPDU = APDU::SecureAPDU(apdu);
		tempVector1 = reader.sendCommand(hCard, secAPDU.generateRawSecureAPDU(kEnc, kMac, SSC));

		/* Получаем ответ и проверям его */
		secRAPDU = APDU::SecureRAPDU(tempVector1, kEnc, kMac, SSC);
		if (!secRAPDU.isResponceOK()) {
			cerr << "Ошибка: не удалось отправить команду Select: " << secRAPDU.report() << endl;
			return false;
		}

		/* Считываем первые 15 байт (ну или сколько сможет дать) чтобы узнать размер файла */
		apdu = ReadBinary;
		secAPDU = APDU::SecureAPDU(apdu);
		secAPDU.setLe(0x0F);

		tempVector1 = reader.sendCommand(hCard, secAPDU.generateRawSecureAPDU(kEnc, kMac, SSC));
		secRAPDU = APDU::SecureRAPDU(tempVector1, kEnc, kMac, SSC);
		if (!secRAPDU.isResponceOK()) {
			cerr << "Ошибка: не удалось считать длину файла: " << secRAPDU.report() << endl;
			return false;
		}

		/* Записываем полученные байты в файл группы данных */
		tempVector1 = secRAPDU.getDecodedResponceData();
		dgFile[file].write(tempVector1.data(), tempVector1.size());
		dgFile[file].flush();

		/* 2 байта размера - это минимум для DG1, например */
		if (tempVector1.size() < 0x02) {
			cerr << "Ошибка: не удалось узнать длину файла" << endl;
			return false;
		}

		/* Получаем длину файла с проверкой первого тега */
		fileLen = BerTLV::BerDecodeLenBytes(tempVector1, dgTag[file]);
		if (fileLen == -1) {
			cerr << "Ошибка: неверная длина файла" << endl;
			return false;
		}

		/* Начинаем считывание всего файла */
		fileOff = tempVector1.size();
		while (fileOff < fileLen) {
			/* Создаём команду для отправки */
			apdu = ReadBinary;
			apdu.setCommandField(APDU::APDU::Fields::P1, (fileOff >> 8) & 0xFF);
			apdu.setCommandField(APDU::APDU::Fields::P2, fileOff & 0xFF);
			apdu.setLe(min(0x0F, fileLen - fileOff));

			/* Формируем защищённую команду и отправляем её */
			secAPDU = APDU::SecureAPDU(apdu);
			tempVector1 = reader.sendCommand(hCard, secAPDU.generateRawSecureAPDU(kEnc, kMac, SSC));

			/* Получаем ответ и производим его проверку */
			secRAPDU = APDU::SecureRAPDU(tempVector1, kEnc, kMac, SSC);
			if (!secRAPDU.isResponceOK()) {
				cerr << "Ошибка: не удалось считать файл: " << secRAPDU.report() << endl;
				return false;
			}

			/* Записываем расшифрованный ответ в файл */
			tempVector1 = secRAPDU.getDecodedResponceData();
			dgFile[file].write(tempVector1.data(), tempVector1.size());
			dgFile[file].flush();

			/* Увеличиваем смещение файла */
			fileOff += tempVector1.size();
		}
		return true;
	}
};