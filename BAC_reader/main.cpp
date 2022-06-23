#include <Windows.h>

#include "Crypto.hpp"
#include "CardReader.hpp"
#include "MRZDecoder.hpp"
#include "BerTLV.hpp"

#include <fstream>
#include <sstream>
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

//INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, INT nCmdShow) {
int main() {
	srand(time(NULL));
	setlocale(LC_ALL, "rus");

	//"L898902C<3UTO6908061F9406236ZE184226B<<<<<14"
	auto tuple = getKeysToEncAndMac("5200000001RUS8008046F0902274<<<<<<<<<<<<<<<06");
	auto& kEnc = std::get<0>(tuple);
	auto& kMac = std::get<1>(tuple);
	auto& kSeed = std::get<2>(tuple);

	//vector<BYTE> rndIFD(8, 0);
	//vector<BYTE> kIFD(16, 0);
	//for (int i = 0; i < 8; i += 1) { rndIFD[i] = rand() & 0xFF; }
	//for (int i = 0; i < 16; i += 1) { kIFD[i] = rand() & 0xFF; }

	vector<BYTE> rndIFD = { 0x78, 0x17, 0x23, 0x86, 0x0C, 0x06, 0xC2, 0x26 };
	vector<BYTE> kIFD = { 0x0B, 0x79, 0x52, 0x40, 0xCB, 0x70, 0x49, 0xB0, 0x1C, 0x19, 0xB3, 0x3E, 0x32, 0x80, 0x4F, 0x0B };

	CardReader reader;
	auto readersList = reader.GetReadersList();
	reader.CardConnect(readersList[0]);

	/* Отправим команду выбора приложения */
	Responce responce = reader.SendCommand(SelectApp);
	responce = reader.SendCommand(GetICC);
	//Responce responce = { {0x46, 0x08, 0xF9, 0x19, 0x88, 0x70, 0x22, 0x12}, 0x90, 0x00 };

	vector<BYTE> S(rndIFD);
	vector<BYTE> rICC = responce.responceData;
	S.insert(S.end(), responce.responceData.begin(), responce.responceData.end());
	S.insert(S.end(), kIFD.begin(), kIFD.end());

	Crypto::Des3 des3;
	Crypto::RetailMAC mac;
	auto E_IFD = des3.encrypt(S, kEnc);
	E_IFD.resize(32);

	auto M_IFD = mac.getMAC(E_IFD, kMac);

	vector<BYTE> cmdData;
	cmdData.insert(cmdData.end(), E_IFD.begin(), E_IFD.end());
	cmdData.insert(cmdData.end(), M_IFD.begin(), M_IFD.end());
	//print_vector_hex(cmdData, "cmd_data");

	vector<BYTE> mutualAuthCommand(MUTUAL_AUTH);
	mutualAuthCommand.insert(mutualAuthCommand.end(), cmdData.begin(), cmdData.end());
	mutualAuthCommand.push_back(0x28);

	responce = reader.SendCommand(mutualAuthCommand);
	//cout << hex << (int)responce.SW1 << endl;

	//responce = { {0x46, 0xB9, 0x34, 0x2A, 0x41, 0x39, 0x6C, 0xD7, 0x38, 0x6B, 0xF5, 0x80, 0x31, 0x04, 0xD7, 0xCE, 0xDC, 0x12, 0x2B, 0x91, 0x32, 0x13, 0x9B, 0xAF, 0x2E, 0xED, 0xC9, 0x4E, 0xE1, 0x78, 0x53, 0x4F, 0x2F, 0x2D, 0x23, 0x5D, 0x07, 0x4D, 0x74, 0x49}, 0x90, 0x00 };
	vector<BYTE> resp_data(responce.responceData);
	vector<BYTE> E_ICC_RAW; E_ICC_RAW.insert(E_ICC_RAW.begin(), resp_data.begin(), resp_data.begin() + 32);
	vector<BYTE> M_ICC_RAW; M_ICC_RAW.insert(M_ICC_RAW.begin(), resp_data.begin() + 32, resp_data.end());

	auto M_ICC_CALC = mac.getMAC(E_ICC_RAW, kMac);
	if (M_ICC_CALC != M_ICC_RAW) {
		throw exception("Ошибка: MAC не совпал с ответом");
	}

	auto E_ICC = des3.decrypt(E_ICC_RAW, kEnc);
	vector<BYTE> RND_IFD; RND_IFD.insert(RND_IFD.begin(), E_ICC.begin() + 8, E_ICC.begin() + 16);
	if (RND_IFD != rndIFD) {
		throw exception("Ошибка: RND.IFD не совпал");
	}

	vector<BYTE> kICC; kICC.insert(kICC.begin(), E_ICC.begin() + 16, E_ICC.end());
	//print_vector_hex(kICC, "kICC");
	for (int i = 0; i < kICC.size(); i += 1) {
		kICC[i] = kICC[i] ^ kIFD[i];
	}
	//print_vector_hex(kICC, "kICC xor kIFD");

	auto keys = generateKeyPair(kICC);
	auto& ksEnc = keys.first;
	auto& ksMac = keys.second;

	//print_vector_hex(ksMac, "ksMAC");

	size_t SSC = ((size_t)rICC[4] << 56) | ((size_t)rICC[5] << 48) | ((size_t)rICC[6] << 40) | ((size_t)rICC[7] << 32) | ((size_t)rndIFD[4] << 24) | ((size_t)rndIFD[5] << 16) | ((size_t)rndIFD[6] << 8) | rndIFD[7];
	//cout << hex << SSC << dec << endl;

	vector<BYTE> M = Crypto::fillISO9797({ 0x0C, 0xA4, 0x02, 0x0C }, 8);
	vector<BYTE> data = Crypto::fillISO9797({ 0x01, 0x1E }, 8);
	vector<BYTE> encData = des3.encrypt(data, ksEnc);

	vector<BYTE> DO87 = { 0x87, 0x09, 0x01 };
	DO87.insert(DO87.end(), encData.begin(), encData.end());
	M.insert(M.end(), DO87.begin(), DO87.end());
	print_vector_hex(M, "M");

	//size_t SSC = 0x887022120C06C227ULL;

	/* Удачно!! */
	SSC += 1;
	vector<BYTE> N = castToVector(SSC);
	N.insert(N.end(), M.begin(), M.end());
	print_vector_hex(N, "N");

	vector<BYTE> CC = mac.getMAC(N, ksMac);
	print_vector_hex(CC, "CC");

	vector<BYTE> DO8E = { 0x8E, 0x08 };
	DO8E.insert(DO8E.end(), CC.begin(), CC.end());

	vector<BYTE> APDU = { 0x0C, 0xA4, 0x02, 0x0C, 0x15 };
	APDU.insert(APDU.end(), DO87.begin(), DO87.end());
	APDU.insert(APDU.end(), DO8E.begin(), DO8E.end());
	APDU.push_back(0x00);

	//print_vector_hex(APDU, "Completed APDU: ");
	responce = reader.SendCommand(APDU);
	//responce = { {0x99, 0x02, 0x90, 0x00, 0x8E, 0x08, 0xFA, 0x85, 0x5A, 0x5D, 0x4C, 0x50, 0xA8, 0xED, 0x90, 0x00 }, 0x90, 0x00 };
	print_vector_hex(responce.responceData, "APDU responce");
	cout << "APDU result: " << hex << (int)responce.SW1 << (int)responce.SW2 << dec << endl;
	/* Удачно !! */

	/* Декодируем RAPDU */
	BerTLV::BerTLVDecoder decoder;
	FILE* file = tmpfile();
	fstream responceFile(file);
	responceFile.write((char*)responce.responceData.data(), responce.responceData.size());
	responceFile.seekg(0);

	UINT16 rootIndex = decoder.decode(responceFile);
	BerTLV::BerTLVDecoderToken* rootToken = decoder.getTokenPtr(rootIndex);
	if (rootToken->getTag() != 0x99) { cerr << "Что-то не так в ответе" << endl; }

	auto tag0x99Raw = decoder.getTokenRaw(responceFile, rootIndex);
	auto CC_resp = decoder.getTokenPtr(decoder.getChildByTag(0, 0x8E))->getData(responceFile);
	print_vector_hex(CC_resp, "CC_resp");

	SSC += 1;
	auto K = castToVector(SSC);
	K.insert(K.end(), tag0x99Raw.begin(), tag0x99Raw.end());
	print_vector_hex(K, "k_not_filled");

	CC = mac.getMAC(K, ksMac);
	print_vector_hex(CC, "CC");

	cout << (CC_resp == CC) << endl;

	return 0;
}