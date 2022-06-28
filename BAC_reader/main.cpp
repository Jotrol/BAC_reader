#include "GUI.hpp"

string getMRZCode() {
	return "5200000001RUS8008046F0902274<<<<<<<<<<<<<<06";
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