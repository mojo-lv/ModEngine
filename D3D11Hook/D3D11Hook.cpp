#include "pch.h"
#include "ThirdParty/minhook/include/MinHook.h"
#include "DebugMenu/Graphics.h"
#include "D3D11Hook.h"

typedef HRESULT(APIENTRY* Present_t)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
static Present_t oPresent = nullptr; // Original function pointer

HRESULT APIENTRY HookedD3D11Present(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
    RenderImGui(pSwapChain);
    return oPresent(pSwapChain, SyncInterval, Flags);
}

DWORD WINAPI ApplyD3D11Hook(LPVOID _param)
{
    // Wait for dxgi.dll to be loaded
    while (!GetModuleHandleW(L"dxgi.dll"))
        Sleep(1000);

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
    sd.BufferCount = 2;
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
        // Get the address of the Present function from the vtable
        void** pVTable = *reinterpret_cast<void***>(swapChain);
        void* pPresentAddress = pVTable[8];
        if (MH_CreateHook(pPresentAddress, &HookedD3D11Present, reinterpret_cast<void**>(&oPresent)) == MH_OK) {
            MH_EnableHook(pPresentAddress);
        }

        // Cleanup the dummy resources
        swapChain->Release();
        device->Release();
        context->Release();
    }

    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
