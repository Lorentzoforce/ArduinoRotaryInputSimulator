// ... 保留原有 #include 与宏定义
#include <windows.h>
#include <tchar.h>
#include <string>
#include <sstream>
#include <vector>
#include <thread>            // [ADDED]
#include <atomic>            // [ADDED]

#define WINDOW_CLASS_NAME L"ArduinoRotaryInputSimulator"
#define WINDOW_TITLE L"Arduino Rotary Input Simulator"

// 控件 ID、串口参数与全局变量保持不变...
// 窗口类名和标题
#define WINDOW_CLASS_NAME L"ArduinoRotaryInputSimulator"
#define WINDOW_TITLE L"Arduino Rotary Input Simulator"

// 控件 ID
#define IDC_COMBO_PORT 1001
#define IDC_BUTTON_REFRESH 1002
#define IDC_STATIC_STATUS 1003
#define IDC_BUTTON_CONNECT 1004
#define IDC_BUTTON_DISCONNECT 1005
#define IDC_CHECKBOX_MOUSE 1006
#define IDC_CHECKBOX_KEYBOARD 1007
#define IDC_BUTTON_CONFIRM 1008
#define IDC_STATIC_SCALE 1009
#define IDC_EDIT_SCALE 1010
#define IDC_BUTTON_CANCEL 1011
#define IDC_STATIC_INPUT_STATE 1012

// 串口参数
#define BAUD_RATE CBR_9600

// 全局变量
HWND g_hWnd; // 窗口句柄
HANDLE g_hSerial = INVALID_HANDLE_VALUE; // 串口句柄
std::wstring g_serialData = L"No data received"; // 最新串口数据
std::wstring g_inputStatus = L"No input simulated"; // 模拟输入状态
float g_currentAngle = 0.0f; // 当前角度
float g_lastAngle = 0.0f; // 上次角度
bool g_enableMouse = false; // 鼠标模拟启用标志
bool g_enableKeyboard = false; // 键盘模拟启用标志
float g_mouseScale = 1.0f; // 鼠标移动比例
int g_baseMouseX = 0; // [ADDED] 鼠标基准X坐标（用于Relative Mode）
HWND g_hComboPort, g_hStaticStatus; // 控件句柄
HWND hModeCombo;
HWND hCombo;
std::vector<std::wstring> g_availablePorts; // 可用端口列表
DWORD g_lastKeyTime = 0; // 上次按键时间
bool g_isKeyDown = false; // 按键是否持续按下

std::atomic<bool> g_threadRunning{ false }; // [ADDED] 控制串口读取线程
std::thread g_workerThread;                 // [ADDED] 后台工作线程
//新的鼠标模拟信号条件
int g_mouseInputMode = 0; // [ADDED] 0 = 差值模式，1 = 基准点模式
float g_baseAngle = 0.0f; // [ADDED] 基准角度（Relative Mode使用）
int sel;

// 所有前向声明保持不变...
// 前向声明
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void InitSerialPort(const std::wstring& port);
void ReadSerialPort();
void SimulateInput(float angleDelta);
void UpdateDisplay(HDC hdc);
void RefreshPorts();
bool ConnectPort();
void DisconnectPort();
void UpdateMouseScale();

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    // 注册窗口类...
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEXW), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hInstance, NULL, NULL, (HBRUSH)(COLOR_WINDOW + 1), NULL, WINDOW_CLASS_NAME, NULL };
    if (!RegisterClassExW(&wcex))
    {
        MessageBoxW(NULL, L"Window Registration Failed!", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 创建窗口...
    g_hWnd = CreateWindowW(WINDOW_CLASS_NAME, WINDOW_TITLE, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, 400, 450, NULL, NULL, hInstance, NULL);
    if (!g_hWnd)
    {
        MessageBoxW(NULL, L"Window Creation Failed!", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    // 删除原来的 SetTimer 调用
    // SetTimer(g_hWnd, 1, 50, NULL);  // [REMOVED]

    // 消息循环...
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        // [ADDED] 捕获快捷键 F1 和 F2，不让控件吃掉
        if (msg.message == WM_KEYDOWN) {
            if (msg.wParam == VK_F1) {
                SendMessageW(g_hWnd, WM_COMMAND, IDC_BUTTON_CONFIRM, 0);
                continue;
            }
            else if (msg.wParam == VK_F2) {
                SendMessageW(g_hWnd, WM_COMMAND, IDC_BUTTON_CANCEL, 0);
                continue;
            }
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 退出前线程清理
    g_threadRunning = false;
    if (g_workerThread.joinable())
        g_workerThread.join();  // [ADDED]

    if (g_hSerial != INVALID_HANDLE_VALUE) CloseHandle(g_hSerial);
    // KillTimer(g_hWnd, 1); // [REMOVED]

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        // 创建控件与初始端口刷新逻辑不变...
        // 创建控件
        g_hComboPort = CreateWindowW(L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            10, 10, 150, 200, hWnd, (HMENU)IDC_COMBO_PORT, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        CreateWindowW(L"BUTTON", L"Refresh", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            170, 10, 80, 25, hWnd, (HMENU)IDC_BUTTON_REFRESH, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        g_hStaticStatus = CreateWindowW(L"STATIC", L"Status: Not Connected", WS_CHILD | WS_VISIBLE | SS_LEFT,
            10, 40, 240, 20, hWnd, (HMENU)IDC_STATIC_STATUS, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        CreateWindowW(L"BUTTON", L"Connect", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 70, 80, 25, hWnd, (HMENU)IDC_BUTTON_CONNECT, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        CreateWindowW(L"BUTTON", L"Disconnect", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            100, 70, 80, 25, hWnd, (HMENU)IDC_BUTTON_DISCONNECT, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        CreateWindowW(L"BUTTON", L"Confirm Input", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 100, 100, 25, hWnd, (HMENU)IDC_BUTTON_CONFIRM, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        CreateWindowW(L"BUTTON", L"Cancel Input", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            120, 100, 100, 25, hWnd, (HMENU)IDC_BUTTON_CANCEL, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        CreateWindowW(L"BUTTON", L"Mouse", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            10, 130, 80, 25, hWnd, (HMENU)IDC_CHECKBOX_MOUSE, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        CreateWindowW(L"BUTTON", L"Keyboard", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            100, 130, 80, 25, hWnd, (HMENU)IDC_CHECKBOX_KEYBOARD, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        CreateWindowW(L"STATIC", L"Mouse Move Scale:", WS_CHILD | WS_VISIBLE | SS_LEFT,
            10, 160, 100, 20, hWnd, (HMENU)IDC_STATIC_SCALE, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        CreateWindowW(L"EDIT", L"1.0", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
            110, 160, 60, 20, hWnd, (HMENU)IDC_EDIT_SCALE, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        CreateWindowW(L"STATIC", L"Input State: Disabled", WS_CHILD | WS_VISIBLE | SS_LEFT,
            10, 220, 150, 20, hWnd, (HMENU)IDC_STATIC_INPUT_STATE, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);
        //加控件：下拉框与静态文本
        // [ADDED] Combo box for input mode
        hModeCombo = CreateWindowW(L"COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            140, 190, 200, 100, hWnd, (HMENU)2015, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE), NULL);

        // [ADDED] Add options
        SendMessageW(hModeCombo, CB_ADDSTRING, 0, (LPARAM)L"Delta Mode (angle change)");
        SendMessageW(hModeCombo, CB_ADDSTRING, 0, (LPARAM)L"Relative Mode (from base)");
        SendMessageW(hModeCombo, CB_SETCURSEL, 0, 0); // Default: Delta Mode

        RefreshPorts();
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_BUTTON_REFRESH:
            RefreshPorts();
            break;
        case IDC_BUTTON_CONNECT:
            if (ConnectPort()) {
                SetWindowTextW(g_hStaticStatus, (L"Status: Connected to " + g_availablePorts[SendMessageW(g_hComboPort, CB_GETCURSEL, 0, 0)]).c_str());

                // 启动串口读取线程 [ADDED]
                g_threadRunning = true;
                g_workerThread = std::thread([]() {
                    while (g_threadRunning) {
                        if (g_hSerial != INVALID_HANDLE_VALUE) {
                            ReadSerialPort();
                            float angleDelta = g_currentAngle - g_lastAngle;
                            g_lastAngle = g_currentAngle;
                            if (angleDelta != 0.0f)
                                SimulateInput(angleDelta);

                            // 更新显示
                            InvalidateRect(g_hWnd, NULL, TRUE);  // [MODIFIED] 仅触发 WM_PAINT，减少 flicker
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    }
                    });
            }
            break;
        case IDC_BUTTON_DISCONNECT:
            DisconnectPort();
            SetWindowTextW(g_hStaticStatus, L"Status: Not Connected");
            break;
        case IDC_BUTTON_CONFIRM:
            g_enableMouse = SendMessageW(GetDlgItem(hWnd, IDC_CHECKBOX_MOUSE), BM_GETCHECK, 0, 0) == BST_CHECKED;
            g_enableKeyboard = SendMessageW(GetDlgItem(hWnd, IDC_CHECKBOX_KEYBOARD), BM_GETCHECK, 0, 0) == BST_CHECKED;
            UpdateMouseScale();
            //记录基准角度：在 Confirm Input 时
            // [ADDED] Get selected input mode
            hCombo = GetDlgItem(hWnd, 2015);
            sel = SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
            if (sel != CB_ERR)
            {
                g_mouseInputMode = sel;
                if (sel == 1) // Relative Mode
                {

                    // [ADDED] 记录当前鼠标X坐标作为基准位置
                    POINT pt;
                    if (GetCursorPos(&pt))
                    {
                        g_baseMouseX = pt.x;
                    }
                }
            }
            SetWindowTextW(GetDlgItem(hWnd, IDC_STATIC_INPUT_STATE), L"Input State: Enabled");
            break;
        case IDC_BUTTON_CANCEL:
            g_enableMouse = false;
            g_enableKeyboard = false;
            SetWindowTextW(GetDlgItem(hWnd, IDC_STATIC_INPUT_STATE), L"Input State: Disabled");
            break;
        }
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        UpdateDisplay(hdc);
        EndPaint(hWnd, &ps);
        break;
    }
    case WM_KEYDOWN:  // [ADDED] 处理键盘快捷键
        if (wParam == VK_F1) {
            // 模拟 Confirm Input 按钮点击
            g_enableMouse = SendMessageW(GetDlgItem(hWnd, IDC_CHECKBOX_MOUSE), BM_GETCHECK, 0, 0) == BST_CHECKED;
            g_enableKeyboard = SendMessageW(GetDlgItem(hWnd, IDC_CHECKBOX_KEYBOARD), BM_GETCHECK, 0, 0) == BST_CHECKED;
            UpdateMouseScale();
            SetWindowTextW(GetDlgItem(hWnd, IDC_STATIC_INPUT_STATE), L"Input State: Enabled");
        }
        else if (wParam == VK_F2) {
            // 模拟 Cancel Input 按钮点击
            g_enableMouse = false;
            g_enableKeyboard = false;
            SetWindowTextW(GetDlgItem(hWnd, IDC_STATIC_INPUT_STATE), L"Input State: Disabled");
        }
        break;

    // 删除 WM_TIMER，因为现在由线程处理 [REMOVED]
    /*
    case WM_TIMER:
        if (g_hSerial != INVALID_HANDLE_VALUE)
        {
            ReadSerialPort();
            float angleDelta = g_currentAngle - g_lastAngle;
            g_lastAngle = g_currentAngle;
            if (angleDelta != 0.0f) SimulateInput(angleDelta);
        }
        InvalidateRect(hWnd, NULL, TRUE);
        break;
    */

    case WM_DESTROY:
        DisconnectPort();
        g_threadRunning = false;               // [ADDED]
        if (g_workerThread.joinable())
            g_workerThread.join();             // [ADDED]
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}
// 初始化串口
void InitSerialPort(const std::wstring& port)
{
    g_hSerial = CreateFileW((L"\\\\.\\" + port).c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (g_hSerial == INVALID_HANDLE_VALUE)
    {
        g_serialData = L"Failed to open COM port";
        InvalidateRect(g_hWnd, NULL, TRUE);
        return;
    }

    DCB dcb = { 0 };
    dcb.DCBlength = sizeof(dcb);
    GetCommState(g_hSerial, &dcb);
    dcb.BaudRate = BAUD_RATE;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    SetCommState(g_hSerial, &dcb);

    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    SetCommTimeouts(g_hSerial, &timeouts);
}

// 读取串口数据
void ReadSerialPort()
{
    char buffer[256] = { 0 };
    DWORD bytesRead;
    if (ReadFile(g_hSerial, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0)
    {
        buffer[bytesRead] = '\0';
        std::string data(buffer);

        // 查找最新的 "angle=" 数据
        size_t anglePos = data.find("angle=");
        if (anglePos != std::string::npos)
        {
            try
            {
                std::string angleStr = data.substr(anglePos + 6); // 提取角度值
                size_t endPos = angleStr.find_first_of("\r\n"); // 找到第一个换行符或结束
                if (endPos != std::string::npos)
                    angleStr = angleStr.substr(0, endPos); // 截取到换行符前
                g_currentAngle = std::stof(angleStr);
                std::wstringstream wss;
                wss << L"Received: angle=" << std::wstring(angleStr.begin(), angleStr.end());
                g_serialData = wss.str();
            }
            catch (...)
            {
                // 保留现有数据，不更新
            }
        }
    }
    // 如果 ReadFile 失败或无数据，保留现有 g_serialData 和 g_currentAngle
}

// 模拟输入
void SimulateInput(float angleDelta)
{
    DWORD currentTime = GetTickCount();
    std::wstringstream status;

    if (g_enableMouse)
    {
        // 鼠标移动，根据角度变化乘以比例
        //int mouseMove = (int)(angleDelta * g_mouseScale); // 正为右移，负为左移(旧的单一方法)
        float delta = 0.0f;
        if (g_mouseInputMode == 0) // Delta Mode
        {
            delta = angleDelta;
            int mouseMove = (int)(delta * g_mouseScale);

            INPUT input = { 0 };
            input.type = INPUT_MOUSE;
            input.mi.dx = mouseMove;
            input.mi.dy = 0;
            input.mi.dwFlags = MOUSEEVENTF_MOVE;
            SendInput(1, &input, sizeof(INPUT));

            status << L"Mouse: Moved " << (mouseMove > 0 ? L"right " : L"left ") << abs(mouseMove) << L"px";
        }
        else if (g_mouseInputMode == 1) // Relative Mode
        {
            delta = g_currentAngle - g_baseAngle;

            int targetX = g_baseMouseX + (int)(delta * g_mouseScale);

            // 获取当前Y坐标，保持垂直方向不变
            POINT pt;
            GetCursorPos(&pt);

            SetCursorPos(targetX, pt.y); // 设置鼠标到计算后的位置

            status << L"Mouse: Jumped to X=" << targetX;
        }
    }

    if (g_enableKeyboard)
    {
        // 键盘输入，根据角度变化
        if (abs(angleDelta) >= 150.0f)
        {
            if (!g_isKeyDown)
            {
                INPUT input = { 0 };
                input.type = INPUT_KEYBOARD;
                input.ki.wVk = (angleDelta > 0) ? 'D' : 'A';
                SendInput(1, &input, sizeof(INPUT));
                g_isKeyDown = true;
            }
            status << L", Keyboard: Holding '" << ((angleDelta > 0) ? L"D" : L"A") << L"'";
        }
        else if (angleDelta != 0.0f)
        {
            if (g_isKeyDown)
            {
                INPUT input = { 0 };
                input.type = INPUT_KEYBOARD;
                input.ki.wVk = (g_lastAngle > g_currentAngle) ? 'A' : 'D';
                input.ki.dwFlags = KEYEVENTF_KEYUP;
                SendInput(1, &input, sizeof(INPUT));
                g_isKeyDown = false;
            }

            int interval = (int)(1000 - (1000 - 50) * (abs(angleDelta) / 150.0f));
            if (currentTime - g_lastKeyTime >= interval)
            {
                INPUT inputs[2] = { 0 };
                inputs[0].type = INPUT_KEYBOARD;
                inputs[0].ki.wVk = (angleDelta > 0) ? 'D' : 'A';
                inputs[1].type = INPUT_KEYBOARD;
                inputs[1].ki.wVk = (angleDelta > 0) ? 'D' : 'A';
                inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
                SendInput(2, inputs, sizeof(INPUT));
                g_lastKeyTime = currentTime;
                status << L", Keyboard: Pressed '" << ((angleDelta > 0) ? L"D" : L"A") << L"' (Interval: " << interval << L"ms)";
            }
            else
            {
                status << L", Keyboard: Waiting (Interval: " << interval << L"ms)";
            }
        }
        else if (g_isKeyDown)
        {
            INPUT input = { 0 };
            input.type = INPUT_KEYBOARD;
            input.ki.wVk = (g_lastAngle > g_currentAngle) ? 'A' : 'D';
            input.ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(1, &input, sizeof(INPUT));
            g_isKeyDown = false;
        }
    }

    if (!g_enableMouse && !g_enableKeyboard)
        status << L"No input";

    g_inputStatus = status.str();
}

// 更新窗口显示
void UpdateDisplay(HDC hdc)
{
    RECT rect;
    GetClientRect(g_hWnd, &rect);
    TextOutW(hdc, 10, 260, g_serialData.c_str(), (int)g_serialData.length()); // 下移到 230
    TextOutW(hdc, 10, 280, g_inputStatus.c_str(), (int)g_inputStatus.length()); // 下移到 250
}

// 刷新可用端口
void RefreshPorts()
{
    SendMessageW(g_hComboPort, CB_RESETCONTENT, 0, 0);
    g_availablePorts.clear();

    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        wchar_t valueName[256];
        BYTE data[256];
        DWORD valueNameSize, dataSize, index = 0;

        while (RegEnumValueW(hKey, index++, valueName, &(valueNameSize = 256), NULL, NULL, data, &(dataSize = 256)) == ERROR_SUCCESS)
        {
            std::wstring port((wchar_t*)data, dataSize / sizeof(wchar_t));
            g_availablePorts.push_back(port);
            SendMessageW(g_hComboPort, CB_ADDSTRING, 0, (LPARAM)port.c_str());
        }
        RegCloseKey(hKey);
    }

    if (!g_availablePorts.empty())
        SendMessageW(g_hComboPort, CB_SETCURSEL, 0, 0);
    else
        SendMessageW(g_hComboPort, CB_ADDSTRING, 0, (LPARAM)L"No ports found");
}

// 建立连接
bool ConnectPort()
{
    if (g_hSerial != INVALID_HANDLE_VALUE)
        DisconnectPort();

    int selected = SendMessageW(g_hComboPort, CB_GETCURSEL, 0, 0);
    if (selected != CB_ERR)
    {
        InitSerialPort(g_availablePorts[selected]);
        if (g_hSerial != INVALID_HANDLE_VALUE)
            return true;
        else
            MessageBoxW(g_hWnd, L"Connection failed! Check port or physical connection.", L"Error", MB_OK | MB_ICONERROR);
    }
    return false;
}


void DisconnectPort()
{
    if (g_hSerial != INVALID_HANDLE_VALUE)
    {
        CloseHandle(g_hSerial);
        g_hSerial = INVALID_HANDLE_VALUE;
        g_serialData = L"No data received";
        g_currentAngle = 0.0f;
        g_lastAngle = 0.0f;
        g_enableMouse = false;
        g_enableKeyboard = false;
        SetWindowTextW(GetDlgItem(g_hWnd, IDC_STATIC_INPUT_STATE), L"Input State: Disabled");
        InvalidateRect(g_hWnd, NULL, TRUE);
    }

    // [ADDED] 停止线程（多次调用 DisconnectPort 安全处理）
    g_threadRunning = false;
    if (g_workerThread.joinable())
        g_workerThread.join();
}

// 更新鼠标移动比例
void UpdateMouseScale()
{
    HWND hEdit = GetDlgItem(g_hWnd, IDC_EDIT_SCALE);
    wchar_t buffer[16];
    GetWindowTextW(hEdit, buffer, 16);
    float scale = 1.0f;
    if (wcslen(buffer) > 0)
    {
        try
        {
            scale = std::stof(std::wstring(buffer).c_str());
            if (scale <= 0) scale = 1.0f;
        }
        catch (...) { scale = 1.0f; }
    }
    g_mouseScale = scale;
    SetWindowTextW(hEdit, std::to_wstring(g_mouseScale).c_str());
}
