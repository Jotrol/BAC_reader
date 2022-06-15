#include <Windows.h>
#include <winscard.h>
#include <iostream>
#include <exception>
#include <vector>
#include <memory>
using namespace std;

#pragma comment(lib, "winscard.lib")

struct Responce {
	vector<BYTE> responceData;

	unsigned char SW1;
	unsigned char SW2;
};

class WinscardReader {
private:
	SCARDCONTEXT hScardContext;
	SCARDHANDLE hCardHandle;

public:
	WinscardReader() : hScardContext(), hCardHandle() {
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
			int readerNameLen = wcslen(pReader);

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

	/* ������� "����������" ����� - ��� �������� ������ */
	bool CardDisconnect() {
		return SCardDisconnect(hCardHandle, SCARD_LEAVE_CARD) == SCARD_S_SUCCESS;
	}

	/* ������� �������� ������� */
	Responce SendCommand(LPCBYTE pbCommand, INT pbCommandSize) {
		/* ��������� ������ �� 256 ��������  */
		DWORD bufferSize = 256;
		vector<BYTE> buffer(bufferSize);
		LONG lReturn = SCARD_E_NOT_READY;

		/* ��������� ������� ������� ������, ���� �� ��������� */
		while (lReturn != SCARD_S_SUCCESS) {
			/* �������� ������� ������ */
			lReturn = SCardTransmit(hCardHandle, SCARD_PCI_T1, pbCommand, pbCommandSize, nullptr, buffer.data(), &bufferSize);

			/* ���� ���������� ������ �������� ������ */
			if (lReturn == SCARD_E_INSUFFICIENT_BUFFER) {
				/* ����������� ������ ������ ����� */
				bufferSize *= 2;
				
				/* ����������, ��� ������ ������ �� ����� ������ 10 ����� */
				if (bufferSize >= (2 << 12)) {
					cerr << "���� ������� �������" << endl;
					return {};
				}

				/* ����������� ������ ������ */
				buffer.reserve(bufferSize);
			}
			else if(lReturn != SCARD_S_SUCCESS) {
				cerr << "��������� ������ ����������, ��� ������: " << hex << lReturn << dec << endl;
				return {};
			}
		}

		/* �������� ��� ������ */
		unsigned char SW1 = buffer[(size_t)(bufferSize - 2)];
		unsigned char SW2 = buffer[(size_t)(bufferSize - 1)];

		/* ��������� ������ ������ ����� �� ������� ������ */
		buffer.resize((size_t)(bufferSize - 2));

		/* ���������� ����������� ������ � ��������� ������, � ����� ���������� ���� ������ */
		return { move(buffer), SW1, SW2 };
	}

	~WinscardReader() {
		SCardReleaseContext(hScardContext);
	}
};

//INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, INT nCmdShow) {
int main() {
	setlocale(LC_ALL, "rus");

	WinscardReader cardReader;
	vector<wstring> readers = cardReader.GetReadersList();
	for (const auto& readerName : readers) {
		wcout << L"Reader: " << readerName.data() << endl;
	}

	cin.get();
	if (cardReader.CardConnect(readers[0])) {
		cout << "������� ���������" << endl;

		/* ���������� ������������ ���� */
		BYTE SelectAPDU[] = { 0x00, 0xA4, 0x04, 0x0C, 0x07, 0xA0, 0x00, 0x00, 0x02, 0x47, 0x10, 0x01 };
		Responce responce = cardReader.SendCommand(SelectAPDU, sizeof(SelectAPDU));
		if (responce.SW1 == 0x90 && responce.SW2 == 0x00) {
			cout << "������� ������� ����������� ���������� ����" << endl;
			cout << "�����: 0x90 0x00" << endl << endl;
		}
		
		/* ��������� ���������� ����� �� ����� */
		BYTE GetRndID[] = { 0x00, 0x84, 0x00, 0x00, 0x08 };
		responce = cardReader.SendCommand(GetRndID, sizeof(GetRndID));
		if (responce.SW1 == 0x90 && responce.SW2 == 0x00 && responce.responceData.size() == 8) {
			cout << "������� �������� RND.IC" << endl;
			unsigned long long rndID = 0ULL;
			memcpy(&rndID, responce.responceData.data(), 8);
			cout << "��������: " << rndID << endl << endl;
		}

		cardReader.CardDisconnect();
	}
	
	return 0;
}