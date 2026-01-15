#pragma once

#include <d3d11.h>
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/imgui_impl_win32.h"
#include "ThirdParty/imgui/imgui_impl_dx11.h"

#define MENU_ENTRIES_COUNT 200

struct GraphicsContext {
    IDXGISwapChain*         pSwapChain        = nullptr;
    ID3D11DeviceContext*    pContext          = nullptr;
    ID3D11RenderTargetView* pRenderTargetView = nullptr;
    ImFont*                 pMenuFont         = nullptr;
    ImGuiWindowFlags        sWindowFlags      = 0;
};

void RenderImGui(IDXGISwapChain* pSwapChain);
void ShutdownImGui();
