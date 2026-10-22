#include "mhook-lib/mhook.h"
#include <Windows.h>
#include <cstdlib>
#include <string>
#include <chrono>
#include <iostream>

auto startup = std::chrono::high_resolution_clock::now();

typedef int(WINAPI* _MessageBoxW) (
	HWND hWnd,
	LPCWSTR lpText,
	LPCWSTR lpCaption,
    UINT uType
);

 _MessageBoxW TrueMessageBoxW = (_MessageBoxW)(void (*)(void))GetProcAddress(GetModuleHandle(L"user32.dll"), "MessageBoxW");


void showRandomMessageBox() {
	auto now = std::chrono::high_resolution_clock::now();
	srand(std::chrono::duration_cast<std::chrono::milliseconds>(now - startup).count());

	int rand_num = (rand() % 20) + 1;
	std::wstring random_text = L"Random number is: " + std::to_wstring(rand_num);
	MessageBoxW(NULL, random_text.c_str(), L"RNG", 0);
}

int WINAPI hookRandomMessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) {
	auto now = std::chrono::high_resolution_clock::now();
	srand(std::chrono::duration_cast<std::chrono::milliseconds>(now - startup).count());

	int rand_num = (rand() % 20) + 1;
	if (rand_num == 1) {
		rand_num = 100;
	}
	else if (rand_num == 2) {
		std::abort();
	}
	std::wstring random_text = L"Random number is: " + std::to_wstring(rand_num);
	return TrueMessageBoxW(NULL, random_text.c_str(), L"RNG", 0);
}

int wmain(int, WCHAR* []) {

	if (Mhook_SetHook((PVOID*)&TrueMessageBoxW, reinterpret_cast<PVOID>(hookRandomMessageBoxW))) {
		std::cout << "Hook successfull\n";
	}
	
	while (true) {
		showRandomMessageBox();
		Sleep(10);
	}

	Mhook_Unhook((PVOID*)hookRandomMessageBoxW);

	return 0;
}
