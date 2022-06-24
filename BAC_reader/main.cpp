#include <Windows.h>

#include "Crypto.hpp"
#include "CardReader.hpp"
#include "MRZDecoder.hpp"
#include "BerTLV.hpp"
#include "APDU.hpp"

#include <fstream>
#include <sstream>
#include <cassert>
using namespace std;

#define REAL_TEST

pair< vector<UINT8>, vector<UINT8>> generateKeyPair(const vector<UINT8>& kSeed) {
	Crypto::Sha1& sha1 = Crypto::getSha1Alg();

	vector<UINT8> encAppend = { 0x00, 0x00, 0x00, 0x01 };
	vector<UINT8> macAppend = { 0x00, 0x00, 0x00, 0x02 };
	vector<UINT8> dEnc(kSeed.begin(), kSeed.end()); dEnc.insert(dEnc.end(), encAppend.begin(), encAppend.end());
	vector<UINT8> dMac(kSeed.begin(), kSeed.end()); dMac.insert(dMac.end(), macAppend.begin(), macAppend.end());

	auto kEnc = sha1.getHash(dEnc);
	kEnc.resize(16);
	kEnc = Crypto::correctParityBits(kEnc);

	auto kMac = sha1.getHash(dMac);
	kMac.resize(16);
	kMac = Crypto::correctParityBits(kMac);

	return { kEnc, kMac };
}
tuple<vector<UINT8>, vector<UINT8>, vector<UINT8>> getKeysToEncAndMac(string mrzLine2) {
	Crypto::RetailMAC& mac = Crypto::getMacAlg();
	Crypto::Sha1& sha1 = Crypto::getSha1Alg();

	DecoderMRZLine2 mrzDecoder(mrzLine2);
	auto mrzData = mrzDecoder.ComposeData();
	auto kSeed = sha1.getHash(mrzData);
	kSeed.resize(16);

	auto keys = generateKeyPair(kSeed);
	auto& kEnc = keys.first;
	auto& kMac = keys.second;

	return make_tuple(kEnc, kMac, kSeed);
}

vector<UINT8> SelectApp = { 0x00, 0xA4, 0x04, 0x0C, 0x07, 0xA0, 0x00, 0x00, 0x02, 0x47, 0x10, 0x01 };
vector<UINT8> GetICC = { 0x00, 0x84, 0x00, 0x00, 0x08 };
vector<UINT8> MUTUAL_AUTH = { 0x00, 0x82, 0x00, 0x00, 0x28 };

#include <random>
#include <ctime>

void print_vector_hex(const vector<UINT8>& data, string msg) {
	const char hexVals[17] = "0123456789ABCDEF";

	cout << msg << ": ";
	for (int i = 0; i < data.size(); i += 1) {
		cout << hexVals[(data[i] >> 4) & 0x0F];
		cout << hexVals[data[i] & 0x0F];
		//cout << " ";
	}
	cout << endl;
}

#ifdef REAL_TEST
	string getMRZCode() {
		return "5200000001RUS8008046F0902274<<<<<<<<<<<<<<<06";
	}
	vector<UINT8> generateRndIFD() {
		vector<UINT8> rndIFD(8, 0);
		for (int i = 0; i < 8; i += 1) {
			rndIFD[i] = rand() & 0xFF;
		}
		return rndIFD;
	}
	vector<UINT8> generateKIFD() {
		vector<UINT8> kIFD(16, 0);
		for (int i = 0; i < 16; i += 1) {
			kIFD[i] = rand() & 0xFF;
		}
		return kIFD;
	}
	vector<UINT8> getICC(const CardReader& reader) {
		/* Выбираем приложение карты */
		APDU::RAPDU responce(reader.SendCommand(SelectApp));
		if (!responce.isResponceOK()) {
			throw std::exception("Ошибка: не удалось отправить SelectApp");
		}

		/* Отправляем команду для начала обмена ключами */
		responce = APDU::RAPDU(reader.SendCommand(GetICC));
		if (!responce.isResponceOK()) {
			throw std::exception("Ошибка: не удалось отправить GetICC");
		}
		return responce.getResponceData();
	}
	vector<UINT8> getMutualAuth(const CardReader& reader, const vector<UINT8>& cmdData) {
		vector<UINT8> mutualAuthCommand(MUTUAL_AUTH);
		mutualAuthCommand.insert(mutualAuthCommand.end(), cmdData.begin(), cmdData.end());
		mutualAuthCommand.push_back(0x28);

		APDU::RAPDU responce = reader.SendCommand(mutualAuthCommand);
		if (!responce.isResponceOK()) {
			throw std::exception("Ошибка: не удалось отправить MutualAuth");
		}

		return responce.getResponceData();
	}
	UINT64 getSSC(const vector<UINT8>& rICC, const vector<UINT8>& rndIFD) {
		return ((UINT64)rICC[4] << 56) | ((UINT64)rICC[5] << 48) | ((UINT64)rICC[6] << 40) | ((UINT64)rICC[7] << 32) | ((UINT64)rndIFD[4] << 24) | ((UINT64)rndIFD[5] << 16) | ((UINT64)rndIFD[6] << 8) | rndIFD[7];
	}
	vector<UINT8> sendAPDUEFCOM(const CardReader& reader, const vector<UINT8>& apdu) {
		return reader.SendCommand(apdu);
	}
	vector<UINT8> sendReadEFCOM(const CardReader& reader, const vector<UINT8>& apdu) {
		return reader.SendCommand(apdu);
	}
#else
	vector<UINT8> expectedCmdData = { 0x72, 0xC2, 0x9C, 0x23, 0x71, 0xCC, 0x9B, 0xDB, 0x65, 0xB7, 0x79, 0xB8, 0xE8, 0xD3, 0x7B, 0x29, 0xEC, 0xC1, 0x54, 0xAA, 0x56, 0xA8, 0x79, 0x9F, 0xAE, 0x2F, 0x49, 0x8F, 0x76, 0xED, 0x92, 0xF2, 0x5F, 0x14, 0x48, 0xEE, 0xA8, 0xAD, 0x90, 0xA7 };

	/* Если верить стандарту (а надо), то в конце может быть записано двухбайтное Le, либо 0x00 (если Le = 0) или 0x00 0x00 (потому что два байта). Два нулевых байта можно */
	vector<UINT8> expectedAPDUEFCOM = { 0x0C, 0xA4, 0x02, 0x0C, 0x15, 0x87, 0x09, 0x01, 0x63, 0x75, 0x43, 0x29, 0x08, 0xC0, 0x44, 0xF6, 0x8E, 0x08, 0xBF, 0x8B, 0x92, 0xD6, 0x35, 0xFF, 0x24, 0xF8, 0x00 };

	vector<UINT8> expectedReadEFCOM = { 0x0C, 0xB0, 0x00, 0x00, 0x0D, 0x97, 0x01, 0x04, 0x8E, 0x08, 0xED, 0x67, 0x05, 0x41, 0x7E, 0x96, 0xBA, 0x55, 0x00 };

	string getMRZCode() { return "L898902C<3UTO6908061F9406236ZE184226B<<<<<14"; }
	vector<UINT8> generateRndIFD() { return { 0x78, 0x17, 0x23, 0x86, 0x0C, 0x06, 0xC2, 0x26 }; }
	vector<UINT8> generateKIFD() { return { 0x0B, 0x79, 0x52, 0x40, 0xCB, 0x70, 0x49, 0xB0, 0x1C, 0x19, 0xB3, 0x3E, 0x32, 0x80, 0x4F, 0x0B }; }
	vector<UINT8> getICC(const CardReader& reader) { return { 0x46, 0x08, 0xF9, 0x19, 0x88, 0x70, 0x22, 0x12 }; }
	vector<UINT8> getMutualAuth(const CardReader& reader, const vector<UINT8>& cmdData) {
		assert(cmdData == expectedCmdData);
		return { 0x46, 0xB9, 0x34, 0x2A, 0x41, 0x39, 0x6C, 0xD7, 0x38, 0x6B, 0xF5, 0x80, 0x31, 0x04, 0xD7, 0xCE, 0xDC, 0x12, 0x2B, 0x91, 0x32, 0x13, 0x9B, 0xAF, 0x2E, 0xED, 0xC9, 0x4E, 0xE1, 0x78, 0x53, 0x4F, 0x2F, 0x2D, 0x23, 0x5D, 0x07, 0x4D, 0x74, 0x49 };
	}
	UINT64 getSSC(const vector<UINT8>& rICC, const vector<UINT8>& rndIFD) { 
		UINT64 SSC = ((UINT64)rICC[4] << 56) | ((UINT64)rICC[5] << 48) | ((UINT64)rICC[6] << 40) | ((UINT64)rICC[7] << 32) | ((UINT64)rndIFD[4] << 24) | ((UINT64)rndIFD[5] << 16) | ((UINT64)rndIFD[6] << 8) | rndIFD[7];
		assert(SSC == 0x887022120C06C226ULL);
		return SSC;
	}
	vector<UINT8> sendAPDUEFCOM(const CardReader& reader, const vector<UINT8>& apdu) {
		assert(apdu == expectedAPDUEFCOM);
		return { 0x99, 0x02, 0x90, 0x00, 0x8E, 0x08, 0xFA, 0x85, 0x5A, 0x5D, 0x4C, 0x50, 0xA8, 0xED, 0x90, 0x00 };
	}
	vector<UINT8> sendReadEFCOM(const CardReader& reader, const vector<UINT8>& apdu) {
		print_vector_hex(apdu, "APDU        ");
		print_vector_hex(expectedReadEFCOM, "expectedAPDU");
		assert(apdu == expectedReadEFCOM);
		return { 0x87, 0x09, 0x01, 0x9F, 0xF0, 0xEC, 0x34, 0xF9, 0x92, 0x26, 0x51, 0x99, 0x02, 0x90, 0x00, 0x8E, 0x08, 0xAD, 0x55, 0xCC, 0x17, 0x14, 0x0B, 0x2D, 0xED, 0x90, 0x00 };
	}
#endif



int main() {
	srand((UINT)time(NULL));
	setlocale(LC_ALL, "rus");

	/* Инициализация шифрования и кпритографической подписи, кардридера */
	Crypto::Des3& des3 = Crypto::getDes3Alg();
	Crypto::RetailMAC& mac = Crypto::getMacAlg();

	CardReader reader;
	auto readersList = reader.GetReadersList();

	cin.get();
	reader.CardConnect(readersList[0]);

	/* D1, D2. Получение ключей для шифрования, MAC и генерации ключевых пар */
	/* Из MRZ кода второй строки */
	auto tuple = getKeysToEncAndMac(getMRZCode());
	auto& kEnc = std::get<0>(tuple);
	auto& kMac = std::get<1>(tuple);
	auto& kSeed = std::get<2>(tuple);

	/* D3.1. Получение случайного числа от карты */
	vector<UINT8> rICC = getICC(reader);
	if (rICC.size() == 0) { return 1; }

	/* D3.2. Генерация случайных чисел для обмена ключами */
	vector<UINT8> rndIFD = generateRndIFD();
	vector<UINT8> kIFD = generateKIFD();

	/* D3.3. Конкатенация rndIFD, rICC и kIFD */
	vector<UINT8> S(rndIFD);
	S.insert(S.end(), rICC.begin(), rICC.end());
	S.insert(S.end(), kIFD.begin(), kIFD.end());

	/* D3.4. Шифрование S ключом kEnc; отсекаются лишние байты, так как нужны только 32 байта */
	auto E_IFD = des3.encrypt(S, kEnc);
	E_IFD.resize(32);

	/* D3.5. Вычисление MAC по E_IFD ключом 3DES kMac */
	auto M_IFD = mac.getMAC(E_IFD, kMac);

	/* D3.6. Формирование APDU команды EXTERNAL AUTHENTICATE и посылка команды МСПД */
	vector<UINT8> cmdData;
	cmdData.insert(cmdData.end(), E_IFD.begin(), E_IFD.end());
	cmdData.insert(cmdData.end(), M_IFD.begin(), M_IFD.end());

	vector<UINT8> resp_data = getMutualAuth(reader, cmdData);
	if (resp_data.size() == 0) { return 1; }

	/* D3.6.Система проверки */
	vector<UINT8> E_ICC_RAW;
	E_ICC_RAW.insert(E_ICC_RAW.begin(), resp_data.begin(), resp_data.begin() + 32);
	vector<UINT8> M_ICC_RAW;
	M_ICC_RAW.insert(M_ICC_RAW.begin(), resp_data.begin() + 32, resp_data.end());

	auto M_ICC_CALC = mac.getMAC(E_ICC_RAW, kMac);
	if (M_ICC_CALC != M_ICC_RAW) {
		throw std::exception("Ошибка: MAC не совпал с ответом");
	}

	auto E_ICC = des3.decrypt(E_ICC_RAW, kEnc);
	vector<UINT8> RND_IFD;
	RND_IFD.insert(RND_IFD.begin(), E_ICC.begin() + 8, E_ICC.begin() + 16);
	if (RND_IFD != rndIFD) {
		throw std::exception("Ошибка: RND.IFD не совпал");
	}

	vector<UINT8> kICC;
	kICC.insert(kICC.begin(), E_ICC.begin() + 16, E_ICC.end());
	for (int i = 0; i < kICC.size(); i += 1) {
		kICC[i] = kICC[i] ^ kIFD[i];
	}

	auto keys = generateKeyPair(kICC);
	auto& ksEnc = keys.first;
	auto& ksMac = keys.second;

	/* Получение счётчика посылаемых блоков */
	size_t SSC = getSSC(rICC, rndIFD);

	APDU::APDU selectEFCOM(0x00, 0xA4, 0x02, 0xC, { 0x01, 0x1E });
	APDU::SecureAPDU secureApdu(selectEFCOM);

	vector<UINT8> responceRaw = sendAPDUEFCOM(reader, secureApdu.generateRawSecureAPDU(ksEnc, ksMac, SSC));
	APDU::SecureRAPDU secureResponce(responceRaw, SSC, ksMac, ksEnc);
	if (secureResponce.isResponceOK()) {
		cout << "Ура, всё ОК!" << endl;
	}

	APDU::APDU readAPDU(0x00, 0xB0, 0x00, 0x00, {}, 0x0A);
	APDU::SecureAPDU secureReadAPDU(readAPDU);
	responceRaw = sendReadEFCOM(reader, secureReadAPDU.generateRawSecureAPDU(ksEnc, ksMac, SSC));
	APDU::SecureRAPDU readResponce(responceRaw, SSC, ksMac, ksEnc);
	if (readResponce.isResponceOK()) {
		print_vector_hex(readResponce.getResponceData(), "Responce");
	}

	return 0;
}