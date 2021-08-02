#define FPS_TARGET 144
#define FPS_OFFSET -2

#define _WIN32_WINNT _WIN32_WINNT_WIN7
#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <string>
#include <thread>
#include <psapi.h>

bool bStop = false;

// didnt made this pattern scan - c+p'd from somewhere
uintptr_t PatternScan(void* module, const char* signature)
{
	static auto pattern_to_byte = [](const char* pattern) {
		auto bytes = std::vector<int>{};
		auto start = const_cast<char*>(pattern);
		auto end = const_cast<char*>(pattern) + strlen(pattern);

		for (auto current = start; current < end; ++current) {
			if (*current == '?') {
				++current;
				if (*current == '?')
					++current;
				bytes.push_back(-1);
			}
			else {
				bytes.push_back(strtoul(current, &current, 16));
			}
		}
		return bytes;
	};

	auto dosHeader = (PIMAGE_DOS_HEADER)module;
	auto ntHeaders = (PIMAGE_NT_HEADERS)((std::uint8_t*)module + dosHeader->e_lfanew);

	auto sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;
	auto patternBytes = pattern_to_byte(signature);
	auto scanBytes = reinterpret_cast<std::uint8_t*>(module);

	auto s = patternBytes.size();
	auto d = patternBytes.data();

	for (auto i = 0ul; i < sizeOfImage - s; ++i) {
		bool found = true;
		for (auto j = 0ul; j < s; ++j) {
			if (scanBytes[i + j] != d[j] && d[j] != -1) {
				found = false;
				break;
			}
		}
		if (found) {
			return (uintptr_t)&scanBytes[i];
		}
	}
	return 0;
}

std::string GetLastErrorAsString(DWORD code)
{
	LPSTR buf = nullptr;
	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buf, 0, NULL);
	std::string ret = buf;
	LocalFree(buf);
	return ret;
}

bool GetModule(DWORD pid, std::string ModuleName, PMODULEENTRY32 pEntry)
{
	if (!pEntry)
		return false;

	MODULEENTRY32 mod32{};
	mod32.dwSize = sizeof(mod32);
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
	for (Module32First(snap, &mod32); Module32Next(snap, &mod32);)
	{
		if (mod32.th32ProcessID != pid)
			continue;

		if (mod32.szModule == ModuleName)
		{
			*pEntry = mod32;
			break;
		}
	}
	CloseHandle(snap);

	return pEntry->modBaseAddr;
}

DWORD GetPID(std::string ProcessName)
{
	DWORD pid = 0;
	PROCESSENTRY32 pe32{};
	pe32.dwSize = sizeof(pe32);
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	for (Process32First(snap, &pe32); Process32Next(snap, &pe32);)
	{
		if (pe32.szExeFile == ProcessName)
		{
			pid = pe32.th32ProcessID;
			break;
		}
	}
	CloseHandle(snap);
	return pid;
}

std::string ReadConfig()
{
	HANDLE hFile = CreateFileA("config", GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		printf("Config not found - Starting first time setup\nPlease leave this open and start the game\nThis only need to be done once\n\n");
		printf("Waiting for game...\n");

		DWORD pid = 0;
		while (!(pid = GetPID("YuanShen.exe")) && !(pid = GetPID("GenshinImpact.exe")))
			std::this_thread::sleep_for(std::chrono::milliseconds(200));

		HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid);

		char szPath[MAX_PATH]{};
		DWORD length = sizeof(szPath);
		QueryFullProcessImageNameA(hProcess, 0, szPath, &length);

		// this shouldn't fail
		hFile = CreateFileA("config", GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

		DWORD written = 0;
		WriteFile(hFile, szPath, strlen(szPath), &written, nullptr);
		CloseHandle(hFile);

		HWND hwnd = nullptr;
		while (!(hwnd = FindWindowA("UnityWndClass", nullptr)))
			std::this_thread::sleep_for(std::chrono::milliseconds(200));

		DWORD ExitCode = STILL_ACTIVE;
		while (ExitCode == STILL_ACTIVE)
		{
			SendMessageA(hwnd, WM_CLOSE, 0, 0);
			GetExitCodeProcess(hProcess, &ExitCode);
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}

		// wait for the game to close then continue
		WaitForSingleObject(hProcess, -1);
		CloseHandle(hProcess);

		system("cls");
		return szPath;
	}

	DWORD FileSize = GetFileSize(hFile, nullptr);

	std::string buf;
	buf.reserve(MAX_PATH);
	ZeroMemory(buf.data(), MAX_PATH);

	DWORD read = 0;
	ReadFile(hFile, buf.data(), FileSize, &read, nullptr);
	CloseHandle(hFile);

	// check if the path is valid - if not then redo the setup again
	if (GetFileAttributesA(buf.c_str()) == INVALID_FILE_ATTRIBUTES)
	{
		printf("Looks like you've moved your game somewhere else - Lets setup again\n");
		DeleteFileA("config");
		return ReadConfig();
	}

	return buf.c_str();
}

int main(int argc, char** argv)
{
	SetConsoleTitleA("");
	int TargetFPS = FPS_TARGET + FPS_OFFSET;

	std::string CommandLine{};
	if (argc > 1)
	{
		for (int i = 1; i < argc; i++)
			CommandLine += argv[i] + std::string(" ");
	}
	
	// read path from config
	std::string ProcessPath = ReadConfig();
	std::string ProcessDir{};

	printf("FPS Unlocker\n");
	printf("Game: %s\n\n", ProcessPath.c_str());
	ProcessDir = ProcessPath.substr(0, ProcessPath.find_last_of("\\"));

	STARTUPINFOA si{};
	PROCESS_INFORMATION pi{};
	if (!CreateProcessA(ProcessPath.c_str(), (LPSTR)CommandLine.c_str(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
	{
		DWORD code = GetLastError();
		printf("CreateProcess failed (%d): %s\n", code, GetLastErrorAsString(code).c_str());
		return 0;
	}

	CloseHandle(pi.hThread);
	printf("PID: %d\n", pi.dwProcessId);

	MODULEENTRY32 hUnityPlayer{};
	while (!GetModule(pi.dwProcessId, "UnityPlayer.dll", &hUnityPlayer))
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

	printf("UnityPlayer: %X%X\n", (uintptr_t)hUnityPlayer.modBaseAddr >> 32 & -1, hUnityPlayer.modBaseAddr);

	LPVOID mem = VirtualAlloc(nullptr, hUnityPlayer.modBaseSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!mem)
	{
		DWORD code = GetLastError();
		printf("VirtualAlloc failed (%d): %s\n", code, GetLastErrorAsString(code).c_str());
		return 0;
	}

	ReadProcessMemory(pi.hProcess, hUnityPlayer.modBaseAddr, mem, hUnityPlayer.modBaseSize, nullptr);

	printf("Searching for pattern...\n");
	/*
		 7F 0F              jg   0x11
		 8B 05 ? ? ? ?      mov eax, dword ptr[rip+?]
	*/
	uintptr_t address = PatternScan(mem, "7F 0F 8B 05 ? ? ? ?");
	if (!address)
	{
		printf("outdated pattern\n");
		return 0;
	}

	// calculate the offset to where the fps value is stored
	uintptr_t pfps = 0;
	{
		uintptr_t rip = address + 2;
		uint32_t rel = *(uint32_t*)(rip + 2);
		pfps = rip + rel + 6;
		pfps -= (uintptr_t)mem;
		printf("FPS Offset: %X\n", pfps);
		pfps = (uintptr_t)hUnityPlayer.modBaseAddr + pfps;
	}

	VirtualFree(mem, 0, MEM_RELEASE);
	printf("FPS_TARGET = %d\n", FPS_TARGET);
	printf("Keep this console open to maintain unlocked fps.\n");

	DWORD ExitCode = STILL_ACTIVE;
	while (ExitCode == STILL_ACTIVE)
	{
		GetExitCodeProcess(pi.hProcess, &ExitCode);
		// runs a check every 2 seconds
		std::this_thread::sleep_for(std::chrono::seconds(2));
		int fps = 0;
		ReadProcessMemory(pi.hProcess, (LPVOID)pfps, &fps, sizeof(fps), nullptr);
		if (fps == -1)
			continue;
		if (fps != TargetFPS)
			WriteProcessMemory(pi.hProcess, (LPVOID)pfps, &TargetFPS, sizeof(TargetFPS), nullptr);
	}

	bStop = true;
	CloseHandle(pi.hProcess);
	TerminateProcess((HANDLE)-1, 0);

	return 0;
}
