#pragma once

#include <CommCtrl.h>

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
	Crypto::Sha1& sha1 = Crypto::getSha1Alg();

public:
	enum DGFILES { EFCOM, DG1, DG2, DG_COUNT };

private:
	bool isConnectedViaBAC;
	SCARDHANDLE hCard;
	UINT64 SSC;

	ByteVec kEnc;
	ByteVec kMac;

	ByteStream dgFile[DGFILES::DG_COUNT];
	string dgFileName[DGFILES::DG_COUNT];
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
		return S;
	}
public:
	Passport() {
		/* Заполняем имена файлов групп данных */
		dgFileName[DGFILES::DG1] = "DG1.bin";
		dgFileName[DGFILES::DG2] = "DG2.bin";
		dgFileName[DGFILES::EFCOM] = "EF_COM.bin";

		/* Заполняем идентификаторы файлов */
		dgID[DGFILES::DG1] = { 0x01, 0x01 };
		dgID[DGFILES::DG2] = { 0x01, 0x02 };
		dgID[DGFILES::EFCOM] = { 0x01, 0x1E };

		/* Заполняем стартовые теги файлов */
		dgTag[DGFILES::DG1] = 0x61;
		dgTag[DGFILES::DG2] = 0x75;
		dgTag[DGFILES::EFCOM] = 0x60;

		/* Флаг говорящий о том, подключен соединен ли паспорт по BAC */
		isConnectedViaBAC = false;

		/* Дескриптор карты по умолчанию */
		hCard = 0;
	}

	/* Функция соединения паспорта с кардридером, не BAC! */
	void connectPassport(const CardReader& reader, const wstring& readerName) {
		hCard = reader.cardConnect(readerName);
		if (hCard == 0) {
			throw std::exception("Ошибка: не удалось соединиться с картой");
		}
	}

	/* Функция отключения паспорта от ридера */
	void disconnectPassport(const CardReader& reader) {
		/* Ставим флажок, чтобы нельзя было считать данные */
		isConnectedViaBAC = false;

		/* Закрыываем все файлы групп данных */
		for (UINT i = 0; i < DGFILES::DG_COUNT; i += 1) {
			dgFile[i].close();
		}

		/* Отключаем паспорт */
		reader.cardDisconnect(hCard);
	}

	/* Функция выполнения соединения по протоколу Basic Access Control */
	void perfomBAC(const CardReader& reader, string mrzLine2) {
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
		Line2MRZDecoder decoderMRZLine2(mrzLine2);
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
			throw std::exception(("Ошибка: произошла ошибка в SelectApp: " + tempRAPDU.report()).c_str());
		}

		/* Получаем случайное число */
		tempVector1 = reader.sendCommand(hCard, GetICC.getCommand());
		tempRAPDU = APDU::RAPDU(tempVector1);
		if (!tempRAPDU.isResponceOK()) {
			throw std::exception(("Ошибка: не удалось получить RICC: " + tempRAPDU.report()).c_str());
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
			throw std::exception(("Ошибка: не удалось отправить CmdData: " + tempRAPDU.report()).c_str());
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
			throw std::exception("Ошибка: не совпал MAC в respData: ");
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
			throw std::exception("Ошибка: rndIFD не одинаков");
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
	}

	void readDG(const CardReader& reader, DGFILES file, HWND progressBarHwnd = nullptr) {
		/* Если не было установлено соединение - то нельзя и считать данные */
		if (!isConnectedViaBAC) {
			throw std::exception("Ошибка: нет соединения по BAC");
		}

		APDU::APDU apdu;
		APDU::RAPDU rapdu;

		APDU::SecureAPDU secAPDU;
		APDU::SecureRAPDU secRAPDU;

		ByteVec tempVector1 = {};

		UINT16 fileLen = 0x00;
		UINT16 fileOff = 0x00;

		/* Открываем считываемый файл по новой */
#ifdef _DEBUG
		dgFile[file] = ByteStream(dgFileName[file], ios::in | ios::out | ios::binary | ios::trunc);
#else
		/* В случае временных файлов, сначала надо закрыть предыдущий (чтобы он удалился) */
		dgFile[file].close();

		/* И открыть новый временный файл */
		dgFile[file] = ByteStream(tmpfile());
#endif

		/* Выбираем файл */
		apdu = SelectFile;
		apdu.appendCommandData(dgID[file]);

		/* Конструируем защищённую команду и отправляем */
		secAPDU = APDU::SecureAPDU(apdu);
		tempVector1 = reader.sendCommand(hCard, secAPDU.generateRawSecureAPDU(kEnc, kMac, SSC));

		/* Получаем ответ и проверям его */
		secRAPDU = APDU::SecureRAPDU(tempVector1, kEnc, kMac, SSC);
		if (!secRAPDU.isResponceOK()) {
			throw std::exception(("Ошибка: не удалось отправить команду Select: " + secRAPDU.report()).c_str());
		}

		/* Считываем первые 15 байт (ну или сколько сможет дать) чтобы узнать размер файла */
		apdu = ReadBinary;
		secAPDU = APDU::SecureAPDU(apdu);
		secAPDU.setLe(0x0F);

		tempVector1 = reader.sendCommand(hCard, secAPDU.generateRawSecureAPDU(kEnc, kMac, SSC));
		secRAPDU = APDU::SecureRAPDU(tempVector1, kEnc, kMac, SSC);
		if (!secRAPDU.isResponceOK()) {
			throw std::exception(("Ошибка: не удалось считать длину файла: " + secRAPDU.report()).c_str());
		}

		/* Записываем полученные байты в файл группы данных */
		tempVector1 = secRAPDU.getDecodedResponceData();
		dgFile[file].write(tempVector1.data(), tempVector1.size());
		dgFile[file].flush();

		/* 2 байта размера - это минимум для DG1, например */
		if (tempVector1.size() < 0x02) {
			throw std::exception("Ошибка: не удалось узнать длину файла");
		}

		/* Получаем длину файла с проверкой первого тега */
		fileLen = BerTLV::BerDecodeLenBytes(tempVector1, dgTag[file]);
		if (fileLen == -1) {
			throw std::exception("Ошибка: неверная длина файла");
		}

		/* Начинаем считывание всего файла */
		fileOff = tempVector1.size();

		if (progressBarHwnd) {
			SendMessage(progressBarHwnd, PBM_SETRANGE, 0, MAKELPARAM(0, fileLen - fileOff));
			SendMessage(progressBarHwnd, PBM_SETSTEP, (WPARAM)0xF, 0);
		}
		
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
				throw std::exception(("Ошибка: не удалось считать файл: " + secRAPDU.report()).c_str());
			}

			/* Записываем расшифрованный ответ в файл */
			tempVector1 = secRAPDU.getDecodedResponceData();
			dgFile[file].write(tempVector1.data(), tempVector1.size());
			dgFile[file].flush();

			/* Увеличиваем смещение файла */
			fileOff += tempVector1.size();

			if (progressBarHwnd) {
				SendMessage(progressBarHwnd, PBM_STEPIT, 0, 0);
			}
		}
	}

	ByteVec getPassportRawImage() {
		if (!dgFile[DGFILES::DG2].is_open()) {
			throw std::exception("Ошибка: файл DG2 не считан!");
		}

		BerTLV::BerTLVDecoder decoder;
		UINT16 rootIndex = decoder.decode(dgFile[DGFILES::DG2]);
		
		INT16 tag0x75 = decoder.getChildByTag(rootIndex, 0x75);
		if (tag0x75 == -1) {
			throw std::exception("Ошибка: не удалось получить тег 0x75");
		}

		INT16 tag0x7F61 = decoder.getChildByTag(tag0x75, 0x7F61);
		if (tag0x7F61 == -1) {
			throw std::exception("Ошибка: не удалось получить тег 0x7F61");
		}

		INT16 tag0x7F60 = decoder.getChildByTag(tag0x7F61, 0x7F60);
		if (tag0x7F60 == -1) {
			throw std::exception("Ошибка: не удалось получить тег 0x7F60");
		}

		INT16 tagImage = decoder.getChildByTag(tag0x7F60, 0x7F2E);
		if (tagImage == -1) {
			tagImage = decoder.getChildByTag(tag0x7F60, 0x5F2E);
			if (tagImage == -1) {
				throw std::exception("Ошибка: не удалось получить изображение по тегам 0x7F2E и 0x5F2E");
			}
		}

		return decoder.getTokenData(dgFile[DGFILES::DG2], tagImage);
	}

	ByteVec getPassportMRZ() {
		if (!dgFile[DGFILES::DG1].is_open()) {
			throw std::exception("Ошибка: файл DG1 не считан!");
		}

		BerTLV::BerTLVDecoder decoder;
		UINT16 rootIndex = decoder.decode(dgFile[DGFILES::DG1]);

		INT16 tag0x61 = decoder.getChildByTag(rootIndex, 0x61);
		if (tag0x61 == -1) {
			throw std::exception("Ошибка: не удалось обнаружить тег 0x61");
		}

		INT16 tag0x5F1F = decoder.getChildByTag(tag0x61, 0x5F1F);
		if (tag0x5F1F == -1) {
			throw std::exception("Ошибка: не удалось обнаружить тег 0x5F1F");
		}

		return decoder.getTokenData(dgFile[DGFILES::DG1], tag0x5F1F);
	}
};