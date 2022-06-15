#include <Windows.h>
#include "CardReader.hpp"
using namespace std;

//INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, INT nCmdShow) {
int main() {
	setlocale(LC_ALL, "rus");

	CardReader cardReader;
	vector<wstring> readers = cardReader.GetReadersList();
	for (const auto& readerName : readers) {
		wcout << L"Reader: " << readerName.data() << endl;
	}

	cin.get();
	if (cardReader.CardConnect(readers[0])) {
		cout << "Успешно соединено" << endl;

		/* Приложение электронного МСПД */
		BYTE SelectAPDU[] = { 0x00, 0xA4, 0x04, 0x0C, 0x07, 0xA0, 0x00, 0x00, 0x02, 0x47, 0x10, 0x01 };
		Responce responce = cardReader.SendCommand(SelectAPDU, sizeof(SelectAPDU));
		if (responce.SW1 == 0x90 && responce.SW2 == 0x00) {
			cout << "Успешно выбрано стандартное приложение МСПД" << endl;
			cout << "Ответ: 0x90 0x00" << endl << endl;
		}
		
		/* Получение случайного числа от карты */
		BYTE GetRndID[] = { 0x00, 0x84, 0x00, 0x00, 0x08 };
		responce = cardReader.SendCommand(GetRndID, sizeof(GetRndID));
		if (responce.SW1 == 0x90 && responce.SW2 == 0x00 && responce.responceData.size() == 8) {
			cout << "Успешно получено RND.IC" << endl;
			unsigned long long rndID = 0ULL;
			memcpy(&rndID, responce.responceData.data(), 8);
			cout << "Значение: " << rndID << endl << endl;
		}
	}
	
	return 0;
}