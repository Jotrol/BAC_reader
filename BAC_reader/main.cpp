#include <Windows.h>

#include "Passport.hpp"

#include <vector>
#include <fstream>
#include <iomanip>
#include <random>
#include <ctime>
using namespace std;

string getMRZCode1() {
	return "5200000001RUS8008046F0902274<<<<<<<<<<<<<<<06";
}

int main() {
	srand((UINT)time(NULL));
	setlocale(LC_ALL, "rus");

	CardReader reader;
	auto readersList = reader.getReadersList();
	auto& readerName = readersList[0];

	cin.get();
	Passport passport;

	if (!passport.connectPassport(reader, readerName)) {
		return 1;
	}

	if (!passport.perfomBAC(reader, getMRZCode1())) {
		return 1;
	}

	if (!passport.readDG(reader, Passport::DGFILES::EFCOM)) {
		return 1;
	}

	if (!passport.readDG(reader, Passport::DGFILES::DG1)) {
		return 1;
	}

	if (!passport.readDG(reader, Passport::DGFILES::DG2)) {
		return 1;
	}

	if (!passport.disconnectPassport(reader)) {
		return 1;
	}

	return 0;
}