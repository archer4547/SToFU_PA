#include "mhook-lib/mhook.h"
#include <Windows.h>
#include <cstdlib>
#include <string>
#include <chrono>
#include <iostream>

auto startup = std::chrono::high_resolution_clock::now();
int seen_numbers[20];

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
	// point the iterator to the end of the string
	int wstr_iter = 0;
	char c;
	while (true) {
		c = (char)lpText[wstr_iter];
		if (IsCharAlphaNumericW(lpText[wstr_iter]) || c == ' ' || c == ':') {
			wstr_iter++;
		}
		else 
			break;
	}

	// get the number from the string
	int random_number = 0;
	if (lpText[wstr_iter - 2] != ' ') {
		random_number = (int)lpText[wstr_iter - 2] - 48; // 48 - "0" char
	}
	if (std::isalpha(lpText[wstr_iter - 2])) {
		//std::abort();
	}
	else {
		random_number = random_number * 10 + (int)lpText[wstr_iter - 1] - 48;
	}
	seen_numbers[random_number - 1] = 1;

	// main hook part
	if (random_number == 1) {
		random_number = 100;
	}
	else if (random_number == 2) {
		std::abort();
	}
	std::wstring random_text = L"Random number is: " + std::to_wstring(random_number);

	// return injected MessageBox
	return TrueMessageBoxW(hWnd, random_text.c_str(), lpCaption, uType);

}

int wmain(int, WCHAR* []) {
	for (int i = 0; i < 20; i++) {
		seen_numbers[i] = 0;
	}


	if (Mhook_SetHook((PVOID*)&TrueMessageBoxW, reinterpret_cast<PVOID>(hookRandomMessageBoxW))) {
		std::cout << "Hook successfull\n";
	}
	
	int s = 0;
	while (s < 20) {
		s = 0;
		for (int i = 0; i < 20; i++) {
			s += seen_numbers[i];
		}
		showRandomMessageBox();
	}

	if (Mhook_Unhook((PVOID*)&TrueMessageBoxW)) {
		std::cout << "Unhooked successfully\n";
	}

	Sleep(5000);

	return 0;
}
