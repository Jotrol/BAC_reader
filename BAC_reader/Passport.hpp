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
	/* ��������� � "����-���������" */
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
		/* ������������� �� ��� S */
		ByteVec S = {};
		S.insert(S.end(), rndIFD.begin(), rndIFD.end());
		S.insert(S.end(), rndICC.begin(), rndICC.end());
		S.insert(S.end(), kIFD.begin(), kIFD.end());

		/* ������� � ����� rndIFD | rndICC | kIFD */
		S = des3.encrypt(S, kEnc);
		return S;
	}
public:
	Passport() {
		/* ��������� ����� ������ ����� ������ */
		dgFileName[DGFILES::DG1] = "DG1.bin";
		dgFileName[DGFILES::DG2] = "DG2.bin";
		dgFileName[DGFILES::EFCOM] = "EF_COM.bin";

		/* ��������� �������������� ������ */
		dgID[DGFILES::DG1] = { 0x01, 0x01 };
		dgID[DGFILES::DG2] = { 0x01, 0x02 };
		dgID[DGFILES::EFCOM] = { 0x01, 0x1E };

		/* ��������� ��������� ���� ������ */
		dgTag[DGFILES::DG1] = 0x61;
		dgTag[DGFILES::DG2] = 0x75;
		dgTag[DGFILES::EFCOM] = 0x60;

		/* ���� ��������� � ���, ��������� �������� �� ������� �� BAC */
		isConnectedViaBAC = false;

		/* ���������� ����� �� ��������� */
		hCard = 0;
	}

	/* ������� ���������� �������� � �����������, �� BAC! */
	void connectPassport(const CardReader& reader, const wstring& readerName) {
		hCard = reader.cardConnect(readerName);
		if (hCard == 0) {
			throw std::exception("������: �� ������� ����������� � ������");
		}
	}

	/* ������� ���������� �������� �� ������ */
	void disconnectPassport(const CardReader& reader) {
		/* ������ ������, ����� ������ ���� ������� ������ */
		isConnectedViaBAC = false;

		/* ���������� ��� ����� ����� ������ */
		for (UINT i = 0; i < DGFILES::DG_COUNT; i += 1) {
			dgFile[i].close();
		}

		/* ��������� ������� */
		reader.cardDisconnect(hCard);
	}

	/* ������� ���������� ���������� �� ��������� Basic Access Control */
	void perfomBAC(const CardReader& reader, string mrzLine2) {
		/* ��������� ���������� ��� �������� ������ */
		/* ����������� ��� ����������� �� BAC */
		APDU::APDU tempAPDU;
		APDU::RAPDU tempRAPDU;

		ByteVec tempVector1 = {};
		ByteVec tempVector2 = {};

		/* ���������� ��� ��������� SSC */
		ByteVec rICC = {};
		ByteVec rIFD = {};
		ByteVec kIFD = {};

		/* ���������� ���������������� ��� �� ������ MRZ */
		Line2MRZDecoder decoderMRZLine2(mrzLine2);
		tempVector1 = sha1.getHash(decoderMRZLine2.ComposeData());
		tempVector1.resize(16);

		/* ���������� ����� ��� ������ */
		auto keyPair = generateKeys(tempVector1);
		kEnc = keyPair.first;
		kMac = keyPair.second;

		/* �������� ���������� */
		tempVector1 = reader.sendCommand(hCard, SelectApp.getCommand());
		tempRAPDU = APDU::RAPDU(tempVector1);
		if (!tempRAPDU.isResponceOK()) {
			throw std::exception(("������: ��������� ������ � SelectApp: " + tempRAPDU.report()).c_str());
		}

		/* �������� ��������� ����� */
		tempVector1 = reader.sendCommand(hCard, GetICC.getCommand());
		tempRAPDU = APDU::RAPDU(tempVector1);
		if (!tempRAPDU.isResponceOK()) {
			throw std::exception(("������: �� ������� �������� RICC: " + tempRAPDU.report()).c_str());
		}

		/* ������� ��������� ����� �� ����, ���� ���������� ��������� ����� */
		/* �, ������� ��������� ����� �� ����, ������� ��������� */
		/* � tempRAPDU.data ���������� ��������� ����� �� ���� */
		rIFD = generateBytes(8);			/* ��� rndIFD */
		kIFD = generateBytes(16);			/* ��� kIFD */
		rICC = tempRAPDU.getResponceData(); /* ��� rICC */

		tempVector1 = encryptS(rIFD, rICC, kIFD);

		/* ��������� MAC �� �������������� */
		tempVector2 = mac.getMAC(tempVector1, kMac);

		/* ������� � E_IFD 8 ���� MAC[kMac](E_IFD) - ��������� 40 ���� */
		tempVector1.insert(tempVector1.end(), tempVector2.begin(), tempVector2.end());

		/* �������� cmdData � ���� APDU ������� EXTERNAL_AUTHENTICATE */
		/* �������� APDU ������� ����� � �������� */
		tempAPDU = ExtAuth;

		/* ��������� ������ ���� ������� */
		tempAPDU.appendCommandData({ 0x28 });
		tempAPDU.appendCommandData(tempVector1);

		/* ���������� ������� �� ����� � �������� �����*/
		tempVector1 = reader.sendCommand(hCard, tempAPDU.getCommand());
		tempRAPDU = APDU::RAPDU(tempVector1);
		if (!tempRAPDU.isResponceOK()) {
			throw std::exception(("������: �� ������� ��������� CmdData: " + tempRAPDU.report()).c_str());
		}

		/* �������� ������ �� ������� EXTERNAL_AUTHENTICATE */
		/* �������� ������ ������ */
		tempVector1 = tempRAPDU.getResponceData();

		/* �������� �� ������ ������ � ������� 32 ����� */
		tempVector2 = vector<UINT8>(tempVector1.begin() + 32, tempVector1.end());
		tempVector1.resize(32);

		/* ������� MAC �� ������� 32 ���� */
		tempVector1 = mac.getMAC(tempVector1, kMac);
		if (tempVector1 != tempVector2) {
			throw std::exception("������: �� ������ MAC � respData: ");
		}

		/* �������� ������ ������ ������ */
		tempVector1 = tempRAPDU.getResponceData();

		/* ����� ������ ������� 32 ����� */
		tempVector1.resize(32);

		/* ��������� ��� 32 ����� */
		tempVector1 = des3.decrypt(tempVector1, kEnc);

		/* ���������, ��� �������� �� �� rndIFD */
		tempVector2 = ByteVec(tempVector1.begin() + 8, tempVector1.begin() + 16);
		if (tempVector2 != rIFD) {
			throw std::exception("������: rndIFD �� ��������");
		}

		/* �������� ����� kSeed  */
		tempVector2 = ByteVec(tempVector1.begin() + 16, tempVector1.end());
		for (UINT i = 0; i < tempVector2.size(); i += 1) {
			tempVector2[i] = tempVector2[i] ^ kIFD[i];
		}

		/* ���������� ����� ����� �� ����������� kSeed */
		keyPair = generateKeys(tempVector2);
		kEnc = keyPair.first;
		kMac = keyPair.second;

		/* �������� ��������� �������� �������� */
		SSC = ((UINT64)rICC[4] << 56) | ((UINT64)rICC[5] << 48) | ((UINT64)rICC[6] << 40) | ((UINT64)rICC[7] << 32) | ((UINT64)rIFD[4] << 24) | ((UINT64)rIFD[5] << 16) | ((UINT64)rIFD[6] << 8) | rIFD[7];

		/* ���, BAC ��������� */
		isConnectedViaBAC = true;
	}

	void readDG(const CardReader& reader, DGFILES file, HWND progressBarHwnd = nullptr) {
		/* ���� �� ���� ����������� ���������� - �� ������ � ������� ������ */
		if (!isConnectedViaBAC) {
			throw std::exception("������: ��� ���������� �� BAC");
		}

		APDU::APDU apdu;
		APDU::RAPDU rapdu;

		APDU::SecureAPDU secAPDU;
		APDU::SecureRAPDU secRAPDU;

		ByteVec tempVector1 = {};

		UINT16 fileLen = 0x00;
		UINT16 fileOff = 0x00;

		/* ��������� ����������� ���� �� ����� */
#ifdef _DEBUG
		dgFile[file] = ByteStream(dgFileName[file], ios::in | ios::out | ios::binary | ios::trunc);
#else
		/* � ������ ��������� ������, ������� ���� ������� ���������� (����� �� ��������) */
		dgFile[file].close();

		/* � ������� ����� ��������� ���� */
		dgFile[file] = ByteStream(tmpfile());
#endif

		/* �������� ���� */
		apdu = SelectFile;
		apdu.appendCommandData(dgID[file]);

		/* ������������ ���������� ������� � ���������� */
		secAPDU = APDU::SecureAPDU(apdu);
		tempVector1 = reader.sendCommand(hCard, secAPDU.generateRawSecureAPDU(kEnc, kMac, SSC));

		/* �������� ����� � �������� ��� */
		secRAPDU = APDU::SecureRAPDU(tempVector1, kEnc, kMac, SSC);
		if (!secRAPDU.isResponceOK()) {
			throw std::exception(("������: �� ������� ��������� ������� Select: " + secRAPDU.report()).c_str());
		}

		/* ��������� ������ 15 ���� (�� ��� ������� ������ ����) ����� ������ ������ ����� */
		apdu = ReadBinary;
		secAPDU = APDU::SecureAPDU(apdu);
		secAPDU.setLe(0x0F);

		tempVector1 = reader.sendCommand(hCard, secAPDU.generateRawSecureAPDU(kEnc, kMac, SSC));
		secRAPDU = APDU::SecureRAPDU(tempVector1, kEnc, kMac, SSC);
		if (!secRAPDU.isResponceOK()) {
			throw std::exception(("������: �� ������� ������� ����� �����: " + secRAPDU.report()).c_str());
		}

		/* ���������� ���������� ����� � ���� ������ ������ */
		tempVector1 = secRAPDU.getDecodedResponceData();
		dgFile[file].write(tempVector1.data(), tempVector1.size());
		dgFile[file].flush();

		/* 2 ����� ������� - ��� ������� ��� DG1, �������� */
		if (tempVector1.size() < 0x02) {
			throw std::exception("������: �� ������� ������ ����� �����");
		}

		/* �������� ����� ����� � ��������� ������� ���� */
		fileLen = BerTLV::BerDecodeLenBytes(tempVector1, dgTag[file]);
		if (fileLen == -1) {
			throw std::exception("������: �������� ����� �����");
		}

		/* �������� ���������� ����� ����� */
		fileOff = tempVector1.size();

		if (progressBarHwnd) {
			SendMessage(progressBarHwnd, PBM_SETRANGE, 0, MAKELPARAM(0, fileLen - fileOff));
			SendMessage(progressBarHwnd, PBM_SETSTEP, (WPARAM)0xF, 0);
		}
		
		while (fileOff < fileLen) {
			/* ������ ������� ��� �������� */
			apdu = ReadBinary;
			apdu.setCommandField(APDU::APDU::Fields::P1, (fileOff >> 8) & 0xFF);
			apdu.setCommandField(APDU::APDU::Fields::P2, fileOff & 0xFF);
			apdu.setLe(min(0x0F, fileLen - fileOff));

			/* ��������� ���������� ������� � ���������� � */
			secAPDU = APDU::SecureAPDU(apdu);
			tempVector1 = reader.sendCommand(hCard, secAPDU.generateRawSecureAPDU(kEnc, kMac, SSC));

			/* �������� ����� � ���������� ��� �������� */
			secRAPDU = APDU::SecureRAPDU(tempVector1, kEnc, kMac, SSC);
			if (!secRAPDU.isResponceOK()) {
				throw std::exception(("������: �� ������� ������� ����: " + secRAPDU.report()).c_str());
			}

			/* ���������� �������������� ����� � ���� */
			tempVector1 = secRAPDU.getDecodedResponceData();
			dgFile[file].write(tempVector1.data(), tempVector1.size());
			dgFile[file].flush();

			/* ����������� �������� ����� */
			fileOff += tempVector1.size();

			if (progressBarHwnd) {
				SendMessage(progressBarHwnd, PBM_STEPIT, 0, 0);
			}
		}
	}

	ByteVec getPassportRawImage() {
		if (!dgFile[DGFILES::DG2].is_open()) {
			throw std::exception("������: ���� DG2 �� ������!");
		}

		BerTLV::BerTLVDecoder decoder;
		UINT16 rootIndex = decoder.decode(dgFile[DGFILES::DG2]);
		
		INT16 tag0x75 = decoder.getChildByTag(rootIndex, 0x75);
		if (tag0x75 == -1) {
			throw std::exception("������: �� ������� �������� ��� 0x75");
		}

		INT16 tag0x7F61 = decoder.getChildByTag(tag0x75, 0x7F61);
		if (tag0x7F61 == -1) {
			throw std::exception("������: �� ������� �������� ��� 0x7F61");
		}

		INT16 tag0x7F60 = decoder.getChildByTag(tag0x7F61, 0x7F60);
		if (tag0x7F60 == -1) {
			throw std::exception("������: �� ������� �������� ��� 0x7F60");
		}

		INT16 tagImage = decoder.getChildByTag(tag0x7F60, 0x7F2E);
		if (tagImage == -1) {
			tagImage = decoder.getChildByTag(tag0x7F60, 0x5F2E);
			if (tagImage == -1) {
				throw std::exception("������: �� ������� �������� ����������� �� ����� 0x7F2E � 0x5F2E");
			}
		}

		return decoder.getTokenData(dgFile[DGFILES::DG2], tagImage);
	}

	ByteVec getPassportMRZ() {
		if (!dgFile[DGFILES::DG1].is_open()) {
			throw std::exception("������: ���� DG1 �� ������!");
		}

		BerTLV::BerTLVDecoder decoder;
		UINT16 rootIndex = decoder.decode(dgFile[DGFILES::DG1]);

		INT16 tag0x61 = decoder.getChildByTag(rootIndex, 0x61);
		if (tag0x61 == -1) {
			throw std::exception("������: �� ������� ���������� ��� 0x61");
		}

		INT16 tag0x5F1F = decoder.getChildByTag(tag0x61, 0x5F1F);
		if (tag0x5F1F == -1) {
			throw std::exception("������: �� ������� ���������� ��� 0x5F1F");
		}

		return decoder.getTokenData(dgFile[DGFILES::DG1], tag0x5F1F);
	}
};