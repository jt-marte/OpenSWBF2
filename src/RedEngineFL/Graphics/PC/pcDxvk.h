#pragma once

#include <d3d9.h>

#include "pcDisplayLogic.h"

// OpenSWBF2: the Vulkan renderer. DXVK's d3d9.dll is loaded by full path from
// <exe dir>\dxvk, never from beside the exe, where it would also replace the
// system d3d9.dll the D3D9 renderer links. Only RedRenderer::InitStatic uses
// it, and only for a rendering run (never -norender or -dedicated).
namespace Dxvk
{

// DXVK's IDirect3D9 with at least one adapter, or 0 with the first step that
// failed (eFail) and its GetLastError (ulCode). Sets DXVK's log folder and
// config first (see pcDxvk.cpp).
IDirect3D9* Create(DisplayLogic::VulkanFail& eFail, unsigned long& ulCode);

// Creates and drops a small windowed device on the adapter the game picked,
// so a Vulkan driver that can't make one fails here, before VidMode_Init
// tests modes with it. The HRESULT of CreateDevice.
HRESULT ProbeDevice(IDirect3D9* pD3D, unsigned int uiAdapter, HWND hWnd);

// The renderer this run started on: kRendererNone until InitStatic picks
// one, and for headless runs.
DisplayLogic::Renderer Running();
void SetRunning(DisplayLogic::Renderer eRenderer);

}
