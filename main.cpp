// ... ����ԭ�� #include ��궨��
#include <windows.h>
#include <tchar.h>
#include <string>
#include <sstream>
#include <vector>
#include <thread>            // [ADDED]
#include <atomic>            // [ADDED]

#define WINDOW_CLASS_NAME L"ArduinoRotaryInputSimulator"
#define WINDOW_TITLE L"Arduino Rotary Input Simulator"

// �ؼ� ID�����ڲ�����ȫ�ֱ������ֲ���...
// ���������ͱ���
#define WINDOW_CLASS_NAME L"ArduinoRotaryInputSimulator"
#define WINDOW_TITLE L"Arduino Rotary Input Simulator"

// �ؼ� ID
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

// ���ڲ���
#define BAUD_RATE CBR_9600

// ȫ�ֱ���
HWND g_hWnd; // ���ھ��
HANDLE g_hSerial = INVALID_HANDLE_VALUE; // ���ھ��
std::wstring g_serialData = L"No data received"; // ���´�������
std::wstring g_inputStatus = L"No input simulated"; // ģ������״̬
float g_currentAngle = 0.0f; // ��ǰ�Ƕ�
float g_lastAngle = 0.0f; // �ϴνǶ�
bool g_enableMouse = false; // ���ģ�����ñ�־
bool g_enableKeyboard = false; // ����ģ�����ñ�־
float g_mouseScale = 1.0f; // ����ƶ�����
int g_baseMouseX = 0; // [ADDED] ����׼X���꣨����Relative Mode��
HWND g_hComboPort, g_hStaticStatus; // �ؼ����
HWND hModeCombo;
HWND hCombo;
std::vector<std::wstring> g_availablePorts; // ���ö˿��б�
DWORD g_lastKeyTime = 0; // �ϴΰ���ʱ��
bool g_isKeyDown = false; // �����Ƿ��������

std::atomic<bool> g_threadRunning{ false }; // [ADDED] ���ƴ��ڶ�ȡ�߳�
std::thread g_workerThread;                 // [ADDED] ��̨�����߳�
//�µ����ģ���ź�����
int g_mouseInputMode = 0; // [ADDED] 0 = ��ֵģʽ��1 = ��׼��ģʽ
float g_baseAngle = 0.0f; // [ADDED] ��׼�Ƕȣ�Relative Modeʹ�ã�
int sel;

// ����ǰ���������ֲ���...
// ǰ������
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
    // ע�ᴰ����...
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEXW), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hInstance, NULL, NULL, (HBRUSH)(COLOR_WINDOW + 1), NULL, WINDOW_CLASS_NAME, NULL };
    if (!RegisterClassExW(&wcex))
    {
        MessageBoxW(NULL, L"Window Registration Failed!", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // ��������...
    g_hWnd = CreateWindowW(WINDOW_CLASS_NAME, WINDOW_TITLE, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, 400, 450, NULL, NULL, hInstance, NULL);
    if (!g_hWnd)
    {
        MessageBoxW(NULL, L"Window Creation Failed!", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    // ɾ��ԭ���� SetTimer ����
    // SetTimer(g_hWnd, 1, 50, NULL);  // [REMOVED]

    // ��Ϣѭ��...
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        // [ADDED] �����ݼ� F1 �� F2�����ÿؼ��Ե�
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

    // �˳�ǰ�߳�����
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
        // �����ؼ����ʼ�˿�ˢ���߼�����...
        // �����ؼ�
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
        //�ӿؼ����������뾲̬�ı�
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

                // �������ڶ�ȡ�߳� [ADDED]
                g_threadRunning = true;
                g_workerThread = std::thread([]() {
                    while (g_threadRunning) {
                        if (g_hSerial != INVALID_HANDLE_VALUE) {
                            ReadSerialPort();
                            float angleDelta = g_currentAngle - g_lastAngle;
                            g_lastAngle = g_currentAngle;
                            if (angleDelta != 0.0f)
                                SimulateInput(angleDelta);

                            // ������ʾ
                            InvalidateRect(g_hWnd, NULL, TRUE);  // [MODIFIED] ������ WM_PAINT������ flicker
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
            //��¼��׼�Ƕȣ��� Confirm Input ʱ
            // [ADDED] Get selected input mode
            hCombo = GetDlgItem(hWnd, 2015);
            sel = SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
            if (sel != CB_ERR)
            {
                g_mouseInputMode = sel;
                if (sel == 1) // Relative Mode
                {

                    // [ADDED] ��¼��ǰ���X������Ϊ��׼λ��
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
    case WM_KEYDOWN:  // [ADDED] ������̿�ݼ�
        if (wParam == VK_F1) {
            // ģ�� Confirm Input ��ť���
            g_enableMouse = SendMessageW(GetDlgItem(hWnd, IDC_CHECKBOX_MOUSE), BM_GETCHECK, 0, 0) == BST_CHECKED;
            g_enableKeyboard = SendMessageW(GetDlgItem(hWnd, IDC_CHECKBOX_KEYBOARD), BM_GETCHECK, 0, 0) == BST_CHECKED;
            UpdateMouseScale();
            SetWindowTextW(GetDlgItem(hWnd, IDC_STATIC_INPUT_STATE), L"Input State: Enabled");
        }
        else if (wParam == VK_F2) {
            // ģ�� Cancel Input ��ť���
            g_enableMouse = false;
            g_enableKeyboard = false;
            SetWindowTextW(GetDlgItem(hWnd, IDC_STATIC_INPUT_STATE), L"Input State: Disabled");
        }
        break;

    // ɾ�� WM_TIMER����Ϊ�������̴߳��� [REMOVED]
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
// ��ʼ������
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

// ��ȡ��������
void ReadSerialPort()
{
    char buffer[256] = { 0 };
    DWORD bytesRead;
    if (ReadFile(g_hSerial, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0)
    {
        buffer[bytesRead] = '\0';
        std::string data(buffer);

        // �������µ� "angle=" ����
        size_t anglePos = data.find("angle=");
        if (anglePos != std::string::npos)
        {
            try
            {
                std::string angleStr = data.substr(anglePos + 6); // ��ȡ�Ƕ�ֵ
                size_t endPos = angleStr.find_first_of("\r\n"); // �ҵ���һ�����з������
                if (endPos != std::string::npos)
                    angleStr = angleStr.substr(0, endPos); // ��ȡ�����з�ǰ
                g_currentAngle = std::stof(angleStr);
                std::wstringstream wss;
                wss << L"Received: angle=" << std::wstring(angleStr.begin(), angleStr.end());
                g_serialData = wss.str();
            }
            catch (...)
            {
                // �����������ݣ�������
            }
        }
    }
    // ��� ReadFile ʧ�ܻ������ݣ��������� g_serialData �� g_currentAngle
}

// ģ������
void SimulateInput(float angleDelta)
{
    DWORD currentTime = GetTickCount();
    std::wstringstream status;

    if (g_enableMouse)
    {
        // ����ƶ������ݽǶȱ仯���Ա���
        //int mouseMove = (int)(angleDelta * g_mouseScale); // ��Ϊ���ƣ���Ϊ����(�ɵĵ�һ����)
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

            // ��ȡ��ǰY���꣬���ִ�ֱ���򲻱�
            POINT pt;
            GetCursorPos(&pt);

            SetCursorPos(targetX, pt.y); // ������굽������λ��

            status << L"Mouse: Jumped to X=" << targetX;
        }
    }

    if (g_enableKeyboard)
    {
        // �������룬���ݽǶȱ仯
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

// ���´�����ʾ
void UpdateDisplay(HDC hdc)
{
    RECT rect;
    GetClientRect(g_hWnd, &rect);
    TextOutW(hdc, 10, 260, g_serialData.c_str(), (int)g_serialData.length()); // ���Ƶ� 230
    TextOutW(hdc, 10, 280, g_inputStatus.c_str(), (int)g_inputStatus.length()); // ���Ƶ� 250
}

// ˢ�¿��ö˿�
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

// ��������
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

    // [ADDED] ֹͣ�̣߳���ε��� DisconnectPort ��ȫ����
    g_threadRunning = false;
    if (g_workerThread.joinable())
        g_workerThread.join();
}

// ��������ƶ�����
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
