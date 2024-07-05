#include <WS2tcpip.h>
#include <Windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <winuser.h>

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#pragma comment(lib, "ws2_32.lib")
#pragma pack(push, 1)

WSAData wData;
WORD ver = MAKEWORD(2, 2);

int main() {
    int wsOk = WSAStartup(ver, &wData);
    if (wsOk != 0) {
        std::cerr << "Error Initializing WinSock! Exiting" << std::endl;
        return -1;
    }

    int x1, y1, x2, y2, w, h;

    // Message protocol
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    // Bind IP address to port
    std::string ipAddress = "127.0.0.1";
    sockaddr_in sockin;
    sockin.sin_family = AF_INET;
    sockin.sin_port = htons(8081);
    inet_pton(AF_INET, ipAddress.c_str(), &sockin.sin_addr);

    std::cout << "Attempting to connect to master controller..." << std::endl;

    // Connect to server
    int connected = connect(sock, (sockaddr*)&sockin, sizeof(sockin)) != SOCKET_ERROR;
    if (!connected) {
        std::cerr << "Could not connect to server" << std::endl;
        return -1;
    }

    while (connected) {
        // get screen dimensions
        x1 = GetSystemMetrics(SM_XVIRTUALSCREEN);
        y1 = GetSystemMetrics(SM_XVIRTUALSCREEN);
        x2 = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        y2 = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        w = x2 - x1;
        h = y2 - y1;

        // copy screen to bitmap
        HDC hScreen = GetDC(NULL);
        HDC hdcCompat = CreateCompatibleDC(hScreen);
        HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, w, h);
        HGDIOBJ old_obj = SelectObject(hdcCompat, hBitmap);
        StretchBlt(hdcCompat, 0, 0, w, h, hScreen, x1, y1, w, h, SRCCOPY);

        BITMAP bmpScreen;
        GetObject(hBitmap, sizeof(BITMAP), &bmpScreen);

        BITMAPINFOHEADER bi;
        bi.biSize = sizeof(BITMAPINFOHEADER);
        bi.biWidth = w;
        bi.biHeight = h;
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = BI_RGB;
        bi.biSizeImage = 0;
        bi.biXPelsPerMeter = 0;
        bi.biYPelsPerMeter = 0;
        bi.biClrUsed = 0;
        bi.biClrImportant = 0;

        DWORD dwBmpSize = ((bmpScreen.bmWidth * bi.biBitCount + 31) / 32) * 4 * bmpScreen.bmHeight;

        HGLOBAL hDIB = GlobalAlloc(GHND, dwBmpSize);
        char* lpbitmap = (char*)GlobalLock(hDIB);

        // Gets the "bits" from the bitmap, and copies them into a buffer 
        // that's pointed to by lpbitmap.
        GetDIBits(hScreen, hBitmap, 0, (UINT)h, lpbitmap, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

        int headerSent = 0;
        if (headerSent == 0) {
            std::cout << "Sending BITMAPINFOHEADER..." << std::endl;
            if (send(sock, (char*)&bi, sizeof(bi), 0) == SOCKET_ERROR) {
                std::cerr << "Failed to send BITMAPINFOHEADER" << std::endl;
                break;
            }
        }
        

        std::cout << "Sending bitmap data..." << std::endl;
        if (send(sock, lpbitmap, dwBmpSize, 0) == SOCKET_ERROR) {
            std::cerr << "Failed to send bitmap data" << std::endl;
            break;
        }

        Sleep(1000);
        GlobalUnlock(hDIB);
        GlobalFree(hDIB);
        DeleteObject(hBitmap);
        DeleteDC(hdcCompat);
        ReleaseDC(NULL, hScreen);
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}