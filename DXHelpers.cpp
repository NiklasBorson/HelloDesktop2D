#include "framework.h"
#include "HelloDesktop2D.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

#pragma region Helpers

[[noreturn]]
void WinException::Throw(HRESULT hr)
{
    // Throw appropriate exception type depending on error.
    switch (hr)
    {
    case D2DERR_RECREATE_TARGET:
    case DXGI_ERROR_DEVICE_REMOVED:
        throw DeviceLostException{ hr };

    default:
        throw WinException{ hr };
    }
}

HRESULT STDMETHODCALLTYPE ComObjectBase::QueryInterface(REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (riid == __uuidof(IUnknown))
    {
        *ppvObject = this;
        AddRef();
        return S_OK;
    }
    else
    {
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }
}

ULONG STDMETHODCALLTYPE ComObjectBase::AddRef()
{
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE ComObjectBase::Release()
{
    uint32_t newCount = --m_refCount;

    if (newCount == 0)
    {
        delete this;
    }

    return newCount;
}

#pragma endregion // Helpers

#pragma region Resources

void ResourceList2D::ResetAll() noexcept
{
    for (IResource2D* p : m_resources)
    {
        p->Reset();
    }
}

void ResourceList2D::EnsureInitialized(ID2D1DeviceContext6* device)
{
    for (IResource2D* p : m_resources)
    {
        if (!p->IsInitialized())
        {
            p->Initialize(device);
        }
    }
}

void SolidColorBrush::Initialize(ID2D1DeviceContext6* device)
{
    HR(device->CreateSolidColorBrush(m_color, m_ptr.ReleaseAndGetAddressOf()));
}

void SolidColorBrush::SetColor(D2D_COLOR_F newColor) noexcept
{
    if (m_ptr != nullptr)
    {
        m_ptr->SetColor(newColor);
    }
    m_color = newColor;
}

#pragma endregion // Resources

#pragma region DXDevice

ComPtr<ID2D1Factory7> DXDevice::CreateD2DFactory()
{
    ComPtr<ID2D1Factory7> ptr;
    HR(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, ptr.GetAddressOf()));
    return ptr;
}

ComPtr<IDWriteFactory7> DXDevice::CreateDWriteFactory()
{
    ComPtr<IDWriteFactory7> ptr;
    HR(DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory7),
        reinterpret_cast<IUnknown**>(ptr.GetAddressOf())
    ));
    return ptr;
}

void DXDevice::Reset() noexcept
{
    m_d3dContext.Reset();
    m_dxgiDevice.Reset();
    m_d2dDevice.Reset();
}

void DXDevice::EnsureInitialized()
{
    if (IsInitialized())
    {
        return;
    }

    // Create the D3D device.
    ComPtr<ID3D11Device> d3dDevice;
    ComPtr<ID3D11DeviceContext> d3dContext;
    HR(D3D11CreateDevice(
        nullptr, // use the default adapter
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr, // leave as null if hardware is used
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr, // levels
        0,
        D3D11_SDK_VERSION,
        &d3dDevice,
        nullptr,
        &d3dContext
        ));

    // Get the D3D device as a DXGI device.
    ComPtr<IDXGIDevice> dxgiDevice;
    HR(d3dDevice->QueryInterface(dxgiDevice.GetAddressOf()));

    // Create the D2D device.
    ComPtr<ID2D1Device6> d2dDevice;
    m_d2dFactory->CreateDevice(dxgiDevice.Get(), d2dDevice.GetAddressOf());

    // Initialize members.
    m_d3dContext = std::move(d3dContext);
    m_dxgiDevice = std::move(dxgiDevice);
    m_d2dDevice = std::move(d2dDevice);
    m_generation++;
}

#pragma endregion // DXDevice

#pragma region DXWindowContext

uint32_t DXWindowContext::m_forceDpi = 0;

DXWindowContext::DXWindowContext(DXDevice* device, HWND hwnd) noexcept :
    m_device{ device },
    m_hwnd{ hwnd },
    m_pixelSize{ GetWindowSize(hwnd) },
    m_dpi{ m_forceDpi ? m_forceDpi : GetDpiForWindow(hwnd) }
{
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<UINT_PTR>(this));
}

D2D_SIZE_U DXWindowContext::GetWindowSize(HWND hwnd) noexcept
{
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    return D2D_SIZE_U
    {
        std::max<uint32_t>(1, clientRect.right - clientRect.left),
        std::max<uint32_t>(1, clientRect.bottom - clientRect.top)
    };
}

void DXWindowContext::OnResize(HWND hwnd) noexcept
{
    auto context = GetThis(hwnd);
    if (context != nullptr)
    {
        try
        {
            context->OnResizeInternal();
        }
        catch (...)
        {
            std::terminate();
        }
    }
}

void DXWindowContext::OnResizeInternal()
{
    D2D_SIZE_U newSize = GetWindowSize(m_hwnd);
    if (newSize.width != m_pixelSize.width || newSize.height != m_pixelSize.height)
    {
        m_pixelSize = newSize;

        // Free all resources associated with this window (but not the device).
        ResetWindow();

        // Recreate all resources.
        EnsureInitialized();

        // Call virtual method, so derived class can update its layout.
        OnSizeChanged();
    }
}

void DXWindowContext::OnDpiChanged(HWND hwnd, WPARAM wParam, LPARAM lParam) noexcept
{
    auto context = GetThis(hwnd);
    if (context != nullptr)
    {
        try
        {
            // The horizontal DPI is in LOWORD(wParam).
            // The vertical DPI is in HIWORD(wParam) but always equals the horizontal DPI.
            // LPARAM points to a RECT with the new window bounds.
            context->OnDpiChangedInternal(
                m_forceDpi ? m_forceDpi : LOWORD(wParam),
                *reinterpret_cast<RECT*>(lParam)
            );
        }
        catch (...)
        {
            std::terminate();
        }
    }
}

void DXWindowContext::OnDpiChangedInternal(uint32_t newDpi, RECT newRect)
{
    if (newDpi != m_dpi)
    {
        // Save the new DPI.
        m_dpi = newDpi;

        // If the D2D context exists, set its DPI.
        auto d2dContext = GetD2dContext();
        if (d2dContext != nullptr)
        {
            d2dContext->SetDpi(static_cast<float>(newDpi), static_cast<float>(newDpi));
        }

        // Call virtual method, so derived class can update its layout.
        OnDpiChanged();
    }

    // Set the new window bounds.
    SetWindowPos(
        m_hwnd,
        nullptr,
        newRect.left,
        newRect.top,
        newRect.right - newRect.left,
        newRect.bottom - newRect.top,
        SWP_NOZORDER | SWP_NOACTIVATE
        );
}

void DXWindowContext::OnPaint(HWND hwnd) noexcept
{
    auto context = GetThis(hwnd);
    if (context != nullptr)
    {
        try
        {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);

            context->Paint();

            EndPaint(hwnd, &ps);
        }
        catch (...)
        {
            std::terminate();
        }
    }
    else
    {
        DefWindowProc(hwnd, WM_PAINT, 0, 0);
    }
}

void DXWindowContext::Paint()
{
    try
    {
        PaintInternal();
    }
    catch (DeviceLostException&)
    {
        ResetDevice();
        PaintInternal();
    }
}

void DXWindowContext::PaintInternal()
{
    // Ensure device-dependent resources are initialized.
    EnsureInitialized();

    // Begin drawing.
    GetD2dContext()->BeginDraw();

    // Call derived class method to render the window content.
    RenderContent();

    // End drawing and present.
    HR(GetD2dContext()->EndDraw());
    HR(m_swapChain->Present(0, 0));
}

void DXWindowContext::ResetWindow() noexcept
{
    m_d2dContext.Reset();
    m_swapChain.Reset();
}

void DXWindowContext::ResetDevice() noexcept
{
    // Reset the swap chain.
    ResetWindow();

    // Reset device-dependent resources.
    m_resourceList.ResetAll();

    // Reset the device if it hasn't already been reset and reinitialized
    // by another window context.
    if (m_deviceGeneration == m_device->GetGeneration())
    {
        m_device->Reset();
    }
}

void DXWindowContext::EnsureInitialized()
{
    // If the D2D context is already initialized, then just make sure all
    // D2D resources are initialized.
    if (m_d2dContext != nullptr)
    {
        m_resourceList.EnsureInitialized(m_d2dContext.Get());
        return;
    }

    // Ensure the device is initialized, and remember the device generation.
    m_device->EnsureInitialized();
    m_deviceGeneration = m_device->GetGeneration();

    // Get the DXGI factory.
    auto dxgiDevice = m_device->GetDxgiDevice();
    ComPtr<IDXGIAdapter> dxgiAdapter;
    HR(dxgiDevice->GetAdapter(&dxgiAdapter));

    ComPtr<IDXGIFactory> dxgiFactory;
    HR(dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory)));

    // Create a DXGI swap chain for the window.
    DXGI_SWAP_CHAIN_DESC scDesc = {};
    scDesc.BufferDesc.Width = GetPixelWidth();
    scDesc.BufferDesc.Height = GetPixelHeight();
    scDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scDesc.SampleDesc.Count = 1;
    scDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER | DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount = 1;
    scDesc.OutputWindow = m_hwnd;
    scDesc.Windowed = true;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    ComPtr<IDXGISwapChain> dxgiSwapChain;
    HR(dxgiFactory->CreateSwapChain(
        dxgiDevice,
        &scDesc,
        dxgiSwapChain.GetAddressOf()
    ));

    CD3D11_TEXTURE2D_DESC desc(
        DXGI_FORMAT_B8G8R8A8_UNORM,
        GetPixelWidth(),
        GetPixelHeight(),
        1, // arraySize 
        1, // mipLevels
        D3D11_BIND_RENDER_TARGET // bindFlags
    );

    // Get the DXGI surface for the swap chain.
    ComPtr<IDXGISurface> dxgiSurface;
    HR(dxgiSwapChain->GetBuffer(0, IID_PPV_ARGS(&dxgiSurface)));

    // Create the D2D context.
    auto d2dDevice = m_device->GetD2dDevice();
    ComPtr<ID2D1DeviceContext6> d2dContext;
    HR(d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d2dContext));

    // Set the DPI.
    d2dContext->SetDpi(static_cast<float>(m_dpi), static_cast<float>(m_dpi));

    // Create a D2D bitmap from the swap chain surface, and set it as the target.
    ComPtr<ID2D1Bitmap1> d2dBitmap;
    HR(d2dContext->CreateBitmapFromDxgiSurface(
        dxgiSurface.Get(),
        D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)
        ),
        &d2dBitmap
    ));

    d2dContext->SetTarget(d2dBitmap.Get());

    // Initialize device-dependent resources.
    m_resourceList.EnsureInitialized(d2dContext.Get());

    // Update members.
    m_swapChain = std::move(dxgiSwapChain);
    m_d2dContext = std::move(d2dContext);
}

#pragma endregion // DXWindowContext
