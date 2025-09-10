#include <windows.h>
#include <commctrl.h>
#include <thread>
#include <atomic>
#include <string>
#include <chrono>
#include <fstream>
#include <cstdio>

#pragma comment(lib, "comctl32.lib")

// IDs controls
enum {
    IDC_HOURS = 101,
    IDC_MINS,
    IDC_SECS,
    IDC_MS,
    IDC_BTN_START,
    IDC_BTN_STOP,
    IDC_BTN_HOTKEY,
    IDC_COMBO_BUTTON,
    IDC_COMBO_CLICTYPE,
    IDC_RAD_REPEAT,
    IDC_RAD_UNTILSTOP,
    IDC_REPEAT_TIMES,
    IDC_RAD_CURPOS,
    IDC_RAD_PICKPOS,
    IDC_XENTRY,
    IDC_YENTRY,
    IDC_STATUS
};

HWND hMainWnd = nullptr;
HINSTANCE g_hInst = nullptr;

// Helpers
HWND CreateEdit(int x, int y, int w, int h, int id) {
    return CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
        x, y, w, h, hMainWnd, (HMENU)id, g_hInst, NULL);
}
HWND CreateLabel(const char* txt, int x, int y, int w, int h) {
    return CreateWindowExA(0, "STATIC", txt, WS_CHILD | WS_VISIBLE,
        x, y, w, h, hMainWnd, NULL, g_hInst, NULL);
}
HWND CreateButton(const char* txt, int x, int y, int w, int h, int id) {
    return CreateWindowExA(0, "BUTTON", txt, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, w, h, hMainWnd, (HMENU)id, g_hInst, NULL);
}
HWND CreateRadio(const char* txt, int x, int y, int w, int h, int id, BOOL checked) {
    return CreateWindowExA(0, "BUTTON", txt, WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | (checked?WS_GROUP:0),
        x, y, w, h, hMainWnd, (HMENU)id, g_hInst, NULL);
}
HWND CreateCombo(int x, int y, int w, int h, int id) {
    return CreateWindowExA(0, WC_COMBOBOXA, "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        x, y, w, h, hMainWnd, (HMENU)id, g_hInst, NULL);
}

// AutoClicker logic
struct AutoClicker {
    std::atomic<bool> running{false};
    double interval_seconds = 0.0;
    int mouse_button = 0; // 0 left,1 right,2 middle
    bool double_click = false;
    bool use_fixed_pos = false;
    POINT fixed_pos = {0,0};
    bool repeat_enabled = false;
    int repeat_times = 0;

    std::thread worker;

    void start() {
        if (running) return;
        running = true;
        worker = std::thread([this]() {
            int count = 0;
            while (running) {
                if (use_fixed_pos) {
                    SetCursorPos(fixed_pos.x, fixed_pos.y);
                }
                DWORD downFlag = 0, upFlag = 0;
                if (mouse_button == 0) { downFlag = MOUSEEVENTF_LEFTDOWN; upFlag = MOUSEEVENTF_LEFTUP; }
                else if (mouse_button == 1) { downFlag = MOUSEEVENTF_RIGHTDOWN; upFlag = MOUSEEVENTF_RIGHTUP; }
                else { downFlag = MOUSEEVENTF_MIDDLEDOWN; upFlag = MOUSEEVENTF_MIDDLEUP; }

                mouse_event(downFlag, 0,0,0,0);
                mouse_event(upFlag, 0,0,0,0);
                if (double_click) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    mouse_event(downFlag, 0,0,0,0);
                    mouse_event(upFlag, 0,0,0,0);
                }

                count++;
                if (repeat_enabled && repeat_times > 0 && count >= repeat_times) {
                    running = false;
                    break;
                }

                if (interval_seconds <= 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                } else {
                    int ms = (int)(interval_seconds * 1000.0);
                    if (ms < 1) ms = 1;
                    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
                }
            }
        });
        worker.detach();
    }

    void stop() {
        running = false;
    }
} clicker;

// Hotkey handling
int g_hotkey_vk = VK_ADD; // default Numpad '+'
int HOTKEY_ID = 1;
const char* CONFIG_FILE = "config.ini";

#ifndef MAPVK_VK_TO_VSC
#define MAPVK_VK_TO_VSC 0
#endif

void SaveHotkey() {
    std::ofstream f(CONFIG_FILE);
    if (f.is_open()) f << g_hotkey_vk;
}
void LoadHotkey() {
    std::ifstream f(CONFIG_FILE);
    if (f.is_open()) {
        int vk;
        f >> vk;
        if (vk > 0 && vk < 256) g_hotkey_vk = vk;
    }
}

// Utils
int GetIntFromEdit(int id) {
    char buf[64] = {0};
    HWND h = GetDlgItem(hMainWnd, id);
    if (!h) return 0;
    GetWindowTextA(h, buf, sizeof(buf));
    return atoi(buf);
}
void SetStatus(const char* text) {
    HWND h = GetDlgItem(hMainWnd, IDC_STATUS);
    if (h) SetWindowTextA(h, text);
}
void RegisterMyHotkey() {
    UnregisterHotKey(hMainWnd, HOTKEY_ID);
    if (!RegisterHotKey(hMainWnd, HOTKEY_ID, 0, g_hotkey_vk)) {
        SetStatus("Failed to register hotkey");
    } else {
        char buf[128];
        sprintf(buf, "Hotkey registered (vk=%d)", g_hotkey_vk);
        SetStatus(buf);
    }
}
void UpdateSettingsFromUI() {
    int h = GetIntFromEdit(IDC_HOURS);
    int m = GetIntFromEdit(IDC_MINS);
    int s = GetIntFromEdit(IDC_SECS);
    int ms = GetIntFromEdit(IDC_MS);
    double total = h*3600.0 + m*60.0 + s + ms/1000.0;
    clicker.interval_seconds = total;

    HWND cbBtn = GetDlgItem(hMainWnd, IDC_COMBO_BUTTON);
    int selBtn = (int)SendMessageA(cbBtn, CB_GETCURSEL, 0, 0);
    clicker.mouse_button = selBtn;

    HWND cbClick = GetDlgItem(hMainWnd, IDC_COMBO_CLICTYPE);
    int selClick = (int)SendMessageA(cbClick, CB_GETCURSEL, 0, 0);
    clicker.double_click = (selClick == 1);

    LRESULT repChecked = SendMessageA(GetDlgItem(hMainWnd, IDC_RAD_REPEAT), BM_GETCHECK, 0, 0);
    if (repChecked == BST_CHECKED) {
        clicker.repeat_enabled = true;
        clicker.repeat_times = GetIntFromEdit(IDC_REPEAT_TIMES);
        if (clicker.repeat_times < 1) clicker.repeat_times = 1;
    } else {
        clicker.repeat_enabled = false;
        clicker.repeat_times = 0;
    }

    LRESULT pickPos = SendMessageA(GetDlgItem(hMainWnd, IDC_RAD_PICKPOS), BM_GETCHECK, 0, 0);
    if (pickPos == BST_CHECKED) {
        int x = GetIntFromEdit(IDC_XENTRY);
        int y = GetIntFromEdit(IDC_YENTRY);
        clicker.use_fixed_pos = true;
        clicker.fixed_pos.x = x;
        clicker.fixed_pos.y = y;
    } else {
        clicker.use_fixed_pos = false;
    }
}
void HotkeyCaptureThread() {
    SetStatus("Press a key to set as hotkey...");
    for (;;) {
        for (int vk = 8; vk <= 254; ++vk) {
            SHORT st = GetAsyncKeyState(vk);
            if (st & 0x8000) {
                g_hotkey_vk = vk;
                SaveHotkey();
                PostMessageA(hMainWnd, WM_USER + 100, 0, 0);
                return;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// Window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        hMainWnd = hwnd;

        // تحميل الأيقونة من الموارد
        HICON hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(101));
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        SendMessage(hwnd, WM_SETICON, ICON_BIG,   (LPARAM)hIcon);

        CreateLabel("Click interval", 10, 5, 200, 20);
        CreateLabel("hours", 55, 30, 40, 20);
        CreateLabel("mins", 155, 30, 40, 20);
        CreateLabel("secs", 255, 30, 40, 20);
        CreateLabel("milliseconds", 350, 30, 120, 20);

        CreateEdit(10, 30, 40, 22, IDC_HOURS);
        CreateEdit(110, 30, 40, 22, IDC_MINS);
        CreateEdit(210, 30, 40, 22, IDC_SECS);
        CreateEdit(310, 30, 40, 22, IDC_MS);

        CreateLabel("Click options", 10, 70, 200, 20);
        CreateLabel("Mouse button:", 10, 95, 90, 20);
        HWND cbBtn = CreateCombo(100, 95, 100, 100, IDC_COMBO_BUTTON);
        SendMessageA(cbBtn, CB_ADDSTRING, 0, (LPARAM)"left");
        SendMessageA(cbBtn, CB_ADDSTRING, 0, (LPARAM)"right");
        SendMessageA(cbBtn, CB_ADDSTRING, 0, (LPARAM)"middle");
        SendMessageA(cbBtn, CB_SETCURSEL, 0, 0);

        CreateLabel("Click type:", 250, 95, 80, 20);
        HWND cbClick = CreateCombo(330, 95, 100, 100, IDC_COMBO_CLICTYPE);
        SendMessageA(cbClick, CB_ADDSTRING, 0, (LPARAM)"single");
        SendMessageA(cbClick, CB_ADDSTRING, 0, (LPARAM)"double");
        SendMessageA(cbClick, CB_SETCURSEL, 0, 0);

        CreateLabel("Click repeat", 10, 140, 200, 20);
        CreateRadio("Repeat", 10, 165, 100, 20, IDC_RAD_REPEAT, TRUE);
        CreateEdit(80, 165, 50, 22, IDC_REPEAT_TIMES);
        CreateLabel("times", 135, 165, 40, 20);
        CreateRadio("Repeat until stopped", 200, 165, 200, 20, IDC_RAD_UNTILSTOP, FALSE);

        CreateLabel("Cursor position", 10, 200, 200, 20);
        CreateRadio("Current location", 10, 225, 140, 20, IDC_RAD_CURPOS, TRUE);
        CreateRadio("Pick location", 150, 225, 120, 20, IDC_RAD_PICKPOS, FALSE);
        CreateEdit(260, 225, 40, 22, IDC_XENTRY);
        CreateEdit(310, 225, 40, 22, IDC_YENTRY);

        CreateButton("Start (+)", 50, 280, 80, 30, IDC_BTN_START);
        CreateButton("Stop", 150, 280, 80, 30, IDC_BTN_STOP);
        CreateButton("Hotkey setting", 250, 280, 120, 30, IDC_BTN_HOTKEY);

        CreateLabel("Status:", 10, 320, 50, 20);
        CreateWindowExA(0, "STATIC", "Ready", WS_CHILD | WS_VISIBLE,
            70, 320, 350, 20, hMainWnd, (HMENU)IDC_STATUS, g_hInst, NULL);

        // default values
        SetWindowTextA(GetDlgItem(hMainWnd, IDC_HOURS), "0");
        SetWindowTextA(GetDlgItem(hMainWnd, IDC_MINS), "0");
        SetWindowTextA(GetDlgItem(hMainWnd, IDC_SECS), "0");
        SetWindowTextA(GetDlgItem(hMainWnd, IDC_MS), "200");
        SetWindowTextA(GetDlgItem(hMainWnd, IDC_REPEAT_TIMES), "0");
        SetWindowTextA(GetDlgItem(hMainWnd, IDC_XENTRY), "0");
        SetWindowTextA(GetDlgItem(hMainWnd, IDC_YENTRY), "0");

        // load saved hotkey if available
        LoadHotkey();
        RegisterMyHotkey();
        break;
    }
    case WM_ERASEBKGND: { // إصلاح الخلفية السوداء عند Minimize/Restore
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH br = GetSysColorBrush(COLOR_WINDOW);
        FillRect(hdc, &rc, br);
        return 1;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDC_BTN_START) {
            UpdateSettingsFromUI();
            clicker.start();
            SetStatus("Running");
        } else if (id == IDC_BTN_STOP) {
            clicker.stop();
            SetStatus("Stopped");
        } else if (id == IDC_BTN_HOTKEY) {
            std::thread t(HotkeyCaptureThread);
            t.detach();
        }
        break;
    }
    case WM_HOTKEY: {
        if ((int)wParam == HOTKEY_ID) {
            if (!clicker.running) {
                UpdateSettingsFromUI();
                clicker.start();
                SetStatus("Running (hotkey)");
            } else {
                clicker.stop();
                SetStatus("Stopped (hotkey)");
            }
        }
        break;
    }
    case WM_USER + 100: {
        RegisterMyHotkey();
        UINT sc = MapVirtualKeyA(g_hotkey_vk, MAPVK_VK_TO_VSC);
        char name[128] = {0};
        if (sc) {
            LONG lparam = (sc << 16);
            if (GetKeyNameTextA(lparam, name, (int)sizeof(name)) > 0) {
                SetStatus(name);
            } else {
                char buf[64];
                sprintf(buf, "Hotkey set (vk=%d)", g_hotkey_vk);
                SetStatus(buf);
            }
        }
        break;
    }
    case WM_DESTROY: {
        clicker.stop();
        UnregisterHotKey(hMainWnd, HOTKEY_ID);
        PostQuitMessage(0);
        break;
    }
    default:
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// WinMain
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    g_hInst = hInstance;
    const char CLASS_NAME[] = "OPAutoClickerClass";

    WNDCLASSA wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    hMainWnd = CreateWindowExA(0, CLASS_NAME, "Aboubakerkm Autoclicker",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 520, 400,
        NULL, NULL, hInstance, NULL);

    if (!hMainWnd) return 0;
    ShowWindow(hMainWnd, nCmdShow);
    UpdateWindow(hMainWnd);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return 0;
}
