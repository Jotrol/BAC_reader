#include "GUI.hpp"

string getMRZCode() {
	return "5200000001RUS8008046F0902274<<<<<<<<<<<<<<06";
		   "7400030978RUS8908272F2403052<<<<<<<<<<<<<<08";
		   "BSY0867554RUS7001017M2212038S1<<<<<<<<<<<<90";
}


INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, INT nCmdShow) {
	try {
		GUI::MainWindow mainWindow(hInst);
		mainWindow.run();
	}
	catch (std::exception& e) {
		MessageBoxA(nullptr, e.what(), "Îøèáêà", MB_OK | MB_ICONERROR);
	}

	return 0;
}