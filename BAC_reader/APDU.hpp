#pragma once
#include "BerTLV.hpp"
#include "Crypto.hpp"
#include "Util.hpp"

#include <sstream>
#include <string>

namespace APDU {
	class APDU {
	protected:
		UINT8 commandRaw[4];
		vector<UINT8> commandData;
		UINT16 Le;

		vector<UINT8> getLeBytes(UINT16 Le) const {
			UINT8 byte1 = (Le >> 8) & 0xFF;
			UINT8 byte2 = Le & 0xFF;

			vector<UINT8> leBytes = {};
			if (byte1) { leBytes.push_back(byte1); }
			leBytes.push_back(byte2);
			return leBytes;
		}
	public:
		enum Fields { CLA, INS, P1, P2 };

		APDU(UINT8 CLA = 0, UINT8 INS = 0, UINT8 P1 = 0, UINT8 P2 = 0, const vector<UINT8>& data = {}, UINT16 le = 0) {
			commandRaw[APDU::Fields::CLA] = CLA;
			commandRaw[APDU::Fields::INS] = INS;
			commandRaw[APDU::Fields::P1] = P1;
			commandRaw[APDU::Fields::P2] = P2;
			commandData = data;
			Le = le;
		}
		void setCommandField(APDU::Fields field, UINT8 val) {
			commandRaw[field] = val;
		}
		void setLe(UINT16 le) {
			Le = le;
		}
		void setCommandData(const vector<UINT8>& newData) {
			commandData = newData;
		}
		void appendCommandData(const vector<UINT8>& addData) {
			commandData.insert(commandData.end(), addData.begin(), addData.end());
		}
		vector<UINT8> getCommand() const {
			vector<UINT8> leBytes = getLeBytes(Le);
			vector<UINT8> command = { commandRaw[0], commandRaw[1], commandRaw[2], commandRaw[3] };
			command.insert(command.end(), commandData.begin(), commandData.end());
			command.insert(command.end(), leBytes.begin(), leBytes.end());
			return command;
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
				/* ��������� � ����� ����� ����� */
				tokenDO97.addData(getLeBytes(Le));
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
			for (INT i = 0; i < sizeof(this->commandRaw); i += 1) {
				concat.push_back(this->commandRaw[i]);
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
		SecureAPDU() = default;

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
			UINT32 commandTag = (commandRaw[0] << 24) | (commandRaw[1] << 16) | (commandRaw[2] << 8) | commandRaw[3];
			BerTLV::BerTLVCoderToken apduToken(commandTag);

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
			vector<UINT8> leUINT8s = getLeBytes(this->Le);
			apduCommand.insert(apduCommand.end(), leUINT8s.begin(), leUINT8s.end());
			return apduCommand;
		}

		vector<UINT8> getMessageMac() const { return commandMac; }
	};

	class RAPDU {
	protected:
		UINT8 SW1;
		UINT8 SW2;
		vector<UINT8> responceData;

	public:
		RAPDU() = default;
		RAPDU(const vector<UINT8>& responce) {
			UINT64 responceSize = responce.size();
			SW1 = responce[responceSize - 2];
			SW2 = responce[responceSize - 1];

			responceData = {};

			/* ���� ���� ������ - �.� ����� ������ ������ 2 */
			if (responce.size() > 2) {
				responceData.insert(responceData.begin(), responce.begin(), responce.begin() + responceSize - 2);
			}
		}
		
		UINT64 getResponceDataSize() const {
			return responceData.size();
		}

		std::string report() {
			std::stringstream ss;
			ss << "SW1=" << hex << (int)SW1 << ",SW2=" << hex << (int)SW2;
			return ss.str();
		}

		virtual vector<UINT8> getResponceData() const {
			return responceData;
		}

		virtual bool isResponceOK() const {
			return (SW1 == 0x90) && (SW2 == 0x00);
		}
	};
	class SecureRAPDU : public RAPDU {
		bool isValid;
		vector<UINT8> responceDO87DecodedData;
	public:
		SecureRAPDU() = default;
		SecureRAPDU(const vector<UINT8>& responce, const vector<BYTE>& ksEnc, const vector<BYTE>& ksMAC, UINT64& SSC) : RAPDU(responce) {
			/* ������� ����� � ���� ����������� RAPDU, ��� ����� �������� �� ��������� ���� */
			//BerTLV::BerStream berStream("rawAPDU.bin", ios::in | ios::out | ios::binary | ios::trunc);
			BerTLV::BerStream berStream("apduRaw.bin", ios::in | ios::out | ios::binary | ios::trunc);

			/* ���������� ���������� ������ � ����, � ����� ���������� ������� ��� ���������� � ������ ������ */
			berStream.write(this->responceData.data(), this->responceData.size());
			berStream.seekg(0);

			/* ��������� ������������� BER */
			BerTLV::BerTLVDecoder bertlvDecoder;
			UINT16 rootIndex = bertlvDecoder.decode(berStream);

			/* ������� ����� ���� 0x87, 0x97, 0x99 */
			INT16 do87Index = bertlvDecoder.getChildByTag(rootIndex, 0x87);
			INT16 do8EIndex = bertlvDecoder.getChildByTag(rootIndex, 0x8E);
			INT16 do99Index = bertlvDecoder.getChildByTag(rootIndex, 0x99);

			/* ��� 0x8E ������ ������ ���� */
			if (do8EIndex == -1) {
				throw std::exception("������: SRAPDU ������� ��� 0x8E");
			}

			/* ���������� ������������ ������ �� ������ ����, ����� ���� ������� */
			SSC += 1;
			vector<UINT8> concat = Util::castToVector(SSC);

			if (do87Index != -1) {
				vector<UINT8> rawDO87 = bertlvDecoder.getTokenRaw(berStream, do87Index);
				concat.insert(concat.end(), rawDO87.begin(), rawDO87.end());
			}
			if (do99Index != -1) {
				vector<UINT8> rawDO99 = bertlvDecoder.getTokenRaw(berStream, do99Index);
				concat.insert(concat.end(), rawDO99.begin(), rawDO99.end());
			}

			/* ������� MAC ���������, ������� ��� �������� */
			vector<UINT8> responceMAC = bertlvDecoder.getTokenData(berStream, do8EIndex);

			/* ��������� MAC ��������� ���� */
			Crypto::RetailMAC& mac = Crypto::getMacAlg();
			vector<UINT8> computedMAC = mac.getMAC(concat, ksMAC);

			isValid = (computedMAC == responceMAC);
			if (!isValid) {
				return;
			}

			if (do87Index != -1) {
				responceData = bertlvDecoder.getTokenData(berStream, do87Index);

				/* �������� �������� ���������� */
				Crypto::Des3& des3 = Crypto::getDes3Alg();

				/* ����������� �� 0x01 � DO'87 */
				responceDO87DecodedData.insert(responceDO87DecodedData.end(), responceData.begin() + 1, responceData.end());

				responceDO87DecodedData = des3.decrypt(responceDO87DecodedData, ksEnc);

				/* ������� ���������� */
				/* TODO: ����������� ������� ������ ����� 0x80, �� �� ����� �������� �� */
				INT find0x80 = -1;
				for (INT i = responceDO87DecodedData.size() - 1; i > -1; i -= 1) {
					if (responceDO87DecodedData[i] == 0x80) {
						find0x80 = i;
						break;
					}
				}

				/* ���� �� ���������� ����� 0x80 - �� ���������� �� ���� � ��� ���������� ������ */
				if (find0x80 == -1) {
					responceDO87DecodedData = {};
				}
				else {
					responceDO87DecodedData.resize(find0x80);
				}
			}

			berStream.close();
		}

		virtual bool isResponceOK() const {
			return (SW1 == 0x90) && (SW2 == 0x00) && isValid;
		}

		vector<UINT8> getDecodedResponceData() const {
			return responceDO87DecodedData;
		}
	};
}