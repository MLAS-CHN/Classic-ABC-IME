#include <windows.h>
#include <initguid.h>
#include <msctf.h>
#include <shlobj.h>
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

static bool installFiles() {
    wchar_t self[MAX_PATH]; GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring dir(self); dir = dir.substr(0, dir.rfind(L'\\'));

    // Extract DLL from resource
    HRSRC res = FindResourceW(nullptr, L"PROTO_DLL", L"BINARY");
    if (!res) {
        // Fallback: look for proto.dll next to exe
        g_dllPath = dir + L"\\proto_v2.dll";
        if (GetFileAttributesW(g_dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            g_dllPath = dir + L"\\proto_new.dll";
            if (GetFileAttributesW(g_dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
                g_dllPath = dir + L"\\proto2.dll";
                if (GetFileAttributesW(g_dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
                    g_dllPath = dir + L"\\proto.dll";
                    if (GetFileAttributesW(g_dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
                        log(L"proto.dll not found next to exe");
                        return false;
                    }
                }
            }
        }
    } else {
        HGLOBAL hg = LoadResource(nullptr, res);
        void* data = LockResource(hg);
        DWORD size = SizeofResource(nullptr, res);
        g_dllPath = dir + L"\\proto-install.dll";
        HANDLE hf = CreateFileW(g_dllPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hf == INVALID_HANDLE_VALUE) return false;
        DWORD written; WriteFile(hf, data, size, &written, nullptr); CloseHandle(hf);
    }

    // Create data directory with minimal dummy data
    std::wstring dataDir = dir + L"\\data";
    CreateDirectoryW(dataDir.c_str(), nullptr);

    if (GetFileAttributesW((dir + L"\\data\\pinyin_map.txt").c_str()) == INVALID_FILE_ATTRIBUTES) {
        HANDLE h = CreateFileW((dataDir + L"\\pinyin_map.txt").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            const char* dummy = "a a\nb b\n";
            DWORD w; WriteFile(h, dummy, (DWORD)strlen(dummy), &w, nullptr); CloseHandle(h);
        }
        h = CreateFileW((dataDir + L"\\user_dict.txt").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
        h = CreateFileW((dataDir + L"\\char_freq.txt").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
    return true;
}

static bool doRegister() {
    std::wstring cmd = L"regsvr32 /s \"" + g_dllPath + L"\"";
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

static bool addToLangList() {
    wchar_t cmd[1024];
    swprintf_s(cmd, L"powershell -NoProfile -Command \"$L=Get-WinUserLanguageList;"
        L"($L|?{$_.LanguageTag -like 'zh*'}|select -f 1).InputMethodTips.Add("
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

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
    g_log = std::wstring(tmp) + L"proto-install.log";
    log(L"Starting install");

    if (!installFiles()) {
        MessageBoxW(nullptr, L"Failed to find or extract proto.dll", L"Error", MB_ICONERROR);
        return 1;
    }
    if (!doRegister()) {
        MessageBoxW(nullptr, L"Registration failed", L"Error", MB_ICONERROR);
        return 1;
    }
    if (!addToLangList()) {
        MessageBoxW(nullptr, L"Failed to add to language list", L"Error", MB_ICONERROR);
        return 1;
    }

    // Restart ctfmon
    system("taskkill /f /im ctfmon.exe >nul 2>&1");
    system("taskkill /f /im TextInputHost.exe >nul 2>&1");

    MessageBoxW(nullptr,
        L"Proto IME installed.\n\nPress Win+Space to switch.\nType letters to see the test window.",
        L"Install Complete", MB_OK | MB_ICONINFORMATION);
    return 0;
}
