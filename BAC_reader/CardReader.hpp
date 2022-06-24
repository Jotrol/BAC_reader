#pragma once
#include <winscard.h>
#include <vector>
#include <exception>
#include <string>
#include <iostream>
using namespace std;

#pragma comment(lib, "winscard.lib")

class CardReader {
private:
	SCARDCONTEXT hScardContext;
	SCARDHANDLE hCardHandle;

public:
	CardReader() : hScardContext(), hCardHandle() {
		/* ��������� ������������� ��������� ��� ������ �� �����-������� */
		LONG lResult = SCardEstablishContext(SCARD_SCOPE_USER, nullptr, nullptr, &hScardContext);
		if (lResult != SCARD_S_SUCCESS) {
			throw exception("���������� ���������������� �������� SCARDCONTEXT");
		}
	}

	/* ��������� ������ ������������ � ������� */
	vector<wstring> GetReadersList() {
		/* �������� ����� ������������ � ������� */
		/* ��������� ���������� - ����� ������, ������ � ������� �������� ������, ��������� ������ - � ����� ������ */
		LPWSTR pmszReaders = nullptr;
		DWORD cch = SCARD_AUTOALLOCATE;
		LONG lResult = SCardListReaders(hScardContext, SCARD_ALL_READERS, (LPWSTR)&pmszReaders, &cch);
		if (lResult != SCARD_S_SUCCESS) {
			cerr << "�� ������� ������� ��������� ������ � �������" << endl;
			SCardFreeMemory(hScardContext, pmszReaders);
			return {};
		}

		/* ���������� ��� ��������� ������ � ������ ��� */
		vector<wstring> readerNames = {};
		LPWSTR pReader = pmszReaders;
		if (!pReader) {
			cerr << "������ ��������� ������� ����" << endl;
			return {};
		}
		while (*pReader != '\0') {
			size_t readerNameLen = wcslen(pReader);

			/* ���������� ����������� �������� �� ��������� ������ */
			wstring tempString = L"";
			tempString.append(pReader, readerNameLen);

			/* ������� ����� ��������� � ������ ��� */
			readerNames.push_back(tempString);

			/* ������� �����������, ��� ��� ��� ������ �������� ������, ����������� - ������� ������ */
			pReader = pReader + readerNameLen + 1;
		}

		/* ������� ������ ��-��� ��� ������� */
		SCardFreeMemory(hScardContext, pmszReaders);

		/* ���������� ������ ��� ������� */
		return readerNames;
	}

	/* ������� ���������� ���������� � ������ */
	bool CardConnect(const wstring& readerName) {
		/* ������������ �������� */
		DWORD dwAP = 0;

		/* ��������������� ������������� ���������� �� ����� ���������� �� ��������� T1 (�������������) */
		LONG lResult = SCardConnect(hScardContext, readerName.data(), SCARD_SHARE_EXCLUSIVE, SCARD_PROTOCOL_Tx, &hCardHandle, &dwAP);

		/* ���� ��������� ������ ��� (�����) �� ��� �������� ���������� */
		/* �� ��� ����� */
		if (lResult != SCARD_S_SUCCESS) {
			cerr << "�� ���������� ��������� ���������� � ������!" << endl;
			return false;
		}
		if (dwAP != SCARD_PROTOCOL_T1) {
			cerr << "�������� �������� ����������" << endl;
			return false;
		}
		return true;
	}

	/* ������� �������� ������� */
	vector<BYTE> SendCommand(vector<BYTE> commandAPDU) const {
		/* ��������� ������ �� 256 ��������  */
		DWORD bufferSize = 256;
		DWORD apduSize = (DWORD)commandAPDU.size();
		vector<BYTE> buffer(bufferSize);
		LONG lReturn = SCARD_E_NOT_READY;

		/* ��������� ������� ������� ������, ���� �� ��������� */
		while (lReturn != SCARD_S_SUCCESS) {
			/* �������� ������� ������ */
			lReturn = SCardTransmit(hCardHandle, SCARD_PCI_T1, commandAPDU.data(), apduSize, nullptr, buffer.data(), &bufferSize);

			/* ���� ���������� ������ �������� ������ */
			if (lReturn == SCARD_E_INSUFFICIENT_BUFFER) {
				/* ����������� ������ ������ ����� */
				bufferSize *= 2;

				/* ����������, ��� ������ ������ �� ����� ������ 10 ����� */
				if (bufferSize >= (2 << 12)) {
					throw std::exception("������: ���� ������� �������");
				}

				/* ����������� ������ ������ */
				buffer.reserve(bufferSize);
			}
			else if (lReturn != SCARD_S_SUCCESS) {
				cerr << hex << lReturn << dec << endl;
				throw std::exception("������: ������ ����������");
			}
		}
		
		buffer.resize(bufferSize);
		return buffer;
	}

	~CardReader() {
		SCardDisconnect(hCardHandle, SCARD_LEAVE_CARD);
		SCardReleaseContext(hScardContext);
	}
};