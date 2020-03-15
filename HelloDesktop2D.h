#pragma once

#include "resource.h"
#include "DXHelpers.h"

class HelloWorldWindow : public DXWindowContext
{
public:
    HelloWorldWindow(DXDevice* device, HWND hwnd);

private:
    void RenderContent() override;

    SolidColorBrush m_textBrush;
    ComPtr<IDWriteTextLayout> m_textLayout;
};
