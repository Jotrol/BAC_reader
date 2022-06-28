#pragma once
#include <string>
#include <sstream>
#include <vector>
using namespace std;

/* Простенький класс для выделения нужных полей из второй строки MRZ кода */
class Line2MRZDecoder {
private:
	/* Везде нужна контрольная цифра */
	char passNumRaw[10];
	char birthDate[7];
	char expireDate[7];
public:
	Line2MRZDecoder(string mrzLine) {
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
		char dataStr[24] = { 0 };
		memcpy(dataStr, passNumRaw, 10);
		memcpy(dataStr + 10, birthDate, 7);
		memcpy(dataStr + 17, expireDate, 7);

		vector<unsigned char> data(24, 0);
		memcpy(data.data(), dataStr, 24);
		return data;
	}
};

class FullMRZDecoder {
public:
	vector<string> infoVec;
	
	FullMRZDecoder(string mrzRawLine) {
		/* Инициализация буферов для всех данных */
		char documentCode[3] = {0};
		char govCode[4] = {0};
		char name[40] = {0};
		char docNum[10] = {0};
		char nationality[4] = {0};
		char birthDate[7] = {0};
		char sex = 0;
		char expirationDate[7] = {0};

		/* Считывание данных */
		istringstream ss(mrzRawLine);
		ss.read(documentCode, 1);
		ss.seekg(1, ios::cur);

		ss.read(govCode, 3);
		ss.read(name, 39);
		ss.read(docNum, 9);
		ss.seekg(1, ios::cur);

		ss.read(nationality, 3);
		ss.read(birthDate, 6);
		ss.seekg(1, ios::cur);

		ss.read((char*)&sex, 1);
		ss.read(expirationDate, 6);

		/* Переводим всё в строки */
		string docCodeStr = documentCode;
		string govCodeStr = govCode;
		string nameStr = name;
		string docNumStr = docNum;
		string natStr = nationality;
		string birthStr = birthDate;
		string sexStr = { sex };
		string expireStr = expirationDate;

		/* Сначала разделим имя и фамилию */
		size_t nameStart = 0;
		size_t nameEnd = nameStr.find_first_of('<', nameStart);

		size_t famStart = nameStr.find_first_not_of('<', nameEnd);
		size_t famEnd = nameStr.find_first_of('<', famStart);
		
		string nameOnly = nameStr.substr(nameStart, nameEnd - nameStart);
		string famOnly = nameStr.substr(famStart, famEnd - famStart);

		/* Для удобства заполним так */
		infoVec.push_back(docCodeStr);
		infoVec.push_back(govCodeStr);
		infoVec.push_back(nameOnly);
		infoVec.push_back(famOnly);
		infoVec.push_back(docNumStr);
		infoVec.push_back(natStr);
		infoVec.push_back(birthStr);
		infoVec.push_back(sexStr);
		infoVec.push_back(expireStr);
	}
};