#pragma once
#include <winscard.h>
#include <vector>
#include <exception>
#include <string>
#include <iostream>
using namespace std;

#pragma comment(lib, "winscard.lib")

CHAR* SCardGetErrorString(LONG lRetValue) {
    switch (lRetValue) {
    case 0x0l:
        return "SCard OK";
    case 0x80100001:
        return "SCard internal error";
    case 0x80100002:
        return "SCard cancelled";
    case 0x80100003:
        return "SCard invalid handle";
    case 0x80100004:
        return "SCard invalid parameter";
    case 0x80100005:
        return "SCard invalid target";
    case 0x80100006:
        return "SCard no memory";
    case 0x80100007:
        return "SCard waited too long";
    case 0x80100008:
        return "SCard insufficient buffer";
    case 0x80100009:
        return "SCard unknown reader";
    case 0x8010000a:
        return "SCard timeout";
    case 0x8010000b:
        return "SCard sharing violation";
    case 0x8010000c:
        return "SCard no smartcard";
    case 0x8010000d:
        return "SCard unknown card";
    case 0x8010000e:
        return "SCard cant dispose";
    case 0x8010000f:
        return "SCard proto mismatch";
    case 0x80100010:
        return "SCard not ready";
    case 0x80100011:
        return "SCard invalid value";
    case 0x80100012:
        return "SCard system cancelled";
    case 0x80100013:
        return "SCard communications error";
    case 0x80100014:
        return "SCard unknown error";
    case 0x80100015:
        return "SCard invalid atr";
    case 0x80100016:
        return "SCard not transacted";
    case 0x80100017:
        return "SCard reader unavailable";
    case 0x80100018:
        return "SCard p shutdown";
    case 0x80100019:
        return "SCard pci too small";
    case 0x8010001a:
        return "SCard reader unsupported";
    case 0x8010001b:
        return "SCard duplicate reader";
    case 0x8010001c:
        return "SCard card unsupported";
    case 0x8010001d:
        return "SCard no service";
    case 0x8010001e:
        return "SCard service stopped";
    case 0x8010001f:
        return "SCard unexpected";
    case 0x80100020:
        return "SCard icc installation";
    case 0x80100021:
        return "SCard icc createorder";
    case 0x80100022:
        return "SCard unsupported feature";
    case 0x80100023:
        return "SCard dir not found";
    case 0x80100024:
        return "SCard file not  ound";
    case 0x80100025:
        return "SCard no dir";
    case 0x80100026:
        return "SCard no file";
    case 0x80100027:
        return "SCard no access";
    case 0x80100028:
        return "SCard write too many";
    case 0x80100029:
        return "SCard bad seek";
    case 0x8010002a:
        return "SCard invalid chv";
    case 0x8010002b:
        return "SCard unknown res mng";
    case 0x8010002c:
        return "SCard no such certificate";
    case 0x8010002d:
        return "SCard certificate unavailable";
    case 0x8010002e:
return "SCard no readers available";
    case 0x80100065:
        return "SCard warning unsupported card";
    case 0x80100066:
        return "SCard warning unresponsive card";
    case 0x80100067:
        return "SCard warning unpowered card";
    case 0x80100068:
        return "SCard warning reset card";
    case 0x80100069:
        return "SCard warning removed card";
    case 0x8010006a:
        return "SCard warning security violation";
    case 0x8010006b:
        return "SCard warning wrong chv";
    case 0x8010006c:
        return "SCard warning chv blocked";
    case 0x8010006d:
        return "SCard warning eof";
    case 0x8010006e:
        return "SCard warning cancelled by user";
    case 0x0000007b:
        return "SCard inaccessible boot device";
    }
    return "invalid error code";
}

class CardReader {
private:
    SCARDCONTEXT hScardContext;

public:
    CardReader() : hScardContext() {
        /* Начальная инициализация контекста для работы со смарт-картами */
        LONG lResult = SCardEstablishContext(SCARD_SCOPE_USER, nullptr, nullptr, &hScardContext);
        if (lResult != SCARD_S_SUCCESS) {
            throw exception("Невозможно инициализировать контекст SCARDCONTEXT");
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
			throw std::exception(SCardGetErrorString(lReturn));
		}

		buffer.resize(bufferSize);
		return buffer;
	}

	~CardReader() {
		SCardReleaseContext(hScardContext);
	}
};