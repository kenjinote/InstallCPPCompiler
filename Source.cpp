#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma comment(lib, "urlmon.lib")
#include <windows.h>
#include <urlmon.h>
#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include "resource.h"

TCHAR szClassName[] = TEXT("Window");

bool RunVsWhereAndGetVCPath(std::wstring& outPath) {
    wchar_t cmd[] = L"vswhere.exe -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath -latest";

    HANDLE hRead, hWrite;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return false;

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;

    PROCESS_INFORMATION pi;
    if (!CreateProcessW(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hWrite);
        CloseHandle(hRead);
        return false;
    }
    CloseHandle(hWrite);

    std::string utf8Result;
    char buffer[512]; DWORD bytesRead;
    while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead) {
        buffer[bytesRead] = '\0';
        utf8Result += buffer;
    }
    CloseHandle(hRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8Result.c_str(), -1, NULL, 0);
    if (wideLen == 0) return false;
    std::wstring wResult(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8Result.c_str(), -1, &wResult[0], wideLen);

    wResult.erase(std::remove(wResult.begin(), wResult.end(), L'\r'), wResult.end());
    wResult.erase(std::remove(wResult.begin(), wResult.end(), L'\n'), wResult.end());
    outPath = wResult;
    return !outPath.empty();
}

std::wstring GetFileNameFromUrl(const std::wstring& url) {
    size_t pos = url.find_last_of(L'/');
    return (pos == std::wstring::npos) ? url : url.substr(pos + 1);
}

bool DownloadFile(const wchar_t* url) {
    std::wstring filename = GetFileNameFromUrl(url);
    HRESULT hr = URLDownloadToFileW(nullptr, url, filename.c_str(), 0, nullptr);
    if (SUCCEEDED(hr)) {
        std::wcout << filename << L" downloaded successfully.\n";
        return true;
    }
    else {
        std::wcerr << L"Download failed: " << filename << L"\n";
        return false;
    }
}

bool InstallCppBuildTools() {
    wchar_t cmd[] = L"vs_buildtools.exe --quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools";
    STARTUPINFOW si = { sizeof(si) }; PROCESS_INFORMATION pi;
    if (!CreateProcessW(nullptr, cmd, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) return false;
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return exitCode == 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hBtn1, hBtn2, hBtn3, hEdit;
    switch (msg) {
    case WM_CREATE:
        hBtn1 = CreateWindow(TEXT("BUTTON"), TEXT("cl.exeの有無確認"), WS_VISIBLE | WS_CHILD, 0, 0, 0, 0, hWnd, (HMENU)1000, ((LPCREATESTRUCT)lParam)->hInstance, 0);
        hBtn2 = CreateWindow(TEXT("BUTTON"), TEXT("cl.exeをインストール"), WS_VISIBLE | WS_CHILD, 0, 0, 0, 0, hWnd, (HMENU)1001, ((LPCREATESTRUCT)lParam)->hInstance, 0);
        hBtn3 = CreateWindow(TEXT("BUTTON"), TEXT("cl.exeのパス取得"), WS_VISIBLE | WS_CHILD, 0, 0, 0, 0, hWnd, (HMENU)1002, ((LPCREATESTRUCT)lParam)->hInstance, 0);
        hEdit = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), 0, WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL, 0, 0, 0, 0, hWnd, 0, ((LPCREATESTRUCT)lParam)->hInstance, 0);
        break;
    case WM_SIZE:
        MoveWindow(hBtn1, 10, 10, 512, 32, TRUE);
        MoveWindow(hBtn2, 10, 50, 512, 32, TRUE);
        MoveWindow(hBtn3, 10, 90, 512, 32, TRUE);
        MoveWindow(hEdit, 10, 130, 512, 32, TRUE);
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case 1000:
        case 1002: {
            DownloadFile(L"https://github.com/microsoft/vswhere/releases/latest/download/vswhere.exe");
            std::wstring vcBasePath;
            if (RunVsWhereAndGetVCPath(vcBasePath)) {
                if (LOWORD(wParam) == 1000)
                    MessageBox(hWnd, L"cl.exeがインストールされています", L"確認", MB_OK);
                else
                    SetWindowText(hEdit, vcBasePath.c_str());
            }
            else {
                MessageBox(hWnd, L"cl.exeが見つかりません", L"確認", MB_OK);
            }
            DeleteFile(L"vswhere.exe");
            break;
        }
        case 1001:
            if (DownloadFile(L"https://aka.ms/vs/17/release/vs_buildtools.exe")) {
                if (!InstallCppBuildTools())
                    MessageBox(hWnd, L"インストールに失敗しました", L"エラー", MB_OK);
            }
            else {
                MessageBox(hWnd, L"ダウンロードに失敗しました", L"エラー", MB_OK);
            }
            DeleteFile(L"vs_buildtools.exe");
            break;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    WNDCLASS wc = { CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hInstance, 0, LoadCursor(0, IDC_ARROW), (HBRUSH)(COLOR_WINDOW + 1), 0, szClassName };
    RegisterClass(&wc);
    HWND hWnd = CreateWindow(szClassName, TEXT("Window"), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, 0, 0, hInstance, 0);
    ShowWindow(hWnd, SW_SHOWDEFAULT); UpdateWindow(hWnd);
    MSG msg;
    while (GetMessage(&msg, 0, 0, 0)) TranslateMessage(&msg), DispatchMessage(&msg);
    return (int)msg.wParam;
}