#pragma once
#include "BerTLV.hpp"
#include "Crypto.hpp"
#include "Util.hpp"

class APDU {
protected:
	union {
		UINT8 commandRaw[4];
		UINT32 commandTag;
	} commandHeader;
	vector<UINT8> commandData;
	UINT16 Le;
public:
	enum Fields { CLA, INS, P1, P2 };

	APDU(UINT8 CLA = 0, UINT8 INS = 0, UINT8 P1 = 0, UINT8 P2 = 0, const vector<UINT8>& data = {}, UINT16 le = 0) {
		commandHeader.commandRaw[APDU::Fields::CLA] = CLA;
		commandHeader.commandRaw[APDU::Fields::INS] = INS;
		commandHeader.commandRaw[APDU::Fields::P1] = P1;
		commandHeader.commandRaw[APDU::Fields::P2] = P2;
		commandData = data;
		Le = le;
	}
	void setCommandField(APDU::Fields field, UINT8 val) {
		commandHeader.commandRaw[field] = val;
	}
	void setCommandData(const vector<UINT8>& newData) {
		commandData = newData;
	}
	void appendCommandData(const vector<UINT8>& addData) {
		commandData.insert(commandData.end(), addData.begin(), addData.end());
	}
};
class SecureAPDU : public APDU {
	vector<UINT8> commandMac;

	BerTLV::BerTLVCoderToken generateDO87(const vector<UINT8>& ksEnc) {
		/* ������� �������� ���������� */
		Crypto::Des3& des3 = Crypto::getDes3Alg();

		/* �������� ��� ��� DO87 */
		BerTLV::BerTLVCoderToken tokenDO87(0x87);

		/* ���� ����� ������ ����� ����, �� DO87 �� ������������ */
		if (this->commandData.size() != 0) {
			/* ������� ��������� ������ */
			vector<UINT8> dataFilled = Crypto::fillISO9797(this->commandData, des3.getBlockLen());
			vector<UINT8> encrypted = des3.encrypt(dataFilled, ksEnc);

			/* ���������� 0x01 � ������ ������������� ������; ��� � ��������� ������� */
			encrypted.insert(encrypted.begin(), 0x01);

			/* ������� ������������� ������ � ��� 0x87 */
			tokenDO87.addData(encrypted);
		}

		return tokenDO87;
	}

	BerTLV::BerTLVCoderToken generateDO97(UINT16 Le) {
		/* ������ ��� 0x97, � ������� ����� ������ ����� */
		BerTLV::BerTLVCoderToken tokenDO97(0x97);

		/* ���� ����� ��������� ������ �� ���� */
		if (Le != 0) {
			/* �������� ����������� ����� � ������ */
			UINT8 NeRaw[sizeof(Le)] = { 0 };
			memcpy(NeRaw, &Le, sizeof(Le));

			/* ���� ������ ��������� ���� � ���� ����� */
			UINT8 nonZeroIndex = 0;
			for (; nonZeroIndex < sizeof(Le) && !NeRaw[nonZeroIndex]; nonZeroIndex += 1);

			/* �������� ����� ����� � ������ */
			UINT32 neRawLen = sizeof(Le) - nonZeroIndex;

			/* ��������� � ����� ����� ����� */
			tokenDO97.addData(NeRaw + nonZeroIndex, neRawLen);
		}

		return tokenDO97;
	}

	BerTLV::BerTLVCoderToken generateDO8E(const vector<UINT8>& ksMac, UINT64& SSC, const BerTLV::BerTLVCoderToken& DO87, const BerTLV::BerTLVCoderToken& DO97) {
		Crypto::RetailMAC& mac = Crypto::getMacAlg();

		/* ����������� SSC �� 1 */
		SSC += 1;

		/* �������� ��� ����� � ���������� ������������ SSC, ����������� �������, DO87 � DO97 */
		vector<UINT8> concat = Util::castToVector(SSC);

		/* ��������� ����� ������� ��� ������������� ���������� */
		for (INT i = 0; i < sizeof(this->commandHeader.commandRaw); i += 1) {
			concat.push_back(this->commandHeader.commandRaw[i]);
		}

		/* ��������� ���������� */
		concat = Crypto::fillISO9797(concat, mac.getBlockLen());

		/* ����� ���������� ��������� DO87 � DO97 (�������, ���� ��� �� �������) */
		if (DO87.getDataLen() != 0) {
			vector<UINT8> encDO87 = DO87.encode();
			concat.insert(concat.end(), encDO87.begin(), encDO87.end());
		}
		if (DO97.getDataLen() != 0) {
			vector<UINT8> encDO97 = DO97.encode();
			concat.insert(concat.end(), encDO97.begin(), encDO97.end());
		}

		/* ��� ����������, ������ ��� �������� getMAC ��� ��� ����� ������ */
		commandMac = mac.getMAC(concat, ksMac);

		/* ��������� BER-TLV ��� � ������� 0x8E */
		BerTLV::BerTLVCoderToken tokenDO8E(0x8E);
		tokenDO8E.addData(commandMac);
		return tokenDO8E;
	}
public:
	/* ����������� ����������� ������ �������� APDU ������� */
	SecureAPDU(const APDU& apduCommand) : APDU(apduCommand) {
		/* ��������� ���� ������, �� ��������� */
		this->setCommandField(APDU::Fields::CLA, 0x0C);
	}

	vector<UINT8> generateRawSecureAPDU(const vector<UINT8>& ksEnc, const vector<UINT8>& ksMac, UINT64& SSC) {
		/* ���������� ����� APDU ������ � ���� ������� BER-TLV */
		BerTLV::BerTLVCoderToken tokenDO87 = this->generateDO87(ksEnc);
		BerTLV::BerTLVCoderToken tokenDO97 = this->generateDO97(this->Le);
		BerTLV::BerTLVCoderToken tokenDO8E = this->generateDO8E(ksMac, SSC, tokenDO87, tokenDO97);

		/* ��������� �������������� ������ ������� ������ � ������ APDU ���� */
		/* � ������� ������: ��� ��������� ����� � Big Endian ���� */
		/* ��� ����, changeEndiannes ����������� ���, ��� �� BE ������� �� ���������� ������ ������� */
		/* �� �����, ����� ������������� �������������, ���� �������� ��� ������� ���� */
		/* ��� ����� ���������� commandRaw � ������������ APDU, ��� ������ �� ����� ������ BE */
		/* � �� ���� ������� � LE ������� �������������� LE �����. �� BE �������������� BE */
		/* �� ������� changeEndiannes �� BE ������� ������ �� ������ */
		BerTLV::BerTLVCoderToken apduToken(Util::changeEndiannes(this->commandHeader.commandTag));

		/* ���� ������ �� ������� - �� �������� */
		if (tokenDO87.getDataLen() != 0) {
			vector<UINT8> encTokenDO87 = tokenDO87.encode();
			apduToken.addData(encTokenDO87);
		}
		if (tokenDO97.getDataLen() != 0) {
			vector<UINT8> encTokenDO97 = tokenDO97.encode();
			apduToken.addData(encTokenDO97);
		}
		if (tokenDO8E.getDataLen() != 0) {
			vector<UINT8> encTokenDO8E = tokenDO8E.encode();
			apduToken.addData(encTokenDO8E);
		}

		/* ��������� ������� APDU */
		vector<UINT8> apduCommand = apduToken.encode();

		/* �������� ����� ��������� ����� ��������� � ��������� �� � ����� ��������������� APDU ��������� */
		vector<UINT8> leUINT8s = Util::castToVector(this->Le);
		apduCommand.insert(apduCommand.end(), leUINT8s.begin(), leUINT8s.end());
		return apduCommand;
	}

	vector<UINT8> getMessageMac() { return commandMac; }
};

class RAPDU {
protected:
	UINT8 SW1;
	UINT8 SW2;
	vector<UINT8> responceData;

public:
	RAPDU(const vector<UINT8>& responce) {
		UINT64 responceSize = responce.size();
		SW1 = responce[responceSize - 2];
		SW2 = responce[responceSize - 1];
		
		/* �������� ������ ������ */
		copy(responce.begin(), responce.begin() + responceSize - 2, responceData.begin());
	}

	UINT64 getResponceDataSize() const {
		return responceData.size();
	}
	vector<UINT8> getResponceData() const {
		return responceData;
	}

	bool isResponceOK() const {
		return (SW1 == 0x90) && (SW2 == 0x00);
	}
};
class SecureRAPDU : private RAPDU {
	bool isValid;

	vector<UINT8> responceDO87Data;
	vector<UINT8> responceDO99Data;
public:
	SecureRAPDU(const vector<UINT8>& responce, UINT64& SSC, const vector<BYTE>& ksMAC) : RAPDU(responce) {
		/* ������� ����� � ���� ����������� RAPDU, ��� ����� �������� �� ��������� ���� */
		BerTLV::BerStream berStream(tmpfile());

		/* ���������� ���������� ������ � ����, � ����� ���������� ������� ��� ���������� � ������ ������ */
		berStream.write(this->responceData.data(), this->responceData.size());
		berStream.seekg(0);
		
		/* ��������� ������������� BER */
		BerTLV::BerTLVDecoder bertlvDecoder;
		UINT16 rootIndex = bertlvDecoder.decode(berStream);

		/* ������� ����� ���� 0x87, 0x97, 0x99 */
		UINT16 do87Index = bertlvDecoder.getChildByTag(rootIndex, 0x87);
		UINT16 do8EIndex = bertlvDecoder.getChildByTag(rootIndex, 0x8E);
		UINT16 do99Index = bertlvDecoder.getChildByTag(rootIndex, 0x99);

		/* ��� 0x8E ������ ������ ���� */
		if (do87Index == 0) {
			throw std::exception("������: SRAPDU ������� ��� 0x8E");
		}

		/* ���������� ������������ ������ �� ������ ����, ����� ���� ������� */
		vector<UINT8> concat = Util::castToVector(SSC);
		
		if (do87Index) {
			vector<UINT8> rawDO87 = bertlvDecoder.getTokenRaw(berStream, do87Index);
			concat.insert(concat.end(), rawDO87.begin(), rawDO87.end());
		}
		if (do99Index) {
			vector<UINT8> rawDO99 = bertlvDecoder.getTokenRaw(berStream, do99Index);
			concat.insert(concat.end(), rawDO99.begin(), rawDO99.end());
		}

		/* ������� MAC ���������, ������� ��� �������� */
		vector<UINT8> responceMAC = bertlvDecoder.getTokenData(berStream, do8EIndex);

		/* ��������� MAC ��������� ���� */
		Crypto::RetailMAC& mac = Crypto::getMacAlg();
		vector<UINT8> computedMAC = mac.getMAC(concat, ksMAC);

		isValid = (computedMAC == responceMAC);
	}
	
	bool isResponceOK() const {
		return (SW1 == 0x90) && (SW2 == 0x00) && isValid;
	}
};