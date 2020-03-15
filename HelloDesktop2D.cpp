#include "framework.h"
#include "HelloDesktop2D.h"

wchar_t const g_windowClass[] = L"wc_HelloDesktop2D";

HWND CreateMainWindow(HINSTANCE hInstance);
LRESULT CALLBACK MainWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow
)
{
    // Programmatically declare this process is DPI-aware.
    // This must be done before any windows are created, or anything else that depends on DPI.
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT{ DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 }))
    {
        ThrowLastError();
    }

    // Create the main window.
    HWND hwnd = CreateMainWindow(hInstance);

    ShowWindow(hwnd, nCmdShow);

    ComPtr<DXDevice> dxDevice{ new DXDevice{} };
    ComPtr<HelloWorldWindow> windowContext{ new HelloWorldWindow{ dxDevice.Get(), hwnd } };

    UpdateWindow(hwnd);

    // Process messages until the main window is destroyed.
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int) msg.wParam;
}

HWND CreateMainWindow(HINSTANCE hInstance)
{
    constexpr uint32_t maxTitle = 100;
    wchar_t appTitle[maxTitle];

    LoadStringW(hInstance, IDS_APP_TITLE, appTitle, maxTitle);

    WNDCLASSEXW wcex = {};

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = MainWindowProc;
    //wcex.cbClsExtra = 0;
    //wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_HELLODESKTOP2D));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    //wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = g_windowClass;
    wcex.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SMALL));

    ATOM classAtom = RegisterClassExW(&wcex);
    if (classAtom == 0)
    {
        ThrowLastError();
    }

    HWND hwnd = CreateWindowW(
        g_windowClass, 
        appTitle, 
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 
        CW_USEDEFAULT, 0, 
        nullptr, 
        nullptr, 
        hInstance, 
        nullptr
    );

    if (hwnd == nullptr)
    {
        ThrowLastError();
    }

    return hwnd;
}

LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
        DXWindowContext::OnPaint(hwnd);
        break;
    case WM_SIZE:
        DXWindowContext::OnResize(hwnd);
        break;
    case WM_DPICHANGED:
        DXWindowContext::OnDpiChanged(hwnd, wParam, lParam);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

HelloWorldWindow::HelloWorldWindow(DXDevice* device, HWND hwnd) :
    DXWindowContext{ device, hwnd }
{
    // Add the text brush resource, so it will be initialized.
    AddResource(&m_textBrush);

    // Create the text layout object.
    auto dwriteFactory = GetDWriteFactory();

    ComPtr<IDWriteTextFormat> textFormat;
    HR(dwriteFactory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        24.0f,
        L"en-us",
        &textFormat
    ));

    HR(textFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));

    static wchar_t const text[] = L"Hello World!";

    HR(dwriteFactory->CreateTextLayout(
        text,
        ARRAYSIZE(text) - 1,
        textFormat.Get(),
        0,
        0,
        &m_textLayout
    ));
}

void HelloWorldWindow::RenderContent()
{
    auto context = GetD2dContext();

    // Clear to white.
    context->Clear(D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 1.0f });

    // Draw the text.
    context->DrawTextLayout(
        D2D1_POINT_2F{ 10.0f, 10.0f },
        m_textLayout.Get(),
        m_textBrush.Get()
        );
}
