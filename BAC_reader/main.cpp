#include <Windows.h>
#include "CardReader.hpp"
using namespace std;

struct UpperMRZ {
	char docType[2];
	char issuingState[3];
	char name[31];
};

struct DownerMRZ {
	char passNum[9];
	char controlZifr1[1];
	char nationality[3];
	char birthdate[6];
	char controlZifr2[1];
	char sex[1];
	char expires[6];
	char controlZifr3[1];
	char facultative[8];
};


//INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, INT nCmdShow) {
int main() {
	setlocale(LC_ALL, "rus");

	UpperMRZ* upper = (UpperMRZ*)"P<RUSSMIRNOVA<<VALENTINA<<<<<<<<<<<<<<<<<<<<";
	DownerMRZ* downer = (DownerMRZ*)"5200000001RUS8008046F0902274<<<<<<<<<<<06";
	
	/* А дальше строки распознаются и отдают валидный словарь */

	return 0;
}