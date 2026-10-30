#include "pch.h"
#include "DebugMenu/Graphics.h"
#include "D3D11Hook.h"

typedef HRESULT(APIENTRY* Present_t)(IDXGISwapChain*, UINT, UINT);
static Present_t oPresent = nullptr;

typedef HRESULT(STDMETHODCALLTYPE* ResizeBuffers_t)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
static ResizeBuffers_t oResizeBuffers = nullptr;

static void** g_pSwapChainVTable = nullptr;

constexpr size_t SWAP_CHAIN_VTABLE_PRESENT = 8;
constexpr size_t SWAP_CHAIN_VTABLE_RESIZE_BUFFERS = 13;

static void WriteVTableFunction(void** pSlot, void* pFunction, void** pOriginalFunction)
{
    DWORD oldProtect;
    if (VirtualProtect(pSlot, sizeof(void*), PAGE_READWRITE, &oldProtect)) {
        if (pOriginalFunction) *pOriginalFunction = *pSlot;
        *pSlot = pFunction;
        VirtualProtect(pSlot, sizeof(void*), oldProtect, &oldProtect);
    }
}

HRESULT APIENTRY HookedD3D11Present(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
    RenderImGui(pSwapChain);
    return oPresent(pSwapChain, SyncInterval, Flags);
}

HRESULT STDMETHODCALLTYPE HookedResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount,
                            UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
    if (gCtx.pRenderTargetView) {
        gCtx.pRenderTargetView->Release();
        gCtx.pRenderTargetView = nullptr;
    }

    return oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
}

DWORD WINAPI ApplyD3D11Hook(LPVOID _param)
{
    // Wait for dxgi.dll to be loaded
    while (!GetModuleHandleW(L"dxgi.dll"))
        Sleep(3000);

    // Create a dummy device and swapchain to get the vtable address
    WNDCLASSEXW wc = {
        sizeof(WNDCLASSEXW), CS_CLASSDC, DefWindowProcW,
        0L, 0L, GetModuleHandleW(nullptr), nullptr, nullptr, nullptr, nullptr,
        L"DummyDX11", nullptr
    };
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW,
        0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swapChain = nullptr;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &featureLevel, 1,
        D3D11_SDK_VERSION, &sd, &swapChain, &device, nullptr, &context);

    if (SUCCEEDED(hr)) {
        // Replace the shared IDXGISwapChain virtual function pointers directly.
        g_pSwapChainVTable = *reinterpret_cast<void***>(swapChain);

        WriteVTableFunction(&g_pSwapChainVTable[SWAP_CHAIN_VTABLE_PRESENT],
                reinterpret_cast<void*>(&HookedD3D11Present),
                reinterpret_cast<void**>(&oPresent));

        WriteVTableFunction(&g_pSwapChainVTable[SWAP_CHAIN_VTABLE_RESIZE_BUFFERS],
                reinterpret_cast<void*>(&HookedResizeBuffers),
                reinterpret_cast<void**>(&oResizeBuffers));

        // Cleanup the dummy resources
        swapChain->Release();
        context->Release();
        device->Release();
    }

    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}

void RestoreD3D11Hook()
{
    if (!g_pSwapChainVTable) return;

    if (oPresent) {
        WriteVTableFunction(&g_pSwapChainVTable[SWAP_CHAIN_VTABLE_PRESENT],
                reinterpret_cast<void*>(oPresent), nullptr);
    }

    if (oResizeBuffers) {
        WriteVTableFunction(&g_pSwapChainVTable[SWAP_CHAIN_VTABLE_RESIZE_BUFFERS],
                reinterpret_cast<void*>(oResizeBuffers), nullptr);
    }
}
