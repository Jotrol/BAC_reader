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
        ss << "��� ������ WinsCard: 0x" << hex << ulError;
        return ss.str();
    }

public:
    CardReader() : hScardContext() {
        /* ��������� ������������� ��������� ��� ������ �� �����-������� */
        LONG lResult = SCardEstablishContext(SCARD_SCOPE_USER, nullptr, nullptr, &hScardContext);
        if (lResult != SCARD_S_SUCCESS) {
            throw std::exception("���������� ���������������� �������� SCARDCONTEXT");
        }
    }

    /* ��������� ������ ������������ � ������� */
    vector<wstring> getReadersList() const {
        /* �������� ����� ������������ � ������� */
        /* ��������� ���������� - ����� ������, ������ � ������� �������� ������, ��������� ������ - � ����� ������ */
        LPWSTR pmszReaders = nullptr;
        DWORD cch = SCARD_AUTOALLOCATE;
        LONG lResult = SCardListReaders(hScardContext, SCARD_ALL_READERS, (LPWSTR)&pmszReaders, &cch);
        if (lResult != SCARD_S_SUCCESS) {
            throw std::exception("�� ������� ������� ��������� ������ � �������");
        }

        /* ���������� ��� ��������� ������ � ������ ��� */
        vector<wstring> readerNames = {};
        LPWSTR pReader = pmszReaders;
        if (!pReader) {
            throw std::exception("������ ��������� ������� ����");
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
    SCARDHANDLE cardConnect(const wstring& readerName) const {
        SCARDHANDLE hCard = 0;
        DWORD dwAp = 0;

        /* ��������������� ������������� ���������� �� ����� ���������� �� ��������� T1 (�������������) */
        LONG lResult = SCardConnect(hScardContext, readerName.data(), SCARD_SHARE_EXCLUSIVE, SCARD_PROTOCOL_Tx, &hCard, &dwAp);

        /* ���� ��������� ������ ��� (�����) �� ��� �������� ���������� */
        /* �� ��� ����� */
        if (lResult != SCARD_S_SUCCESS) {
            throw std::exception("�� ���������� ��������� ���������� � ������!");
        }
        if (dwAp != SCARD_PROTOCOL_T1) {
            throw std::exception("�������� �������� ����������");
        }
        return hCard;
    }

    /* ������� ���������� ����� */
    void cardDisconnect(SCARDHANDLE hCard) const {
        SCardDisconnect(hCard, SCARD_LEAVE_CARD);
    }

	/* ������� �������� ������� */
	vector<BYTE> sendCommand(SCARDHANDLE hCard, vector<BYTE> commandAPDU) const {
		/* ��������� ������ �� 256 ��������  */
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