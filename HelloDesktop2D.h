#pragma once

#include "resource.h"
#include "DXHelpers.h"

class HelloWorldWindow : public DXWindowContext
{
public:
    static wil::com_ptr<HelloWorldWindow> Create(DXDevice* device, HINSTANCE hInstance, int showCommand);

private:
    HelloWorldWindow(DXDevice* device, HWND hwnd);
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    void RenderContent() override;

    SolidColorBrush m_textBrush;

    struct TextLine
    {
        wil::com_ptr<IDWriteTextLayout> textLayout;
        float lineHeight;
    };

    std::vector<TextLine> m_textLines;
};
