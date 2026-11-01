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

LRESULT CALLBACK DXGIMsgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

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
    while (!GetModuleHandleA("dxgi.dll")) {
        Sleep(4000);
    }

    WNDCLASSEXA wc = { sizeof(WNDCLASSEX), CS_CLASSDC, DXGIMsgProc, 0L, 0L,
        GetModuleHandleA(NULL), NULL, NULL, NULL, NULL, "DX", NULL };
    RegisterClassExA(&wc);
    HWND window = CreateWindowA("DX", NULL, WS_OVERLAPPEDWINDOW,
        100, 100, 300, 300, NULL, NULL, wc.hInstance, NULL);

    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    scd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

    scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    scd.OutputWindow = window;
    scd.SampleDesc.Count = 1;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    scd.Windowed = ((GetWindowLongPtr(window, GWL_STYLE) & WS_POPUP) != 0) ? false : true;

    scd.BufferDesc.Width = 1;
    scd.BufferDesc.Height = 1;
    scd.BufferDesc.RefreshRate.Numerator = 0;
    scd.BufferDesc.RefreshRate.Denominator = 1;

    D3D_FEATURE_LEVEL requestedLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };
    IDXGISwapChain* d3dSwapChain = nullptr;
    ID3D11Device* d3dDevice = nullptr;
    ID3D11DeviceContext* d3dContext = nullptr;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        requestedLevels,
        sizeof(requestedLevels) / sizeof(D3D_FEATURE_LEVEL),
        D3D11_SDK_VERSION,
        &scd,
        &d3dSwapChain,
        &d3dDevice,
        nullptr,
        &d3dContext);

    if (SUCCEEDED(hr)) {
        // Replace the shared IDXGISwapChain virtual function pointers directly.
        g_pSwapChainVTable = *reinterpret_cast<void***>(d3dSwapChain);

        WriteVTableFunction(&g_pSwapChainVTable[SWAP_CHAIN_VTABLE_PRESENT],
                reinterpret_cast<void*>(&HookedD3D11Present),
                reinterpret_cast<void**>(&oPresent));

        WriteVTableFunction(&g_pSwapChainVTable[SWAP_CHAIN_VTABLE_RESIZE_BUFFERS],
                reinterpret_cast<void*>(&HookedResizeBuffers),
                reinterpret_cast<void**>(&oResizeBuffers));

        // Cleanup the dummy resources
        d3dSwapChain->Release();
        d3dContext->Release();
        d3dDevice->Release();
    }

    DestroyWindow(window);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);
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
