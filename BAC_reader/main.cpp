#include <Windows.h>
#include "Crypto.hpp"
#include "CardReader.hpp"
using namespace std;


int hashingSample() {
	BYTE msgToHash[] = "Some message to get hash of";

	Crypto::SHA1 sha1;
	vector<BYTE> pbHash1 = sha1.hash(msgToHash, sizeof(msgToHash) - 1);

	for (int i = 0; i < pbHash1.size(); i += 1) {
		cout << hex << (unsigned int)pbHash1[i];
	}
	cout << endl;

	return 1;
}

//INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, INT nCmdShow) {
int main() {
	setlocale(LC_ALL, "rus");

	hashingSample();
	//encryptingSample();
	
	return 0;
}