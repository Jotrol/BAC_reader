#pragma once

#include <winscard.h>
#include <vector>
#include <sstream>
#include <exception>
#include <string>
#include <iostream>
using namespace std;

#pragma comment(lib, "winscard.lib")

class CardReader {
private:
    SCARDCONTEXT hScardContext;

    static string castErrorCodeToHexString(ULONG ulError) {
        stringstream ss;
        ss << "Код ошибки WinsCard: 0x" << hex << ulError;
        return ss.str();
    }

public:
    CardReader() : hScardContext() {
        /* Начальная инициализация контекста для работы со смарт-картами */
        LONG lResult = SCardEstablishContext(SCARD_SCOPE_USER, nullptr, nullptr, &hScardContext);
        if (lResult != SCARD_S_SUCCESS) {
            throw std::exception("Невозможно инициализировать контекст SCARDCONTEXT");
        }
    }

    /* Получение списка считывателей в системе */
    vector<wstring> getReadersList() const {
        /* Получаем имена считывателей в системе */
        /* Результат выполнения - кусок памяти, строки в котором записаны подряд, последняя строка - с двумя нулями */
        LPWSTR pmszReaders = nullptr;
        DWORD cch = SCARD_AUTOALLOCATE;
        LONG lResult = SCardListReaders(hScardContext, SCARD_ALL_READERS, (LPWSTR)&pmszReaders, &cch);
        if (lResult != SCARD_S_SUCCESS) {
            throw std::exception("Не удалось считать доступные ридеры в системе");
        }

        /* Записываем все считанные ридеры в вектор имён */
        vector<wstring> readerNames = {};
        LPWSTR pReader = pmszReaders;
        if (!pReader) {
            throw std::exception("Список доступных ридеров пуст");
        }

        while (*pReader != '\0') {
            size_t readerNameLen = wcslen(pReader);

            /* Производим копирование названия во временную строку */
            wstring tempString = L"";
            tempString.append(pReader, readerNameLen);

            /* Которую затем добавляем в массив имён */
            readerNames.push_back(tempString);

            /* Смещаем считыватель, так как эти строки записаны подряд, разделитель - нулевой символ */
            pReader = pReader + readerNameLen + 1;
        }

        /* Очищаем память из-под имён ридеров */
        SCardFreeMemory(hScardContext, pmszReaders);

        /* Возвращаем вектор имён ридеров */
        return readerNames;
    }

    /* Функция выполнения соединения с картой */
    SCARDHANDLE cardConnect(const wstring& readerName) const {
        SCARDHANDLE hCard = 0;
        DWORD dwAp = 0;

        /* Непосредственно устанавливаем соединение по имени кардридера по протоколу T1 (бесконтактный) */
        LONG lResult = SCardConnect(hScardContext, readerName.data(), SCARD_SHARE_EXCLUSIVE, SCARD_PROTOCOL_Tx, &hCard, &dwAp);

        /* Если произошла ошибка или (вдруг) не тот протокол соединения */
        /* То это выход */
        if (lResult != SCARD_S_SUCCESS) {
            throw std::exception("Не получилось выполнить соединение с картой!");
        }
        if (dwAp != SCARD_PROTOCOL_T1) {
            throw std::exception("Неверный протокол соединения");
        }
        return hCard;
    }

    /* Функция отключения карты */
    void cardDisconnect(SCARDHANDLE hCard) const {
        SCardDisconnect(hCard, SCARD_LEAVE_CARD);
    }

	/* Функция отправки команды */
	vector<BYTE> sendCommand(SCARDHANDLE hCard, vector<BYTE> commandAPDU) const {
		/* Выделение буфера на 256 значений  */
		DWORD bufferSize = 256;
        DWORD apduSize = (DWORD)commandAPDU.size();
        vector<BYTE> buffer(bufferSize, 0);
	    
		LONG lReturn = SCardTransmit(hCard, SCARD_PCI_T1, commandAPDU.data(), apduSize, nullptr, buffer.data(), &bufferSize);
		if (lReturn != SCARD_S_SUCCESS) {
            string errorMsg = CardReader::castErrorCodeToHexString(lReturn);
			throw std::exception(errorMsg.c_str());
		}

		buffer.resize(bufferSize);
		return buffer;
	}

	~CardReader() {
		SCardReleaseContext(hScardContext);
	}
};