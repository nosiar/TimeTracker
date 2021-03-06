﻿#include "stdafx.h"
#include "TimeTracker.h"
#include "Database.h"
#include <regex>
#include <Shellapi.h>

#define MAX_LOADSTRING 100
#define TIME_FORMAT "%Y-%m-%d %H:%M:%S"
#define WM_USER_SHELLICON WM_USER + 1

using namespace std;
using tstring = basic_string < TCHAR > ;

// Global Variables:
HINSTANCE hInst;
HWND hWndThis;
TCHAR szTitle[MAX_LOADSTRING];
TCHAR szWindowClass[MAX_LOADSTRING];

HWINEVENTHOOK hHook;
mutex g_mutex;
deque<tstring> program_list;
tstring this_process_name;
tstring foreground;
tstring chrome_tab;
tstring chrome_tab_changed;
chrono::system_clock::time_point g_start;

// Forward declarations of functions included in this code module:
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
void Hook();
void Unhook();
void CALLBACK WinEventProc(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);
void AddProcess(Database&, const char*, const char*, const char*, int);
void AddProcess(const tstring&, const tstring&, chrono::system_clock::time_point, chrono::system_clock::time_point);
tstring GetProcessName(HWND);
void balloon(HWND hWnd);

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    MSG msg;
    HACCEL hAccelTable;

    // Initialize global strings
    LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadString(hInstance, IDC_TIMETRACKER, szWindowClass, MAX_LOADSTRING);

    HANDLE hMutex = CreateMutex(NULL, TRUE, _T("NOSIAR_TIMETRACKER"));
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        HWND hWnd = FindWindow(szWindowClass, NULL);
        balloon(hWnd);
        return 1;
    }

    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_TIMETRACKER));

    Hook();

    // Main message loop:
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    Unhook();

    ReleaseMutex(hMutex);
    CloseHandle(hMutex);

    return (int)msg.wParam;
}

void Hook()
{
    if (hHook != 0) return;
    CoInitialize(NULL);
    hHook = SetWinEventHook(EVENT_OBJECT_FOCUS, EVENT_OBJECT_VALUECHANGE, 0, WinEventProc, 0, 0, WINEVENT_SKIPOWNPROCESS);
}

void Unhook()
{
    if (hHook == 0) return;
    UnhookWinEvent(hHook);
    CoUninitialize();
}

bool is_valid_url(tstring url)
{
    tstring s = _T(R"pyapya(((https?|ftp):\/\/)?(([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)|localhost|([a-zA-Z0-9\-]+\.)*[a-zA-Z0-9\-]+\.(com|net|org|info|biz|gov|name|wiki|edu|[a-zA-Z][a-zA-Z]))(:[0-9]+)?((\/|\?)[^ "]*[^ ,;\.:">)])?)pyapya");

    return regex_match(url, basic_regex<TCHAR>(s));
}

void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime)
{
    UNREFERENCED_PARAMETER(hWinEventHook);
    UNREFERENCED_PARAMETER(event);
    UNREFERENCED_PARAMETER(dwEventThread);
    UNREFERENCED_PARAMETER(dwmsEventTime);

    IAccessible* pAcc = NULL;
    VARIANT varChild;
    HRESULT hr = AccessibleObjectFromEvent(hwnd, idObject, idChild, &pAcc, &varChild);
    if ((hr == S_OK) && (pAcc != NULL)) {
        BSTR bstrName, bstrValue;
        pAcc->get_accValue(varChild, &bstrValue);
        pAcc->get_accName(varChild, &bstrName);

        if (!bstrName || !bstrValue) return;

        TCHAR szClassName[50];
        GetClassName(hwnd, szClassName, 50);

        if ((_tcscmp(szClassName, _T("Chrome_WidgetWin_1")) == 0) && (wcscmp(bstrName, L"Address and search bar") == 0 || wcscmp(bstrName, L"주소창 및 검색창") == 0)) {
            auto p = tstring(bstrValue);
            int n = p.find(_T("://"));
            if (n != -1)
                p = p.substr(n + 3);
            n = p.find(_T('/'));
            if (n != -1)
                p.resize(n);
            n = p.find(_T(':'));
            if (n != -1)
                p.resize(n);

            if (chrome_tab_changed != p && is_valid_url(p))
            {
                chrome_tab_changed = p;
            }
        }
        pAcc->Release();
    }
}

void Run()
{
    for (;;)
    {
        HWND hWnd = GetForegroundWindow();
        if (hWnd != NULL)
        {
            auto process_name = GetProcessName(hWnd);
            if (!process_name.empty() && process_name != this_process_name)
            {
                if (foreground != process_name)
                {
                    auto end = chrono::system_clock::now();

                    if (!foreground.empty())
                    {
                        if (foreground == _T("chrome.exe"))
                            AddProcess(foreground, chrome_tab, g_start, end);
                        else
                            AddProcess(foreground, _T(""), g_start, end);
                    }

                    foreground = process_name;
					g_start = end;
                }
                else if (foreground == _T("chrome.exe") && chrome_tab != chrome_tab_changed)
                {
                    auto end = chrono::system_clock::now();

                    AddProcess(foreground, chrome_tab, g_start, end);

                    chrome_tab = chrome_tab_changed;
					g_start = end;
                }
            }
        }
        this_thread::sleep_for(chrono::microseconds(50));
    }
}

std::wstring utf8_to_wstring(const std::string& str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.from_bytes(str);
}

std::string wstring_to_utf8(const std::wstring& str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.to_bytes(str);
}

// assume that the db opened
void AddProcess(Database& db, const char* name, const char* subname, const char* time, int duration)
{
    const Reader& reader = db.select(name, subname, time);
    if (reader.read())
        db.update(name, subname, time, duration + stoi(reader.get("DURATION")));
    else
        db.insert(name, subname, time, duration);
}

void AddProcess(const tstring& process_name, const tstring& subname, chrono::system_clock::time_point start, chrono::system_clock::time_point end)
{
    time_t start_t = chrono::system_clock::to_time_t(start);
    time_t end_t = chrono::system_clock::to_time_t(end);

    tm start_tm;
    tm end_tm;

    localtime_s(&start_tm, &start_t);
    localtime_s(&end_tm, &end_t);


    string p_ = wstring_to_utf8(process_name);
    const char* p = p_.c_str();

    string s_ = wstring_to_utf8(subname);
    const char* s = s_.c_str();

    char time[1024];

    if (start_tm.tm_year == end_tm.tm_year && start_tm.tm_mon == end_tm.tm_mon && start_tm.tm_mday == end_tm.tm_mday && start_tm.tm_hour == end_tm.tm_hour)
    {
        start_tm.tm_min = 0;
        start_tm.tm_sec = 0;
        strftime(time, sizeof(time), TIME_FORMAT, &start_tm);

        Database db;
        db.open();
        AddProcess(db, p, s, time, static_cast<int>(end_t - start_t));
        db.close();
    }
    else
    {
        tm start_before = start_tm;
        start_before.tm_min = 0;
        start_before.tm_sec = 0;

        tm start_next = start_tm;
        start_next.tm_hour++;
        start_next.tm_min = 0;
        start_next.tm_sec = 0;
        time_t start_next_t = mktime(&start_next);

        tm end_before = end_tm;
        end_before.tm_min = 0;
        end_before.tm_sec = 0;
        time_t end_before_t = mktime(&end_before);


        Database db;
        db.open();

        strftime(time, sizeof(time), TIME_FORMAT, &start_before);
        AddProcess(db, p, s, time, static_cast<int>(start_next_t - start_t));

        while (start_next_t != end_before_t)
        {
            strftime(time, sizeof(time), TIME_FORMAT, &start_next);
            AddProcess(db, p, s, time, 3600);

            start_next.tm_hour++;
            start_next_t = mktime(&start_next);
        }

        strftime(time, sizeof(time), TIME_FORMAT, &end_before);
        AddProcess(db, p, s, time, static_cast<int>(end_t - end_before_t));

        db.close();
    }
}

tstring GetProcessName(HWND hWnd)
{
    DWORD pid;
    GetWindowThreadProcessId(hWnd, &pid);
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (h != NULL)
    {
        TCHAR szFileName[1024];
        DWORD ret = GetProcessImageFileName(h, szFileName, sizeof(szFileName) / sizeof(TCHAR));

        CloseHandle(h);

        if (ret != 0)
        {
            tstring process_name = szFileName;
            return process_name.substr(process_name.rfind(_T('\\')) + 1);
        }
    }
    return L"";
}

void balloon(HWND hWnd)
{
    NOTIFYICONDATA nid;
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = IDI_TIMETRACKER;
    nid.uTimeout = 5000;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO;
    _tcscpy_s(nid.szInfoTitle, _T("Time Tracker Is Running"));
    _tcscpy_s(nid.szInfo, _T(" "));
    nid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_TIMETRACKER));

    Shell_NotifyIcon(NIM_MODIFY, &nid);
}

void minimize(bool value)
{
    NOTIFYICONDATA nid;
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWndThis;
    nid.uID = IDI_TIMETRACKER;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    _tcscpy_s(nid.szTip, szTitle);
    nid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_TIMETRACKER));
    nid.uCallbackMessage = WM_USER_SHELLICON;

    Shell_NotifyIcon(value ? NIM_ADD : NIM_DELETE, &nid);

    ShowWindow(hWndThis, !value);

    if (value)
        balloon(hWndThis);
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TIMETRACKER));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCE(IDC_TIMETRACKER);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassEx(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int /*nCmdShow*/)
{
   hInst = hInstance; // Store instance handle in our global variable

   hWndThis = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW&~WS_MAXIMIZEBOX,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

   if (!hWndThis)
   {
      return FALSE;
   }

   minimize(true);

   return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;
    PAINTSTRUCT ps;
    HDC hdc;

    switch (message)
    {
    case WM_CREATE:
        {
            g_start = chrono::system_clock::now();

            Database db;
            db.open();
            db.create_table();
            db.close();

            this_process_name = GetProcessName(hWnd);
            thread t(Run);
            t.detach();
            break;
        }
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_MINIMIZE)
        {
            minimize(true);
            break;
        }
        else
            return DefWindowProc(hWnd, message, wParam, lParam);
    case WM_USER_SHELLICON:
        switch (LOWORD(lParam))
        {
        case WM_LBUTTONDOWN:
            minimize(false);
            break;
        case WM_RBUTTONDOWN:
            POINT p;
            GetCursorPos(&p);
            HMENU hPopMenu = CreatePopupMenu();
            InsertMenu(hPopMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDM_ABOUT, _T("About"));
            InsertMenu(hPopMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, IDM_EXIT, _T("Exit"));
            SetForegroundWindow(hWnd);
            TrackPopupMenu(hPopMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_BOTTOMALIGN,
                p.x, p.y, 0, hWnd, NULL);
            break;
        }
        break;
    case WM_COMMAND:
        wmId = LOWORD(wParam);
        wmEvent = HIWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        // TODO: Add any drawing code here...
        EndPaint(hWnd, &ps);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
