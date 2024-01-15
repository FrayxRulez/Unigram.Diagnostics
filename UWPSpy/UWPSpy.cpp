#include "stdafx.h"

#include "tap.hpp"

using PFN_INITIALIZE_XAML_DIAGNOSTICS_EX =
    decltype(&InitializeXamlDiagnosticsEx);

CAppModule _Module;

namespace {

ATOM RegisterDialogClass(LPCTSTR lpszClassName, HINSTANCE hInstance) {
    WNDCLASS wndcls;
    ::GetClassInfo(hInstance, MAKEINTRESOURCE(32770), &wndcls);

    // Set our own class name.
    wndcls.lpszClassName = lpszClassName;

    // Just register the class.
    return ::RegisterClass(&wndcls);
}

bool GetLoadedDllPath(DWORD processId,
                      PCWSTR dllName,
                      WCHAR resultDllPath[MAX_PATH]) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    bool succeeded = false;

    MODULEENTRY32 entry{
        .dwSize = sizeof(entry),
    };
    if (Module32First(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szModule, dllName) == 0) {
                wcscpy_s(resultDllPath, MAX_PATH, entry.szExePath);
                succeeded = true;
                break;
            }
        } while (Module32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return succeeded;
}

HRESULT UwpInitializeXamlDiagnostics(DWORD pid, PCWSTR dllLocation) {
    const HMODULE wux(LoadLibraryEx(L"Windows.UI.Xaml.dll", nullptr,
                                    LOAD_LIBRARY_SEARCH_SYSTEM32));
    if (!wux) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    const auto ixde = reinterpret_cast<PFN_INITIALIZE_XAML_DIAGNOSTICS_EX>(
        GetProcAddress(wux, "InitializeXamlDiagnosticsEx"));
    if (!ixde) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    return ixde(L"VisualDiagConnection1", pid, L"", dllLocation,
                CLSID_UWPSpyTAP, nullptr);
}

}  // namespace

BOOL WINAPI DllMain(HINSTANCE hinstDLL,  // handle to DLL module
                    DWORD fdwReason,     // reason for calling function
                    LPVOID lpvReserved)  // reserved
{
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH: {
            HRESULT hRes = _Module.Init(nullptr, hinstDLL);
            ATLASSERT(SUCCEEDED(hRes));

            RegisterDialogClass(L"UWPSpy", hinstDLL);
        } break;

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;

        case DLL_PROCESS_DETACH:
            UnregisterClass(L"UWPSpy", hinstDLL);

            _Module.Term();
            break;
    }

    return TRUE;
}

HRESULT WINAPI start(DWORD pid, DWORD framework) {
    AllowSetForegroundWindow(pid);

    // Calling InitializeXamlDiagnosticsEx the second time will reset the
    // existing element tree callbacks. Therefore, first check for existing
    // windows for the target process.

    struct EnumWindowsData {
        DWORD pid;
        BOOL found;
    };

    EnumWindowsData data = {pid, FALSE};

    EnumWindows(
        [](HWND hWnd, LPARAM lParam) -> BOOL {
            EnumWindowsData& data = *reinterpret_cast<EnumWindowsData*>(lParam);

            DWORD windowPid;
            if (!GetWindowThreadProcessId(hWnd, &windowPid) ||
                windowPid != data.pid) {
                return TRUE;
            }

            WCHAR windowClass[16];
            if (!GetClassName(hWnd, windowClass, ARRAYSIZE(windowClass)) ||
                _wcsicmp(windowClass, L"UWPSpy") != 0) {
                return TRUE;
            }

            //data.found = TRUE;
            //PostMessage(hWnd, CMainDlg::UWM_ACTIVATE_WINDOW, 0, 0);
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&data));

    if (data.found) {
        return S_OK;
    }

    WCHAR location[MAX_PATH];
    switch (GetModuleFileName(_Module.GetModuleInstance(), location,
                              ARRAYSIZE(location))) {
        case 0:
        case ARRAYSIZE(location):
            return HRESULT_FROM_WIN32(GetLastError());
    }

    enum Framework : DWORD {
        kFrameworkUWP = 1,
        kFrameworkWinUI,
    };

    switch (framework) {
        case kFrameworkUWP:
            return UwpInitializeXamlDiagnostics(pid, location);
    }

    return E_INVALIDARG;
}

BOOL WINAPI isDebugging(DWORD pid) {
    struct EnumWindowsData {
        DWORD pid;
        BOOL found;
    };

    EnumWindowsData data = {pid, FALSE};

    EnumWindows(
        [](HWND hWnd, LPARAM lParam) -> BOOL {
            EnumWindowsData& data = *reinterpret_cast<EnumWindowsData*>(lParam);

            DWORD windowPid;
            if (!GetWindowThreadProcessId(hWnd, &windowPid) ||
                windowPid != data.pid) {
                return TRUE;
            }

            WCHAR windowClass[16];
            if (!GetClassName(hWnd, windowClass, ARRAYSIZE(windowClass)) ||
                _wcsicmp(windowClass, L"UWPSpy") != 0) {
                return TRUE;
            }

            data.found = TRUE;
            return FALSE;
        },
        reinterpret_cast<LPARAM>(&data));

    return data.found;
}
