// uninstall_proto.cpp - Uninstall Proto IME.
// Removes from language list, unregisters DLL, restarts text input.
#include <windows.h>
#include <initguid.h>
#include <msctf.h>
#include <string>
#include <fstream>

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// {F4E5D6C7-B8A9-40BC-ABCD-EF1234567890}
DEFINE_GUID(CLSID_Proto, 0xF4E5D6C7, 0xB8A9, 0x40BC, 0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56, 0x78, 0x90);
DEFINE_GUID(GUID_ProtoPrf, 0xE5F6A7B8, 0xC9D0, 0x41CD, 0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56, 0x78, 0x90);
const LANGID kLang = 0x0804;

static std::wstring g_dllPath;
static std::wstring g_log;

static void log(const wchar_t* msg) {
    std::wofstream ofs(g_log, std::ios::app);
    if (ofs) ofs << msg << L"\n";
}

// Find proto.dll (same directory as this exe)
static bool findDll() {
    wchar_t self[MAX_PATH]; GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring dir(self); dir = dir.substr(0, dir.rfind(L'\\'));

    g_dllPath = dir + L"\\proto_v2.dll";
    if (GetFileAttributesW(g_dllPath.c_str()) != INVALID_FILE_ATTRIBUTES) return true;
    g_dllPath = dir + L"\\proto_new.dll";
    if (GetFileAttributesW(g_dllPath.c_str()) != INVALID_FILE_ATTRIBUTES) return true;
    g_dllPath = dir + L"\\proto-install.dll";
    if (GetFileAttributesW(g_dllPath.c_str()) != INVALID_FILE_ATTRIBUTES) return true;
    g_dllPath = dir + L"\\proto2.dll";
    if (GetFileAttributesW(g_dllPath.c_str()) != INVALID_FILE_ATTRIBUTES) return true;
    g_dllPath = dir + L"\\proto.dll";
    if (GetFileAttributesW(g_dllPath.c_str()) != INVALID_FILE_ATTRIBUTES) return true;

    log(L"proto.dll not found");
    return false;
}

// Remove IME from the Chinese language entry in the Windows language list
static bool removeFromLangList() {
    wchar_t cmd[1024];
    swprintf_s(cmd, L"powershell -NoProfile -Command \"$L=Get-WinUserLanguageList;"
        L"($L|?{$_.LanguageTag -like 'zh*'}|select -f 1).InputMethodTips.Remove("
        L"'0804:{F4E5D6C7-B8A9-40BC-ABCD-EF1234567890}{E5F6A7B8-C9D0-41CD-ABCD-EF1234567890}');"
        L"Set-WinUserLanguageList $L -Force\"");

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    if (!CreateProcessW(nullptr, cmd, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
        return false;
    WaitForSingleObject(pi.hProcess, 15000);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return true;
}

// Run regsvr32 /s /u to call DllUnregisterServer
static bool doUnregister() {
    std::wstring cmd = L"regsvr32 /s /u \"" + g_dllPath + L"\"";
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    if (!CreateProcessW(nullptr, (LPWSTR)cmd.c_str(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
        return false;
    DWORD exitCode = 0;
    WaitForSingleObject(pi.hProcess, 30000);
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return exitCode == 0;
}

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
    g_log = std::wstring(tmp) + L"proto-uninstall.log";
    log(L"Starting uninstall");

    if (!findDll()) {
        MessageBoxW(nullptr, L"Cannot find proto.dll next to this program.", L"Error", MB_ICONERROR);
        return 1;
    }
    if (!removeFromLangList()) {
        MessageBoxW(nullptr, L"Failed to remove from language list.", L"Error", MB_ICONERROR);
        return 1;
    }
    if (!doUnregister()) {
        MessageBoxW(nullptr, L"DLL unregistration failed.", L"Error", MB_ICONERROR);
        return 1;
    }

    // Restart text input subsystem
    system("taskkill /f /im ctfmon.exe >nul 2>&1");
    system("taskkill /f /im TextInputHost.exe >nul 2>&1");

    MessageBoxW(nullptr,
        L"Proto IME has been uninstalled.\n\n"
        L"You may now delete the program files manually.",
        L"Uninstall Complete", MB_OK | MB_ICONINFORMATION);
    return 0;
}
