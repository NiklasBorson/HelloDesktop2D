#pragma once

#pragma region Helpers

using Microsoft::WRL::ComPtr;

//
// Base class with trivial implementation of IUnknown methods.
//
// QueryInterface only supports IUnknown, but that's OK if all that's needed
// is reference-counting. A derived class can override QueryInterface if needed.
//
class ComObjectBase : public IUnknown
{
public:
    ComObjectBase() noexcept {}
    virtual ~ComObjectBase() {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void** ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() final override;
    ULONG STDMETHODCALLTYPE Release() final override;

    // Disallow copy, move, and assignment.
    ComObjectBase(ComObjectBase const&) = delete;
    ComObjectBase(ComObjectBase&&) = delete;
    void operator=(ComObjectBase const&) = delete;
    void operator=(ComObjectBase&&) = delete;

private:
    uint32_t m_refCount = 0;
};

//
// Error handling helpers.
//
class WinException
{
public:
    WinException(HRESULT hr) noexcept : m_hr(hr)
    {
    }

    HRESULT GetError() const noexcept
    {
        return m_hr;
    }

    [[noreturn]]
    static void Throw(HRESULT hr);

private:
    HRESULT m_hr;
};

class DeviceLostException : public WinException
{
public:
    using WinException::WinException;
};

inline void ThrowLastError()
{
    DWORD err = GetLastError();
    throw WinException{ HRESULT_FROM_WIN32(err) };
}

inline void HR(HRESULT hr)
{
    if (FAILED(hr))
    {
        WinException::Throw(hr);
    }
}

#pragma endregion // Helpers

#pragma region Resources

//
// IResource2D - interface for an object that encapsulates a Direct2D
// device-dependent resource and can be added to a ResourceList2D.
//
class IResource2D
{
public:
    IResource2D() noexcept {}

    // Abstract virtual methods.
    virtual void Initialize(ID2D1DeviceContext6* device) = 0;
    virtual bool IsInitialized() const noexcept = 0;
    virtual void Reset() noexcept = 0;

    // Disallow copy, move, and assignment.
    IResource2D(IResource2D const&) = delete;
    IResource2D(IResource2D&&) = delete;
    void operator=(IResource2D const&) = delete;
    void operator=(IResource2D&&) = delete;
};

//
// Resource2DBase - partial implementation of IResource2D.
//
template<typename T>
class Resource2DBase : public IResource2D
{
public:
    // Derived class must implement Initialize.
    void Initialize(ID2D1DeviceContext6* device) override = 0;

    // Resource2DBase implements IsInitialized and Reset.
    bool IsInitialized() const noexcept override
    {
        return m_ptr != nullptr;
    }

    void Reset() noexcept override
    {
        m_ptr = nullptr;
    }

    // Trivial getters.
    T* Get() const noexcept
    {
        return m_ptr.Get();
    }

    T* operator->() const noexcept
    {
        return m_ptr.Get();
    }

protected:
    ComPtr<T> m_ptr;
};

//
// SolidColorBrush - Implementation of IResource2D for solid color brush.
//
class SolidColorBrush : public Resource2DBase<ID2D1SolidColorBrush>
{
public:
    SolidColorBrush() noexcept : m_color{ 0, 0, 0, 1.0f }
    {
    }

    SolidColorBrush(D2D_COLOR_F color) noexcept : m_color{ color }
    {
    }

    SolidColorBrush(float r, float g, float b) : m_color{ r, g, b, 1.0f }
    {
    }

    void Initialize(ID2D1DeviceContext6* device) override;

    D2D_COLOR_F const& GetColor() const noexcept
    {
        return m_color;
    }

    void SetColor(D2D_COLOR_F newColor) noexcept;

private:
    D2D_COLOR_F m_color;
};

//
// ResourceList2D - non-owning collection of Direct2D device-dependent
// resources. Adding resources to a resource list ensures that they are
// initialized before use and reset when necessary (e.g., on device lost).
//
class ResourceList2D
{
public:
    void Add(IResource2D* p)
    {
        m_resources.push_back(p);
    }

    void ResetAll() noexcept;
    void EnsureInitialized(ID2D1DeviceContext6* device);

private:
    std::vector<IResource2D*> m_resources;
};

#pragma endregion // Resources

#pragma region DX_Context

//
// DXDevice - encapsulates a D3D device, which can be shared by
// multiple window contexts.
//
class DXDevice : public ComObjectBase
{
public:
    bool IsInitialized() const noexcept
    {
        return m_d2dDevice != nullptr;
    }

    void Reset() noexcept;

    void EnsureInitialized();

    ID2D1Factory7* GetD2dFactory() const noexcept
    {
        return m_d2dFactory.Get();
    }

    IDWriteFactory7* GetDWriteFactory() const noexcept
    {
        return m_dwriteFactory.Get();
    }

    ID3D11DeviceContext* GetD3dContext() const noexcept
    {
        return m_d3dContext.Get();
    }

    IDXGIDevice* GetDxgiDevice() const noexcept
    {
        return m_dxgiDevice.Get();
    }

    ID2D1Device6* GetD2dDevice() const noexcept
    {
        return m_d2dDevice.Get();
    }

    uint32_t GetGeneration() const noexcept
    {
        return m_generation;
    }

private:
    static ComPtr<ID2D1Factory7> CreateD2DFactory();
    static ComPtr<IDWriteFactory7> CreateDWriteFactory();

    const ComPtr<ID2D1Factory7> m_d2dFactory = CreateD2DFactory();
    const ComPtr<IDWriteFactory7> m_dwriteFactory = CreateDWriteFactory();

    ComPtr<ID3D11DeviceContext> m_d3dContext;
    ComPtr<IDXGIDevice> m_dxgiDevice;
    ComPtr<ID2D1Device6> m_d2dDevice;
    uint32_t m_generation = 0;
};

//
// DXWindowContext - manages a swap chain and Direct2D device context
// for a window.
//
class DXWindowContext : public ComObjectBase
{
public:
    DXWindowContext(DXDevice* device, HWND hwnd) noexcept;

    void Paint();

    // Static methods for handling window messages.
    static void OnResize(HWND hwnd) noexcept;
    static void OnDpiChanged(HWND hwnd, WPARAM wParam, LPARAM lParam) noexcept;
    static void OnPaint(HWND hwnd) noexcept;

    static DXWindowContext* GetThis(HWND hwnd) noexcept
    {
        return reinterpret_cast<DXWindowContext*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    static D2D_SIZE_U GetWindowSize(HWND hwnd) noexcept;

    // Getters.
    uint32_t GetPixelWidth() const noexcept
    {
        return m_pixelSize.width;
    }

    uint32_t GetPixelHeight() const noexcept
    {
        return m_pixelSize.height;
    }

    float GetWidthDips() const noexcept
    {
        return GetPixelWidth() * (96.0f / m_dpi);
    }

    float GetHeightDips() const noexcept
    {
        return GetPixelHeight() * (96.0f / m_dpi);
    }

    ID2D1Factory7* GetD2dFactory() const noexcept
    {
        return m_device->GetD2dFactory();
    }

    IDWriteFactory7* GetDWriteFactory() const noexcept
    {
        return m_device->GetDWriteFactory();
    }

    ID2D1DeviceContext6* GetD2dContext() const noexcept
    {
        return m_d2dContext.Get();
    }

    static void ForceDpi(uint16_t dpi) noexcept
    {
        m_forceDpi = dpi;
    }

protected:

    // Derived class calls AddResource to ensure device-dependent objects
    // are initialized and reinitialized as needed.
    void AddResource(IResource2D* p)
    {
        m_resourceList.Add(p);
    }

    // Abstract method implemented by derived class to render a frame.
    virtual void RenderContent() = 0;

    virtual void OnSizeChanged()
    {
    }

    virtual void OnDpiChanged()
    {
    }

private:

    // Internal message handlers.
    void OnResizeInternal();
    void OnDpiChangedInternal(uint32_t newDpi, RECT newRect);

    void ResetWindow() noexcept;
    void ResetDevice() noexcept;

    void EnsureInitialized();
    void PaintInternal();

    const ComPtr<DXDevice> m_device;
    uint32_t m_deviceGeneration = 0;

    HWND m_hwnd = nullptr;
    D2D_SIZE_U m_pixelSize = {};
    uint32_t m_dpi = 96;

    static uint32_t m_forceDpi;

    ComPtr<IDXGISwapChain> m_swapChain;
    ComPtr<ID2D1DeviceContext6> m_d2dContext;

    ResourceList2D m_resourceList;
};

#pragma endregion // DX_Context
