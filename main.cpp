#ifndef main
#define main
#endif

#include <windows.h>
#include <d2d1.h>
#include <iostream>
#include <string>
#pragma comment(lib, "d2d1")

#include "comdef.h"
#include "basewin.h"
#include "shlwapi.h"

template <class T> void SafeRelease(T** ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

HANDLE ppmhandle;

class MainWindow : public BaseWindow<MainWindow>
{
    ID2D1Factory* pFactory;
    ID2D1HwndRenderTarget* pRenderTarget;
    ID2D1Bitmap                  *hBitmap;

    HRESULT CreateGraphicsResources();
    void    DiscardGraphicsResources();
    void    OnPaint();
    void    Resize();

public:

    MainWindow() : pFactory(NULL), pRenderTarget(NULL)//, pBrush(NULL)
    {
    }
    int sizeX = 200, sizeY = 200;
    HRESULT CreateBitmapFromHandle(HANDLE ppmHandle, int x, int ys);
    PCWSTR  ClassName() const { return L"Circle Window Class"; }
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

HRESULT MainWindow::CreateGraphicsResources()
{
    HRESULT hr = S_OK;
    if (pRenderTarget == NULL)
    {
        RECT rc;
        GetClientRect(m_hwnd, &rc);

        D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

        hr = pFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(m_hwnd, size),
            &pRenderTarget);
    }
    return hr;
}

void MainWindow::DiscardGraphicsResources()
{
    SafeRelease(&pRenderTarget);
}

void MainWindow::OnPaint()
{
    HRESULT hr = CreateGraphicsResources();
    if (SUCCEEDED(hr))
    {
        PAINTSTRUCT ps;
        BeginPaint(m_hwnd, &ps);

        pRenderTarget->BeginDraw();

        pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));

        if (hBitmap == NULL) {
            hr = CreateBitmapFromHandle(ppmhandle, sizeX, sizeY);
            if (FAILED(hr)) return;
        }
        pRenderTarget->DrawBitmap(
            hBitmap,
            D2D1::RectF(0, 0, sizeX, sizeY),
            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR
        );

        hr = pRenderTarget->EndDraw();
        if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
        {
            DiscardGraphicsResources();
        }
        EndPaint(m_hwnd, &ps);
    }
}

void MainWindow::Resize()
{
    if (pRenderTarget != NULL)
    {
        RECT rc;
        GetClientRect(m_hwnd, &rc);

        D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);
        char str[256];
        sprintf_s(str, sizeof(str), "right: %d bottom: %d\n",rc.right, rc.bottom);

        pRenderTarget->Resize(size);
        //CalculateLayout();
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

inline boolean isDig(char inp) {
    return inp >= '0' || inp <= '9';
}

HRESULT MainWindow::CreateBitmapFromHandle(HANDLE ppmHandle, int width, int height)
{
    HRESULT hr = S_OK;

    UINT8* data = (UINT8*)malloc(height * width * 4);
    
    char readBuffer[26] = { 0 };
    DWORD bytesRead = 0;
    std::string interim = "000";
    for (UINT y = 0; y < height; y++) { //height
        for (UINT x = 0; x < width; x++) { //width
            UINT8* pixelData = data + ((y * width) + x) * 4;
            std::string line = "";
            char bt = 'x' ;
            while (bt != '\n') {
                ReadFile(ppmHandle, static_cast<LPVOID>(&bt), 1, &bytesRead, NULL);
                if(bt != '\0') line += bt;
            }

            std::string curr = ""; int nums = 0;
            for (int i = 0; i < line.size(); i++) {
                if (isdigit(line[i])) {
                    curr += line[i];
                    continue;
                }
                if (isDig(line[i]) == isDig(line[i+1])) {
                    pixelData[nums] = atoi(curr.c_str());
                    curr.clear();
                    nums++;
                    if (nums == 3) {
                        nums = 0;
                        break;
                    }
                }
            }
            pixelData[3] = 255;
        }
    }
    hr = pRenderTarget->CreateBitmap(
        D2D1::SizeU(width, height),
        (const void*)data,
        width * 4,
        D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)),
        &hBitmap
    );
    return hr;
}

bool isUTF8(HANDLE ppmhandle) {
    std::string line = "";
    char bt = 'x';
    DWORD bytesRead;
    while (bt != '\n') {
        ReadFile(ppmhandle, static_cast<LPVOID>(&bt), 1, &bytesRead, NULL);
        if (bt != '\0') line += bt;
    }
    return line.size() == 3;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow)
{
    MainWindow win;
    LPWSTR* args;
    int nArgs;
    args = CommandLineToArgvW(pCmdLine, &nArgs);

    if (args == NULL)
        return -9;

    char fileLoc[256] = { 0 };
    HRESULT hr = GetFullPathName(args[0], 256, (LPWSTR)fileLoc, NULL);
    DWORD bytesRead;
    
    if(FAILED(hr)) exit(-10);

    ppmhandle = CreateFile(
        (LPCWSTR)fileLoc,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (ppmhandle == INVALID_HANDLE_VALUE) {
        
        CloseHandle(ppmhandle);
        return 0;
    }

    char readBuffer[256] = { 0 };
    BOOL bRead = ReadFile(
        ppmhandle,
        static_cast<LPVOID>(&readBuffer),
        9,
        &bytesRead,
        NULL
    );

    if (bRead == 0) {
        DWORD err = GetLastError();
        wchar_t* msg;
        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            err,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR)&msg,
            7,
            NULL
        );
        MessageBoxW(NULL, msg, L"DisplayError", MB_OK);
        CloseHandle(ppmhandle);
        return 0;
    }

    std::string line = "";
    char bt = 'x';
    while (bt != '\n') {
        ReadFile(ppmhandle, static_cast<LPVOID>(&bt), 1, &bytesRead, NULL);
        if (bt != '\0') line += bt;
    }

    for (int i = 0; i < line.size(); i++) {
        if (line[i] == '0') OutputDebugStringA("0");
        if (line[i] == '1') OutputDebugStringA("1");
        if (line[i] == '2') OutputDebugStringA("2");
        if (line[i] == '3') OutputDebugStringA("3");
        if (line[i] == '4') OutputDebugStringA("4");
        if (line[i] == '5') OutputDebugStringA("5");
        if (line[i] == '6') OutputDebugStringA("6");
        if (line[i] == '7') OutputDebugStringA("7");
        if (line[i] == '8') OutputDebugStringA("8");
        if (line[i] == '9') OutputDebugStringA("9");
        if (line[i] == '\n') OutputDebugStringA("n\n");
        else OutputDebugStringA("x");
    }


    int sizes[2] = { 0 };
    std::string curr = ""; int nums = 0;

    ReadFile(ppmhandle, static_cast<LPVOID>(&readBuffer), 10, &bytesRead, NULL);
    
    for (int i = 0; i < line.size(); i++) {
        if (isdigit(line[i])) {
            curr += line[i];
            continue;
        }
        if (isDig(line[i]) == isDig(line[i + 1])) {
            sizes[nums] = atoi(curr.c_str());
            //OutputDebugStringA(curr.c_str());
            //OutputDebugStringA("\n");
            curr.clear();
            nums++;
            if (nums == 2) {
                break;
            }
        }
    }
    //exit(0);
    
    bool bConsoleAlloc = AllocConsole();
    if (!bConsoleAlloc) return -8;
    AttachConsole(ATTACH_PARENT_PROCESS);
    HANDLE stdOut;
    stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (stdOut == NULL || stdOut == INVALID_HANDLE_VALUE) return -1;


    win.sizeX = sizes[0];
    win.sizeY = sizes[1];

    if (!win.Create(L"Clippom", WS_OVERLAPPEDWINDOW, win.sizeX, win.sizeY))
    {
        return 0;
    }

    ShowWindow(win.Window(), nCmdShow);

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CloseHandle(ppmhandle);
    FreeConsole();
    return 0;
}


LRESULT MainWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        if (FAILED(D2D1CreateFactory(
            D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory)))
        {
            return -1;  // Fail CreateWindowEx.
        }
        return 0;

    case WM_DESTROY:
        DiscardGraphicsResources();
        SafeRelease(&pFactory);
        PostQuitMessage(0);
        return 0;
    case WM_PAINT:
        OnPaint();
        return 0;

        // Other messages not shown...

    /*
    case WM_SIZE:
        Resize();
        return 0;
    */
    }
    return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}