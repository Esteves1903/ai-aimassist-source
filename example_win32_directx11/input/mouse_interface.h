#pragma once
#include <windows.h>
#include <iostream>
#include "pragma.h"
#include "xorstr.hpp"
#include "defines.h"

// Shit method to try bypass anticheats...
inline BYTE NtUserInjectMouseInputBytes[30];

class mouse_interface
{
private:
	BOOLEAN WINAPI InjectMouseInputSpoofFunction(InjectedInputMouseInfo* Struct, int Value) {
		LPVOID NtUserInjectMouseInput_Spoof = VirtualAlloc(0, 0x1000, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
		if (!NtUserInjectMouseInput_Spoof) return FALSE;
		memcpy(NtUserInjectMouseInput_Spoof, NtUserInjectMouseInputBytes, sizeof(NtUserInjectMouseInputBytes));
		NTSTATUS Result = reinterpret_cast<NTSTATUS(NTAPI*)(InjectedInputMouseInfo*, int)>(NtUserInjectMouseInput_Spoof)(Struct, Value);
		SecureZeroMemory(NtUserInjectMouseInput_Spoof, 0x1000);
		VirtualFree(NtUserInjectMouseInput_Spoof, 0, MEM_RELEASE);
		return (Result > 0);
	}

public:
	mouse_interface()
	{
		LoadLibrary(xorstr("user32.dll"));
		HMODULE win32u = LoadLibrary(xorstr("win32u.dll"));

		if (!win32u)
		{
			return;
		}

		void* NtUserInjectMouseInputAddress = (void*)GetProcAddress(win32u, xorstr("NtUserInjectMouseInput"));

		if (!NtUserInjectMouseInputAddress)
		{
			return;
		}

		memcpy(NtUserInjectMouseInputBytes, NtUserInjectMouseInputAddress, sizeof(NtUserInjectMouseInputBytes));
	}

	bool left_down(int x = 0, int y = 0) {
		InjectedInputMouseInfo temp = {};
		temp.mouse_options = InjectedInputMouseOptions::left_down;
		temp.move_direction_x = x;
		temp.move_direction_y = y;
		return InjectMouseInputSpoofFunction(&temp, 1);
	}

	bool left_up(int x = 0, int y = 0) {
		InjectedInputMouseInfo temp = {};
		temp.mouse_options = InjectedInputMouseOptions::left_up;
		temp.move_direction_x = x;
		temp.move_direction_y = y;
		return InjectMouseInputSpoofFunction(&temp, 1);
	}

	bool right_down(int x = 0, int y = 0) {
		InjectedInputMouseInfo temp = {};
		temp.mouse_options = InjectedInputMouseOptions::right_down;
		temp.move_direction_x = x;
		temp.move_direction_y = y;
		return InjectMouseInputSpoofFunction(&temp, 1);
	}

	bool right_up(int x = 0, int y = 0) {
		InjectedInputMouseInfo temp = {};
		temp.mouse_options = InjectedInputMouseOptions::right_up;
		temp.move_direction_x = x;
		temp.move_direction_y = y;
		return InjectMouseInputSpoofFunction(&temp, 1);
	}

	bool move(int x = 0, int y = 0) {
		InjectedInputMouseInfo temp = {};
		temp.mouse_options;
		temp.move_direction_x = x;
		temp.move_direction_y = y;
		return InjectMouseInputSpoofFunction(&temp, 1);
	}
};
