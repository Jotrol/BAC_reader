#include <Windows.h>
#include "CardReader.hpp"
using namespace std;



//INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, INT nCmdShow) {
int main() {
	setlocale(LC_ALL, "rus");

	string mrz_line1 = "";
	string mrz_line2 = "";
	cout << "Введите строку MRZ1: "; cin >> mrz_line1;
	cout << "Введите строку MRZ2: "; cin >> mrz_line2;
	
	/* А дальше строки распознаются и отдают валидный словарь */

	return 0;
}