#define _WINSOCKAPI_
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <gdiplus.h>
#include <vector>
#include <string>
#include <map>
#include <thread>
#include <sstream>
#include <algorithm>
#include "LetterGen.h"
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

// Глобальні змінні:
HINSTANCE hInst;
LPCTSTR szWindowClass = TEXT("SERVER_APP");
LPCTSTR szTitle = TEXT("Server");
HWND hwndStatus;
HWND hwndGraph;
std::map<char, int> letterFrequency;
std::wstring connectionStatus = L"Disconnected";

// Прототипи функцій:
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void StartServer();
void DrawGraph(HDC hdc);
void UpdateStatus(HWND hwnd, const std::wstring& status);

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    if (!MyRegisterClass(hInstance)) {
        return FALSE;
    }

    if (!InitInstance(hInstance, nCmdShow)) {
        return FALSE;
    }

    std::thread serverThread(StartServer);
    serverThread.detach();

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    GdiplusShutdown(gdiplusToken);
    return static_cast<int>(msg.wParam);
}

ATOM MyRegisterClass(HINSTANCE hInstance) {
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    return RegisterClassEx(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {
    HWND hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 800, 600, NULL, NULL, hInstance, NULL);

    if (!hWnd) {
        return FALSE;
    }

    hInst = hInstance;
    hwndStatus = CreateWindow(TEXT("STATIC"), TEXT("Disconnected"), WS_VISIBLE | WS_CHILD,
        10, 10, 300, 20, hWnd, NULL, hInstance, NULL);
    hwndGraph = CreateWindow(TEXT("STATIC"), NULL, WS_VISIBLE | WS_CHILD | WS_BORDER,
        10, 40, 760, 500, hWnd, NULL, hInstance, NULL);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return TRUE;
}

void StartServer() {
    WSADATA wsaData;
    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;
    sockaddr_in serverAddr;
    int port = 27015;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        MessageBox(NULL, TEXT("WSAStartup failed"), TEXT("Error"), MB_OK);
        return;
    }

    ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ListenSocket == INVALID_SOCKET) {
        MessageBox(NULL, TEXT("Socket creation failed"), TEXT("Error"), MB_OK);
        WSACleanup();
        return;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(ListenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        MessageBox(NULL, TEXT("Bind failed"), TEXT("Error"), MB_OK);
        closesocket(ListenSocket);
        WSACleanup();
        return;
    }

    if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
        MessageBox(NULL, TEXT("Listen failed"), TEXT("Error"), MB_OK);
        closesocket(ListenSocket);
        WSACleanup();
        return;
    }

    while (true) {
        ClientSocket = accept(ListenSocket, NULL, NULL);
        if (ClientSocket == INVALID_SOCKET) {
            MessageBox(NULL, TEXT("Accept failed"), TEXT("Error"), MB_OK);
            closesocket(ListenSocket);
            WSACleanup();
            return;
        }

        UpdateStatus(hwndStatus, L"Connected");

        char recvbuf[1000];
        int recvbuflen = sizeof(recvbuf);
        int bytesReceived = recv(ClientSocket, recvbuf, recvbuflen, 0);
        if (bytesReceived > 0) {
            std::string receivedData(recvbuf, bytesReceived);
            CalculateFrequency(receivedData, letterFrequency);
            InvalidateRect(hwndGraph, NULL, TRUE);
        }

        closesocket(ClientSocket);
        UpdateStatus(hwndStatus, L"Disconnected");
    }

    closesocket(ListenSocket);
    WSACleanup();
}

void DrawGraph(HDC hdc) {
    Graphics graphics(hdc);
    Pen pen(Color(255, 0, 0, 0));
    SolidBrush brush(Color(255, 255, 0, 0));
    Font font(L"Arial", 10);
    SolidBrush textBrush(Color(255, 0, 0, 0));

    int x = 10;
    int y = 450;
    int barWidth = 20;
    int maxBarHeight = 400;
    int maxValue = 0;

    for (const auto& pair : letterFrequency) {
        if (pair.second > maxValue) {
            maxValue = pair.second;
        }
    }

    for (const auto& pair : letterFrequency) {
        int barHeight = static_cast<int>((static_cast<double>(pair.second) / maxValue) * maxBarHeight);
        graphics.FillRectangle(&brush, x, y - barHeight, barWidth, barHeight);
        std::wstring letter(1, pair.first);
        graphics.DrawString(letter.c_str(), -1, &font, PointF(x, y + 5), &textBrush);
        x += barWidth + 5;
    }
}

void UpdateStatus(HWND hwnd, const std::wstring& status) {
    connectionStatus = status;
    SetWindowText(hwndStatus, connectionStatus.c_str());
    InvalidateRect(hwndStatus, NULL, TRUE);
    UpdateWindow(hwndStatus);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwndGraph, &ps);
        DrawGraph(hdc);
        EndPaint(hwndGraph, &ps);
    }
                 break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
