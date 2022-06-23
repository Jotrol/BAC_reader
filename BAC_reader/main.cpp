#include <Windows.h>

#include "Crypto.hpp"
#include "CardReader.hpp"
#include "MRZDecoder.hpp"
#include "BerTLV.hpp"

#include <fstream>
#include <sstream>
#include <cassert>
using namespace std;

pair< vector<BYTE>, vector<BYTE>> generateKeyPair(const vector<BYTE>& kSeed) {
	Crypto::Sha1 sha1;

	vector<BYTE> encAppend = { 0x00, 0x00, 0x00, 0x01 };
	vector<BYTE> macAppend = { 0x00, 0x00, 0x00, 0x02 };
	vector<BYTE> dEnc(kSeed.begin(), kSeed.end()); dEnc.insert(dEnc.end(), encAppend.begin(), encAppend.end());
	vector<BYTE> dMac(kSeed.begin(), kSeed.end()); dMac.insert(dMac.end(), macAppend.begin(), macAppend.end());

	auto kEnc = sha1.getHash(dEnc);
	kEnc.resize(16);
	kEnc = Crypto::correctParityBits(kEnc);

	auto kMac = sha1.getHash(dMac);
	kMac.resize(16);
	kMac = Crypto::correctParityBits(kMac);

	return { kEnc, kMac };
}
tuple<vector<BYTE>, vector<BYTE>, vector<BYTE>> getKeysToEncAndMac(string mrzLine2) {
	Crypto::RetailMAC mac;
	Crypto::Sha1 sha1;

	DecoderMRZLine2 mrzDecoder(mrzLine2);
	auto mrzData = mrzDecoder.ComposeData();
	auto kSeed = sha1.getHash(mrzData);
	kSeed.resize(16);

	auto keys = generateKeyPair(kSeed);
	auto& kEnc = keys.first;
	auto& kMac = keys.second;

	return make_tuple(kEnc, kMac, kSeed);
}

template<typename T>
vector<BYTE> castToVector(T val) {
	BYTE valRaw[sizeof(T)] = { 0 };
	memcpy(valRaw, &val, sizeof(T));

	vector<BYTE> out;
	for (int i = sizeof(T) - 1; i > -1; i -= 1) {
		out.push_back(valRaw[i]);
	}
	return out;
}

vector<BYTE> SelectApp = { 0x00, 0xA4, 0x04, 0x0C, 0x07, 0xA0, 0x00, 0x00, 0x02, 0x47, 0x10, 0x01 };
vector<BYTE> GetICC = { 0x00, 0x84, 0x00, 0x00, 0x08 };
vector<BYTE> MUTUAL_AUTH = { 0x00, 0x82, 0x00, 0x00, 0x28 };
vector<BYTE> SelectCOM = { 0x00, 0xA4, 0x02, 0x0C, 0x02, 0x01, 0x0E };

#include <random>
#include <ctime>

void print_vector_hex(const vector<BYTE>& data, string msg) {
	const char hexVals[17] = "0123456789ABCDEF";

	cout << msg << ": ";
	for (int i = 0; i < data.size(); i += 1) {
		cout << hexVals[(data[i] >> 4) & 0x0F];
		cout << hexVals[data[i] & 0x0F];
		cout << " ";
	}
	cout << endl;
}
inline void ERROR_LOG(string msg, const Responce& responce) {
	cerr << msg << hex << responce.SW1 << responce.SW2 << dec << endl;
}
inline bool RESPONCE_OK(const Responce& responce) {
	return (responce.SW1 == 0x90 && responce.SW2 == 0x00);
}

#define REAL_TEST

#ifdef REAL_TEST
	string getMRZCode() {
		return "5200000001RUS8008046F0902274<<<<<<<<<<<<<<<06";
	}
	vector<BYTE> generateRndIFD() {
		vector<BYTE> rndIFD(8, 0);
		for (int i = 0; i < 8; i += 1) {
			rndIFD[i] = rand() & 0xFF;
		}
		return rndIFD;
	}
	vector<BYTE> generateKIFD() {
		vector<BYTE> kIFD(16, 0);
		for (int i = 0; i < 16; i += 1) {
			kIFD[i] = rand() & 0xFF;
		}
		return kIFD;
	}
	vector<BYTE> getICC(const CardReader& reader) {
		/* Выбираем приложение карты */
		Responce responce = reader.SendCommand(SelectApp);
		if (!RESPONCE_OK(responce)) {
			ERROR_LOG("Ошибка: не удалось отправить команду SelectApp: ", responce);
			return {};
		}

		/* Отправляем команду для начала обмена ключами */
		responce = reader.SendCommand(GetICC);
		if (!RESPONCE_OK(responce)) {
			ERROR_LOG("Ошибка: не удалось отправить команду GetICC: ", responce);
			return {};
		}

		return responce.responceData;
	}
	vector<BYTE> getMutualAuth(const CardReader& reader, const vector<BYTE>& cmdData) {
		vector<BYTE> mutualAuthCommand(MUTUAL_AUTH);
		mutualAuthCommand.insert(mutualAuthCommand.end(), cmdData.begin(), cmdData.end());
		mutualAuthCommand.push_back(0x28);

		Responce responce = reader.SendCommand(mutualAuthCommand);
		if (!RESPONCE_OK(responce)) {
			ERROR_LOG("Ошибка: не удалось отправить команду MutualAuth: ", responce);
			return {};
		}

		return responce.responceData;
	}
	UINT64 getSSC(const vector<BYTE>& rICC, const vector<BYTE>& rndIFD) {
		return ((UINT64)rICC[4] << 56) | ((UINT64)rICC[5] << 48) | ((UINT64)rICC[6] << 40) | ((UINT64)rICC[7] << 32) | ((UINT64)rndIFD[4] << 24) | ((UINT64)rndIFD[5] << 16) | ((UINT64)rndIFD[6] << 8) | rndIFD[7];
	}
	vector<BYTE> sendAPDUEFCOM(const CardReader& reader, const vector<BYTE>& apdu) {
		Responce responce = reader.SendCommand(apdu);
		if (!RESPONCE_OK(responce)) {
			ERROR_LOG("Ошибка: не удалось отправить команду APDU: ", responce);
			return {};
		}

		return responce.responceData;
	}
#else
vector<BYTE> expectedCmdData = { 0x72, 0xC2, 0x9C, 0x23, 0x71, 0xCC, 0x9B, 0xDB, 0x65, 0xB7, 0x79, 0xB8, 0xE8, 0xD3, 0x7B, 0x29, 0xEC, 0xC1, 0x54, 0xAA, 0x56, 0xA8, 0x79, 0x9F, 0xAE, 0x2F, 0x49, 0x8F, 0x76, 0xED, 0x92, 0xF2, 0x5F, 0x14, 0x48, 0xEE, 0xA8, 0xAD, 0x90, 0xA7 };
vector<BYTE> expectedAPDUEFCOM = { 0x0C, 0xA4, 0x02, 0x0C, 0x15, 0x87, 0x09, 0x01, 0x63, 0x75, 0x43, 0x29, 0x08, 0xC0, 0x44, 0xF6, 0x8E, 0x08, 0xBF, 0x8B, 0x92, 0xD6, 0x35, 0xFF, 0x24, 0xF8, 0x00 };

	string getMRZCode() { return "L898902C<3UTO6908061F9406236ZE184226B<<<<<14"; }
	vector<BYTE> generateRndIFD() { return { 0x78, 0x17, 0x23, 0x86, 0x0C, 0x06, 0xC2, 0x26 }; }
	vector<BYTE> generateKIFD() { return { 0x0B, 0x79, 0x52, 0x40, 0xCB, 0x70, 0x49, 0xB0, 0x1C, 0x19, 0xB3, 0x3E, 0x32, 0x80, 0x4F, 0x0B }; }
	vector<BYTE> getICC(const CardReader& reader) { return { 0x46, 0x08, 0xF9, 0x19, 0x88, 0x70, 0x22, 0x12 }; }
	vector<BYTE> getMutualAuth(const CardReader& reader, const vector<BYTE>& cmdData) {
		assert(cmdData == expectedCmdData);
		return { 0x46, 0xB9, 0x34, 0x2A, 0x41, 0x39, 0x6C, 0xD7, 0x38, 0x6B, 0xF5, 0x80, 0x31, 0x04, 0xD7, 0xCE, 0xDC, 0x12, 0x2B, 0x91, 0x32, 0x13, 0x9B, 0xAF, 0x2E, 0xED, 0xC9, 0x4E, 0xE1, 0x78, 0x53, 0x4F, 0x2F, 0x2D, 0x23, 0x5D, 0x07, 0x4D, 0x74, 0x49 };
	}
	UINT64 getSSC(const vector<BYTE>& rICC, const vector<BYTE>& rndIFD) { 
		UINT64 SSC = ((UINT64)rICC[4] << 56) | ((UINT64)rICC[5] << 48) | ((UINT64)rICC[6] << 40) | ((UINT64)rICC[7] << 32) | ((UINT64)rndIFD[4] << 24) | ((UINT64)rndIFD[5] << 16) | ((UINT64)rndIFD[6] << 8) | rndIFD[7];
		assert(SSC == 0x887022120C06C226ULL);
		return SSC;
	}
	vector<BYTE> sendAPDUEFCOM(const CardReader& reader, const vector<BYTE>& apdu) {
		assert(apdu == expectedAPDUEFCOM);
		return { 0x99, 0x02, 0x90, 0x00, 0x8E, 0x08, 0xFA, 0x85, 0x5A, 0x5D, 0x4C, 0x50, 0xA8, 0xED, 0x90, 0x00 };
	}
#endif

//INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, INT nCmdShow) {
int main() {
	srand(time(NULL));
	setlocale(LC_ALL, "rus");

	/* Инициализация шифрования и кпритографической подписи, кардридера */
	Crypto::Des3 des3;
	Crypto::RetailMAC mac;

	CardReader reader;
	auto readersList = reader.GetReadersList();
	reader.CardConnect(readersList[0]);

	/* D1, D2. Получение ключей для шифрования, MAC и генерации ключевых пар */
	/* Из MRZ кода второй строки */
	auto tuple = getKeysToEncAndMac(getMRZCode());
	auto& kEnc = std::get<0>(tuple);
	auto& kMac = std::get<1>(tuple);
	auto& kSeed = std::get<2>(tuple);

	/* D3.1. Получение случайного числа от карты */
	vector<BYTE> rICC = getICC(reader);
	if (rICC.size() == 0) { return 1; }

	/* D3.2. Генерация случайных чисел для обмена ключами */
	vector<BYTE> rndIFD = generateRndIFD();
	vector<BYTE> kIFD = generateKIFD();

	/* D3.3. Конкатенация rndIFD, rICC и kIFD */
	vector<BYTE> S(rndIFD);
	S.insert(S.end(), rICC.begin(), rICC.end());
	S.insert(S.end(), kIFD.begin(), kIFD.end());

	/* D3.4. Шифрование S ключом kEnc; отсекаются лишние байты, так как нужны только 32 байта */
	auto E_IFD = des3.encrypt(S, kEnc);
	E_IFD.resize(32);

	/* D3.5. Вычисление MAC по E_IFD ключом 3DES kMac */
	auto M_IFD = mac.getMAC(E_IFD, kMac);

	/* D3.6. Формирование APDU команды EXTERNAL AUTHENTICATE и посылка команды МСПД */
	vector<BYTE> cmdData;
	cmdData.insert(cmdData.end(), E_IFD.begin(), E_IFD.end());
	cmdData.insert(cmdData.end(), M_IFD.begin(), M_IFD.end());

	vector<BYTE> resp_data = getMutualAuth(reader, cmdData);
	if (resp_data.size() == 0) { return 1; }

	/* D3.6.Система проверки */
	vector<BYTE> E_ICC_RAW;
	E_ICC_RAW.insert(E_ICC_RAW.begin(), resp_data.begin(), resp_data.begin() + 32);
	vector<BYTE> M_ICC_RAW;
	M_ICC_RAW.insert(M_ICC_RAW.begin(), resp_data.begin() + 32, resp_data.end());

	auto M_ICC_CALC = mac.getMAC(E_ICC_RAW, kMac);
	if (M_ICC_CALC != M_ICC_RAW) {
		throw exception("Ошибка: MAC не совпал с ответом");
	}

	auto E_ICC = des3.decrypt(E_ICC_RAW, kEnc);
	vector<BYTE> RND_IFD;
	RND_IFD.insert(RND_IFD.begin(), E_ICC.begin() + 8, E_ICC.begin() + 16);
	if (RND_IFD != rndIFD) {
		throw exception("Ошибка: RND.IFD не совпал");
	}

	vector<BYTE> kICC;
	kICC.insert(kICC.begin(), E_ICC.begin() + 16, E_ICC.end());
	for (int i = 0; i < kICC.size(); i += 1) {
		kICC[i] = kICC[i] ^ kIFD[i];
	}

	auto keys = generateKeyPair(kICC);
	auto& ksEnc = keys.first;
	auto& ksMac = keys.second;

	/* Получение счётчика посылаемых блоков */
	size_t SSC = getSSC(rICC, rndIFD);

	/* D4. Безопасный обмен сообщениями */
	/* D4.1.a. Маскирование и заполнение байта класса */
	vector<BYTE> M = Crypto::fillISO9797({ 0x0C, 0xA4, 0x02, 0x0C }, 8);

	/* D4.1.b. Заполнение данных */
	vector<BYTE> data = Crypto::fillISO9797({ 0x01, 0x1E }, 8);

	/* D4.1.c. Шифрование данных с ksEnc */
	vector<BYTE> encData = des3.encrypt(data, ksEnc);

	/* D4.1.d. Построение DO'87 */
	vector<BYTE> DO87 = { 0x87, 0x09, 0x01 };
	DO87.insert(DO87.end(), encData.begin(), encData.end());

	/* D4.1.e. Конкатенация заголвока команды и DO'87 */
	M.insert(M.end(), DO87.begin(), DO87.end());

	/* D4.1.f. Вычисление MAC от M */
	/* D4.1.f.i. Приращение SSC на 1 и перевод его в байты в BigEndian */
	SSC += 1;
	vector<BYTE> N = castToVector(SSC);
	
	/* D4.1.f.ii. Конкатенация SSC и M */
	N.insert(N.end(), M.begin(), M.end());

	/* D4.1.f.iii. Добавление заполнения (getMAC это делает сам) и вычисление MAC по N с ksMac */
	vector<BYTE> CC = mac.getMAC(N, ksMac);

	/* D4.1.g. Построение DO'8E */
	vector<BYTE> DO8E = { 0x8E, 0x08 };
	DO8E.insert(DO8E.end(), CC.begin(), CC.end());

	/* D4.1.h. Построение защищённой команды APDU */
	vector<BYTE> APDU = { 0x0C, 0xA4, 0x02, 0x0C, 0x15 };
	APDU.insert(APDU.end(), DO87.begin(), DO87.end());
	APDU.insert(APDU.end(), DO8E.begin(), DO8E.end());
	APDU.push_back(0x00);

	/* D4.1.i. Отправка команды и получение ответа - RAPDU */
	vector<BYTE> apduSendResponce = sendAPDUEFCOM(reader, APDU);
	if (apduSendResponce.size() == 0) { return 1; }

	/* D4.1.j. Верификация RAPDU CC путем вычисления MAC DO'99 */
	/* Инициализируем декодер для BER-TLV данных */
	BerTLV::BerTLVDecoder decoder;

	/* Создаём временный файл, куда запишем полученные данные */
	FILE* file = tmpfile();
	fstream responceFile(file);
	responceFile.write((char*)apduSendResponce.data(), apduSendResponce.size());
	responceFile.seekg(0);

	/* Декодируем данные */
	UINT16 rootIndex = decoder.decode(responceFile);
	BerTLV::BerTLVDecoderToken* rootToken = decoder.getTokenPtr(rootIndex);
	if (rootToken->getTag() != 0x99) { cerr << "Что-то не так в ответе" << endl; return 1; }

	/* Получаем токен с тегом 0x99 как он выглядит без декодирования */
	vector<BYTE> tag0x99Raw = decoder.getTokenRaw(responceFile, rootIndex);

	/* Ищем тег с CC сообщения */
	UINT16 tag0x8E = decoder.getChildByTag(0, 0x8E);
	if (tag0x8E == 0) { cerr << "Нет такого тега!" << endl; return 1; }

	/* Получаем токен */
	BerTLV::BerTLVDecoderToken* CCtoken = decoder.getTokenPtr(tag0x8E);

	/* Получаем MAC сообщения из токена */
	vector<BYTE> CC_resp = CCtoken->getData(responceFile);
	print_vector_hex(CC_resp, "CC_resp");

	/* D4.1.j.i. Приращение SSC на 1 */
	SSC += 1;

	/* D4.1.j.ii. Конкатенация SSC и DO'99 */
	auto K = castToVector(SSC);
	K.insert(K.end(), tag0x99Raw.begin(), tag0x99Raw.end());
	print_vector_hex(K, "k_not_filled");

	/* D4.1.j.iii. Вычисления MAC с ksMac (заполнение не далется, ибо getMAC сам заполняет ввод) */
	CC = mac.getMAC(K, ksMac);
	print_vector_hex(CC, "CC");

	/* D4.1.j.iv. Сравнение с данными DO'8E RAPDU */
	cout << (CC_resp == CC) << endl;

	return 0;
}