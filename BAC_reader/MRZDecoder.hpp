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

	bool isAllNumbers(char* str, int len) {
		for (int i = 0; i < len; i += 1) {
			if (!isdigit(str[i])) { return false; }
		}
		return true;
	}
	bool isThreeCapsSymb(char* str) {
		/* Проверяем, что все три символа это знаки алфавита (латинского) и при этом они записаны в верхнем регистре */
		return (isalpha(str[0]) && isupper(str[0])) && (isalpha(str[1]) && isupper(str[1])) && (isalpha(str[2]) && isupper(str[2]));
	}
	bool isSingle(char symb) {
		return (symb == 'F' || symb == 'M');
	}
public:
	Line2MRZDecoder(string mrzLine) {
		if (mrzLine.size() < 44) {
			throw std::exception("Неверная длина MRZ кода: проверьте ввод");
		}

		string errorMsg = "";
		char buffer[40] = { 0 };

		/* Выполняем процесс считывания */
		istringstream ss(mrzLine);

		/* Считываем номер паспорта и контрольную цифру */
		ss.read(passNumRaw, sizeof(passNumRaw));
		
		/* Считываем и проверяем гражданство */
		ss.read(buffer, 3);
		if (!isThreeCapsSymb(buffer)) {
			errorMsg += "Неверно указано гражданство\n";
		}
		
		/* Проврека на правильность ввода даты рождения и контрольной цифры */
		ss.read(birthDate, sizeof(birthDate));
		if (!isAllNumbers(birthDate, sizeof(birthDate))) {
			errorMsg += "Неверно указана дата рождения\n";
		}

		/* Проверка пола */
		if (!isSingle(ss.get())) {
			errorMsg += "Неверно указан пол\n";
		}

		/* Считываем дату окончания действия и контрольную цифру */
		ss.read(expireDate, sizeof(expireDate));
		if (!isAllNumbers(expireDate, sizeof(expireDate))) {
			errorMsg += "Неверно указана дата истечения\n";
		}

		/* Пропуск факультатитвной информации */
		ss.read(buffer, 14);

		/* Если последние два символа не цифры - тоже ошибка */
		if (!isalnum(ss.get()) || !isalnum(ss.get())) {
			errorMsg += "Неверно указаны последние контрольные цифры\n";
		}

		if (errorMsg.size() != 0) {
			throw std::exception(errorMsg.c_str());
		}
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

		/* Обработаем даты */
		birthStr = { birthStr[4], birthStr[5], '.', birthStr[2], birthStr[3], '.', birthStr[0], birthStr[1]};
		expireStr = { expireStr[4], expireStr[5], '.', expireStr[2], expireStr[3], '.', expireStr[0], expireStr[1]};

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