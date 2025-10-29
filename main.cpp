#include <windows.h>
#include <commctrl.h>
#include <psapi.h>
#include <gdiplus.h>
#include <tlhelp32.h>
#include <string>
#include <sstream>
#include <set>
#include <thread>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "comctl32.lib")

using namespace Gdiplus;

HWND hwndProgress, hwndText, hwndConsole;
ULONG_PTR gdiplusToken;
std::set<std::wstring> previousProcesses;

void GetMemoryInfo(DWORDLONG& totalPhys, DWORDLONG& availPhys) {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(memInfo);
    GlobalMemoryStatusEx(&memInfo);
    totalPhys = memInfo.ullTotalPhys;
    availPhys = memInfo.ullAvailPhys;
}

void UpdateUI() {
    DWORDLONG totalPhys, availPhys;
    GetMemoryInfo(totalPhys, availPhys);
    double used = (double)(totalPhys - availPhys) / totalPhys * 100.0;

    SendMessage(hwndProgress, PBM_SETPOS, (WPARAM)used, 0);

    std::wstringstream ss;
    ss.precision(2);
    ss << L"Memory usage: " << std::fixed << used << L"%";
    SetWindowText(hwndText, ss.str().c_str());
}

void MonitorProcesses() {
    previousProcesses.clear();
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(pe);
        if (Process32First(hSnap, &pe)) {
            do { previousProcesses.insert(pe.szExeFile); } while (Process32Next(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }

    while (true) {
        HANDLE hSnap2 = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        std::set<std::wstring> currentProcesses;
        if (hSnap2 != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32 pe;
            pe.dwSize = sizeof(pe);
            if (Process32First(hSnap2, &pe)) {
                do { currentProcesses.insert(pe.szExeFile); } while (Process32Next(hSnap2, &pe));
            }
            CloseHandle(hSnap2);
        }

        for (const auto& name : previousProcesses) {
            if (currentProcesses.find(name) == currentProcesses.end()) {
                std::wstringstream ss;
                ss << L"Stopped process: " << name << L"\r\n";
                int len = GetWindowTextLength(hwndConsole);
                SendMessage(hwndConsole, EM_SETSEL, len, len);
                SendMessage(hwndConsole, EM_REPLACESEL, FALSE, (LPARAM)ss.str().c_str());
            }
        }
        previousProcesses = currentProcesses;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

DWORD WINAPI RefreshThread(LPVOID) {
    while (true) {
        UpdateUI();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        hwndText = CreateWindow(L"STATIC", L"Initializing...",
            WS_CHILD | WS_VISIBLE | SS_CENTER, 20, 20, 360, 25, hwnd, NULL, NULL, NULL);
        SendMessage(hwndText, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

        hwndProgress = CreateWindowEx(0, PROGRESS_CLASS, NULL,
            WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 20, 50, 360, 25, hwnd, NULL, NULL, NULL);
        SendMessage(hwndProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

        hwndConsole = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            20, 90, 360, 200, hwnd, NULL, NULL, NULL);
        SendMessage(hwndConsole, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

        CreateThread(NULL, 0, RefreshThread, NULL, 0, NULL);
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)MonitorProcesses, NULL, 0, NULL);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_PROGRESS_CLASS };
    InitCommonControlsEx(&icex);

    const wchar_t CLASS_NAME[] = L"zSparksCleanerUI";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, CLASS_NAME, L"zSparks Cleaner",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 340, NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    GdiplusShutdown(gdiplusToken);
    return 0;
}
