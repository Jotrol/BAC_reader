#pragma once
#include <winscard.h>
#include <vector>
#include <exception>
#include <string>
#include <iostream>
using namespace std;

#pragma comment(lib, "winscard.lib")

struct Responce {
	vector<BYTE> responceData;

	unsigned char SW1;
	unsigned char SW2;
};

class CardReader {
private:
	SCARDCONTEXT hScardContext;
	SCARDHANDLE hCardHandle;

public:
	CardReader() : hScardContext(), hCardHandle() {
		/* Начальная инициализация контекста для работы со смарт-картами */
		LONG lResult = SCardEstablishContext(SCARD_SCOPE_USER, nullptr, nullptr, &hScardContext);
		if (lResult != SCARD_S_SUCCESS) {
			throw exception("Невозможно инициализировать контекст SCARDCONTEXT");
		}
	}

	/* Получение списка считывателей в системе */
	vector<wstring> GetReadersList() {
		/* Получаем имена считывателей в системе */
		/* Результат выполнения - кусок памяти, строки в котором записаны подряд, последняя строка - с двумя нулями */
		LPWSTR pmszReaders = nullptr;
		DWORD cch = SCARD_AUTOALLOCATE;
		LONG lResult = SCardListReaders(hScardContext, SCARD_ALL_READERS, (LPWSTR)&pmszReaders, &cch);
		if (lResult != SCARD_S_SUCCESS) {
			cerr << "Не удалось считать доступные ридеры в системе" << endl;
			SCardFreeMemory(hScardContext, pmszReaders);
			return {};
		}

		/* Записываем все считанные ридеры в вектор имён */
		vector<wstring> readerNames = {};
		LPWSTR pReader = pmszReaders;
		if (!pReader) {
			cerr << "Список доступных ридеров пуст" << endl;
			return {};
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
	bool CardConnect(const wstring& readerName) {
		/* Используемый протокол */
		DWORD dwAP = 0;

		/* Непосредственно устанавливаем соединение по имени кардридера по протоколу T1 (бесконтактный) */
		LONG lResult = SCardConnect(hScardContext, readerName.data(), SCARD_SHARE_EXCLUSIVE, SCARD_PROTOCOL_Tx, &hCardHandle, &dwAP);

		/* Если произошла ошибка или (вдруг) не тот протокол соединения */
		/* То это выход */
		if (lResult != SCARD_S_SUCCESS) {
			cerr << "Не получилось выполнить соединение с картой!" << endl;
			return false;
		}
		if (dwAP != SCARD_PROTOCOL_T1) {
			cerr << "Неверный протокол соединения" << endl;
			return false;
		}
		return true;
	}

	/* Функция отправки команды */
	Responce SendCommand(LPCBYTE pbCommand, INT pbCommandSize) {
		/* Выделение буфера на 256 значений  */
		DWORD bufferSize = 256;
		vector<BYTE> buffer(bufferSize);
		LONG lReturn = SCARD_E_NOT_READY;

		/* Совершаем попытки считать данные, пока не получится */
		while (lReturn != SCARD_S_SUCCESS) {
			/* Пытаемся считать данные */
			lReturn = SCardTransmit(hCardHandle, SCARD_PCI_T1, pbCommand, pbCommandSize, nullptr, buffer.data(), &bufferSize);

			/* Если произошлша ошибка нехватки буфера */
			if (lReturn == SCARD_E_INSUFFICIENT_BUFFER) {
				/* Увеличиваем размер буфера вдвое */
				bufferSize *= 2;

				/* Убеждаемся, что размер буфера не будет больше 10 КБайт */
				if (bufferSize >= (2 << 12)) {
					cerr << "Файл слишком большой" << endl;
					return {};
				}

				/* Увеличиваем размер буфера */
				buffer.reserve(bufferSize);
			}
			else if (lReturn != SCARD_S_SUCCESS) {
				cerr << "Произошла ошибка считывания, код ошибки: " << hex << lReturn << dec << endl;
				return {};
			}
		}

		/* Получаем код ответа */
		unsigned char SW1 = buffer[(size_t)(bufferSize - 2)];
		unsigned char SW2 = buffer[(size_t)(bufferSize - 1)];

		/* Уменьшаем размер буфера ровно до размера данных */
		buffer.resize((size_t)(bufferSize - 2));

		/* Производим перемещение буфера в структуру ответа, а также записываем коды ответа */
		return { move(buffer), SW1, SW2 };
	}

	~CardReader() {
		SCardDisconnect(hCardHandle, SCARD_LEAVE_CARD);
		SCardReleaseContext(hScardContext);
	}
};