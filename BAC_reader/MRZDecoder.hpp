#pragma once

#include <sstream>
#include <vector>
using namespace std;

/* Простенький класс для выделения нужных полей из второй строки MRZ кода */
class DecoderMRZLine2 {
private:
	/* Везде нужна контрольная цифра */
	char passNumRaw[10];
	char birthDate[7];
	char expireDate[7];
public:
	DecoderMRZLine2(string mrzLine) {
		char buffer[40] = { 0 };

		/* Выполняем процесс считывания */
		istringstream ss(mrzLine);
		ss.read(passNumRaw, sizeof(passNumRaw));
		ss.read(buffer, 3); /* Пропуск гражданства */
		ss.read(birthDate, sizeof(birthDate));
		ss.read(buffer, 1); /* Пропуск пола */
		ss.read(expireDate, sizeof(expireDate));
	}

	vector<unsigned char> ComposeData() {
		unsigned char dataStr[24] = { 0 };
		memcpy(dataStr, passNumRaw, 10);
		memcpy(dataStr + 10, birthDate, 7);
		memcpy(dataStr + 17, expireDate, 7);

		vector<unsigned char> data(24, 0);
		memcpy(data.data(), dataStr, 24);
		return data;
	}
};