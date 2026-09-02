#include "pch.h"
#include "DebugMenu.h"
#include "Graphics.h"

#define LOG_KEY 'P'

static const ImWchar RANGES[] = {
    0x0020, 0x007F, // Basic Latin
    0x00A0, 0x00FF, // Latin-1 Supplement
    0x2000, 0x206F, // General Punctuation
    0x2191, 0x2191, // General Punctuation
    0x226A, 0x226B, // Much greater than/less than symbol
    0x25A0, 0x26C6, // Black box - White Diamond
    0x3000, 0x303F, // CJK Symbols and Punctuation
    0x3040, 0x309F, // Hiragana
    0x30A0, 0x30FF, // Katakana
    0x31F0, 0x31FF, // Katakana Phonetic Extensions
    0x4E00, 0x9FFF, // CJK Unified Ideographs
    0xFF00, 0xFFEF, // Halfwidth and Fullwidth Forms
    0,
};

static constexpr ImGuiWindowFlags FLAGS = ImGuiWindowFlags_NoMove
                        | ImGuiWindowFlags_NoScrollWithMouse
                        | ImGuiWindowFlags_NoBackground
                        | ImGuiWindowFlags_NoSavedSettings
                        | ImGuiWindowFlags_NoFocusOnAppearing
                        | ImGuiWindowFlags_NoBringToFrontOnFocus
                        | ImGuiWindowFlags_NoDecoration
                        | ImGuiWindowFlags_NoInputs;

static bool log_triggered = false;
static bool last_state = false;

GraphicsContext gCtx;

void ShutdownImGui()
{
    if (gCtx.pSwapChain) {
        gCtx.pSwapChain = nullptr;

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        if (gCtx.pRenderTargetView) {
            gCtx.pRenderTargetView->Release();
            gCtx.pRenderTargetView = nullptr;
        }

        if (gCtx.pContext) {
            gCtx.pContext->Release();
            gCtx.pContext = nullptr;
        }

        if (gCtx.pDevice) {
            gCtx.pDevice->Release();
            gCtx.pDevice = nullptr;
        }
    }
}

static void DrawDebugMenu()
{
    auto* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::Begin("DebugMenu", nullptr, FLAGS);

    if (g_log_debug_menu) {
        bool state = (GetAsyncKeyState(LOG_KEY) & 0x8000) != 0;
        log_triggered = state && !last_state;
        last_state = state;
    }

    uintptr_t ptr = *(uintptr_t*)(*(uintptr_t*)(0x143f3b400) + 0xa0);
    float offsetX = *(float*)(ptr + 0x118);
    float offsetY = *(float*)(ptr + 0x11c);

    for (int i = 0; i < g_menuList.size(); ++i) {
        if (log_triggered) {
            std::cout << "[debug_menu] " << g_menuList[i].text.c_str() << std::endl;
        }

        ImGui::GetWindowDrawList()->AddText(
            gCtx.pMenuFont, g_fontConfig.size,
            ImVec2(g_menuList[i].fX + offsetX, g_menuList[i].fY + offsetY),
            ImColor(g_menuSelectedIndex == i ? g_fontConfig.color : IM_COL32_WHITE),
            g_menuList[i].text.c_str(), nullptr, 0.0f, nullptr);
    }
    g_menuList.clear();
    g_menuSelectedIndex = -1;

    ImGui::End();
}

static void UpdateRenderTargetView()
{
    ID3D11Texture2D* RenderTargetTexture = nullptr;
    HRESULT hr = gCtx.pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&RenderTargetTexture);
    if (FAILED(hr)) return;

    gCtx.pDevice->CreateRenderTargetView(RenderTargetTexture, nullptr, &gCtx.pRenderTargetView);
    RenderTargetTexture->Release();
}

static void InitImGui(IDXGISwapChain* pSwapChain)
{
    ShutdownImGui();
    HRESULT hr = pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&gCtx.pDevice);
    if (FAILED(hr)) return;

    gCtx.pSwapChain = pSwapChain;
    gCtx.pDevice->GetImmediateContext(&gCtx.pContext);
    DXGI_SWAP_CHAIN_DESC sd;
    pSwapChain->GetDesc(&sd);
    HWND hWindow = sd.OutputWindow;

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    if (gCtx.pMenuFont) {
        io.Fonts->Clear();
        gCtx.pMenuFont = nullptr;
    }
    if (!g_fontConfig.path.empty()) {
        gCtx.pMenuFont = io.Fonts->AddFontFromFileTTF(g_fontConfig.path.c_str(), g_fontConfig.size, nullptr, RANGES);
    }
    if (!gCtx.pMenuFont) {
        gCtx.pMenuFont = io.Fonts->AddFontDefault();
    }

    ImGui_ImplWin32_Init(hWindow);
    ImGui_ImplDX11_Init(gCtx.pDevice, gCtx.pContext);
    UpdateRenderTargetView();
}

void RenderImGui(IDXGISwapChain* pSwapChain)
{
    if (pSwapChain != gCtx.pSwapChain) {
        InitImGui(pSwapChain);
    } else if (!gCtx.pRenderTargetView) {
        UpdateRenderTargetView();
    } else if (!g_menuList.empty()) {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        DrawDebugMenu();

        ImGui::Render();
        gCtx.pContext->OMSetRenderTargets(1, &gCtx.pRenderTargetView, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }
}
