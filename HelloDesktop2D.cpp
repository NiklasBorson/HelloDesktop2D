#include "framework.h"
#include "HelloDesktop2D.h"

int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow
)
{
    // Process command-line option to force 96 DPI.
    if (wcscmp(lpCmdLine, L"-96") == 0)
    {
        DXWindowContext::ForceDpi(96);
    }

    // Create the device and the main window.
    wil::com_ptr<DXDevice> dxDevice{ new DXDevice{} };
    auto windowContext = HelloWorldWindow::Create(dxDevice.get(), hInstance, nCmdShow);

    // Process messages until the main window is destroyed.
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int) msg.wParam;
}

wil::com_ptr<HelloWorldWindow> HelloWorldWindow::Create(DXDevice* device, HINSTANCE hInstance, int showCommand)
{
    wchar_t const className[] = L"HelloWorldWindow";

    // Register the window class on the first call.
    static bool isClassRegistered = false;
    if (!isClassRegistered)
    {
        WNDCLASSEXW wcex = {};

        wcex.cbSize = sizeof(WNDCLASSEX);

        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WindowProc;
        //wcex.cbClsExtra = 0;
        //wcex.cbWndExtra = 0;
        wcex.hInstance = hInstance;
        wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_HELLODESKTOP2D));
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        //wcex.lpszMenuName = nullptr;
        wcex.lpszClassName = className;
        wcex.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SMALL));

        ATOM classAtom = RegisterClassExW(&wcex);
        THROW_LAST_ERROR_IF(classAtom == 0);

        isClassRegistered = true;
    }

    // Get the window title.
    constexpr uint32_t maxTitle = 100;
    wchar_t appTitle[maxTitle];
    LoadStringW(hInstance, IDS_APP_TITLE, appTitle, maxTitle);

    // Create the window.
    HWND hwnd = CreateWindowW(
        className,
        appTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0,
        CW_USEDEFAULT, 0,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );
    THROW_LAST_ERROR_IF_NULL(hwnd);

    // Show the window, so it has a size when we create the window context.
    ShowWindow(hwnd, showCommand);

    wil::com_ptr<HelloWorldWindow> windowContext{ new HelloWorldWindow{ device, hwnd } };

    UpdateWindow(hwnd);

    return windowContext;
}

HelloWorldWindow::HelloWorldWindow(DXDevice* device, HWND hwnd) :
    DXWindowContext{ device, hwnd }
{
    // Add the text brush resource, so it will be initialized.
    AddResource(&m_textBrush);

    // Create the text layout object.
    auto dwriteFactory = GetDWriteFactory();

    wil::com_ptr<IDWriteTextFormat> textFormat;
    THROW_IF_FAILED(dwriteFactory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        10.0f,
        L"en-us",
        textFormat.put()
    ));

    THROW_IF_FAILED(textFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));

    constexpr uint32_t lineCount = 24;
    m_textLines.reserve(lineCount);

    for (uint32_t i = 0; i < lineCount; i++)
    {
        TextLine textLine;

        static wchar_t const text[] = L"Hello World! 😀";
        constexpr uint32_t textLength = ARRAYSIZE(text) - 1;

        THROW_IF_FAILED(dwriteFactory->CreateTextLayout(
            text,
            textLength,
            textFormat.get(),
            0,
            0,
            textLine.textLayout.put()
        ));

        THROW_IF_FAILED(textLine.textLayout->SetFontSize(8.0f + i, DWRITE_TEXT_RANGE{ 0, textLength }));

        DWRITE_TEXT_METRICS textMetrics;
        THROW_IF_FAILED(textLine.textLayout->GetMetrics(&textMetrics));

        textLine.lineHeight = textMetrics.height;

        m_textLines.push_back(std::move(textLine));
    }
}

LRESULT CALLBACK HelloWorldWindow::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
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

void HelloWorldWindow::RenderContent()
{
    auto context = GetD2dContext();

    // Clear to white.
    context->Clear(D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 1.0f });

    // Draw the text lines.
    D2D_POINT_2F textPos{ 10.0f, 10.0f };

    for (auto& textLine : m_textLines)
    {
        context->DrawTextLayout(
            textPos,
            textLine.textLayout.get(),
            m_textBrush.Get(),
            D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT
        );

        textPos.y += textLine.lineHeight;
    }
}
