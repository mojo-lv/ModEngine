#include "pch.h"
#include "DebugMenu.h"
#include "Graphics.h"

#define LOG_KEY 'P'

static GraphicsContext gCtx;

static bool log_triggered = false;
static bool last_state = false;

static void DrawDebugMenu()
{
    auto* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::Begin("DebugMenu", nullptr, gCtx.sWindowFlags);

    if (g_log_debug_menu) {
        bool state = (GetAsyncKeyState(LOG_KEY) & 0x8000) != 0;
        log_triggered = state && !last_state;
        last_state = state;
    }

    for (int i = 0; i < g_menuList.size(); ++i) {
        if (log_triggered) {
            std::cout << "[debug_menu] " << g_menuList[i].text << std::endl;
        }

        ImGui::GetWindowDrawList()->AddText(
            gCtx.pMenuFont, g_fontConfig.size,
            ImVec2(g_menuList[i].fX, g_menuList[i].fY),
            ImColor(g_menuSelectedIndex == i ? g_fontConfig.color : IM_COL32_WHITE),
            g_menuList[i].text.c_str(), 0, 0.0f, 0);
    }
    g_menuList.clear();
    g_menuSelectedIndex = -1;

    ImGui::End();
}

static void InitImGui(IDXGISwapChain* pSwapChain)
{
    ID3D11Device* pDevice = nullptr;
    HWND hWindow = nullptr;
    HRESULT hr = pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&pDevice);
    if (FAILED(hr)) return;

    if (gCtx.pContext) gCtx.pContext->Release();
    pDevice->GetImmediateContext(&gCtx.pContext);
    DXGI_SWAP_CHAIN_DESC sd;
    pSwapChain->GetDesc(&sd);
    hWindow = sd.OutputWindow;

    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;

    ImGui_ImplWin32_Init(hWindow);
    ImGui_ImplDX11_Init(pDevice, gCtx.pContext);

    ID3D11Texture2D* RenderTargetTexture = nullptr;
    hr = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&RenderTargetTexture);
    if (FAILED(hr)) return;

    if (gCtx.pRenderTargetView) gCtx.pRenderTargetView->Release();
    pDevice->CreateRenderTargetView(RenderTargetTexture, nullptr, &gCtx.pRenderTargetView);
    RenderTargetTexture->Release();
    pDevice->Release();

    static const ImWchar RANGES[] = {
        0x0020, 0x007F, // Basic Latin
        0x00A0, 0x00FF, // Latin-1 Supplement
        0x2000, 0x206F, // General Punctuation
        0x2191, 0x2191, // General Punctuation
        0x226A, 0x226B,  // Much greater than/less than symbol
        0x25A0, 0x26C6, // Black box - White Diamond
        0x3000, 0x303F, // CJK Symbols and Punctuation
        0x3040, 0x309F, // Hiragana
        0x30A0, 0x30FF, // Katakana
        0x31F0, 0x31FF, // Katakana Phonetic Extensions
        0x4E00, 0x9FFF, // CJK Unified Ideographs
        0xFF00, 0xFFEF, // Halfwidth and Fullwidth Forms
        0,
    };

    ImFontConfig font_cfg;
    if (g_fontConfig.path.empty()) {
        gCtx.pMenuFont = ImGui::GetIO().Fonts->AddFontDefault();
    } else {
        gCtx.pMenuFont = ImGui::GetIO().Fonts->AddFontFromFileTTF(g_fontConfig.path.c_str(), g_fontConfig.size, &font_cfg, RANGES);
        if (gCtx.pMenuFont == nullptr) {
            gCtx.pMenuFont = ImGui::GetIO().Fonts->AddFontDefault();
        }
    }

    gCtx.sWindowFlags = ImGuiWindowFlags_NoTitleBar
                        | ImGuiWindowFlags_NoResize
                        | ImGuiWindowFlags_NoMove
                        | ImGuiWindowFlags_NoCollapse
                        | ImGuiWindowFlags_NoScrollbar
                        | ImGuiWindowFlags_NoScrollWithMouse
                        | ImGuiWindowFlags_NoBackground
                        | ImGuiWindowFlags_NoSavedSettings
                        | ImGuiWindowFlags_NoInputs
                        | ImGuiWindowFlags_NoFocusOnAppearing
                        | ImGuiWindowFlags_NoBringToFrontOnFocus;

    gCtx.pSwapChain = pSwapChain;
}

void RenderImGui(IDXGISwapChain* pSwapChain)
{
    if (pSwapChain != gCtx.pSwapChain) {
        InitImGui(pSwapChain);
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
    }
}
