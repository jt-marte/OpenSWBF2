#include <windows.h>
#include <string.h>

#include "pcDxvk.h"

#include "Battlefront2/Source/pcCrash.h"

namespace Dxvk
{

typedef IDirect3D9*(WINAPI* Direct3DCreate9Fn)(UINT uiSdkVersion);

// Stays loaded once loaded, also after a fallback: DXVK may still have
// threads or window hooks of its own, and it is only a few MB.
static HMODULE sModule;
static DisplayLogic::Renderer seRunning = DisplayLogic::kRendererNone;

// DXVK writes <exe>_d3d9.log into the working folder unless DXVK_LOG_PATH
// says otherwise: send it beside the game's -log, or into the dxvk folder.
// DXVK also makes the process DPI aware when it starts, which would undo
// -nodpi, so DPI stays the game's call (PcWindow::InitDpi). A DXVK_LOG_PATH
// or DXVK_CONFIG the user set wins.
static void PrepareEnvironment(const char* pcExe)
{
    char acValue[MAX_PATH];
    if (GetEnvironmentVariableA("DXVK_LOG_PATH", acValue, sizeof(acValue)) == 0)
    {
        char acLog[MAX_PATH];
        const char* pcLog = pcCrash::GetLogPath();
        const DWORD dwLog = pcLog[0] != '\0' ? GetFullPathNameA(pcLog, MAX_PATH, acLog, 0) : 0;
        if (DisplayLogic::DxvkLogDir(dwLog > 0 && dwLog < MAX_PATH ? acLog : 0, pcExe, acValue,
                                     sizeof(acValue)))
        {
            SetEnvironmentVariableA("DXVK_LOG_PATH", acValue);
        }
    }
    if (GetEnvironmentVariableA("DXVK_CONFIG", acValue, sizeof(acValue)) == 0)
    {
        SetEnvironmentVariableA("DXVK_CONFIG", "d3d9.dpiAware = False");
    }
}

// The module LoadLibrary gave back is DXVK's, not the system d3d9.dll the exe
// already has loaded under the same name.
static bool IsDxvkModule(HMODULE hModule)
{
    static const char kacTail[] = "\\dxvk\\d3d9.dll";
    const DWORD dwTail = sizeof(kacTail) - 1;
    char acPath[MAX_PATH];
    const DWORD dwLen = GetModuleFileNameA(hModule, acPath, MAX_PATH);
    return dwLen >= dwTail && dwLen < MAX_PATH && _stricmp(acPath + dwLen - dwTail, kacTail) == 0;
}

IDirect3D9* Create(DisplayLogic::VulkanFail& eFail, unsigned long& ulCode)
{
    ulCode = 0;
    char acExe[MAX_PATH];
    char acDll[MAX_PATH];
    const DWORD dwExe = GetModuleFileNameA(0, acExe, MAX_PATH);
    const bool bDll = dwExe > 0 && dwExe < MAX_PATH && DisplayLogic::DxvkDllPath(acExe, acDll, sizeof(acDll)) &&
                      GetFileAttributesA(acDll) != INVALID_FILE_ATTRIBUTES;
    bool bLoaded = false;
    Direct3DCreate9Fn pfnCreate = 0;
    IDirect3D9* pD3D = 0;
    unsigned int uiAdapters = 0;
    if (bDll)
    {
        PrepareEnvironment(acExe);
        if (!sModule)
        {
            // by full path; DXVK's own folder is searched first for anything it loads
            sModule = LoadLibraryExA(acDll, 0, LOAD_WITH_ALTERED_SEARCH_PATH);
            if (!sModule)
            {
                ulCode = GetLastError();
            }
        }
        bLoaded = sModule != 0 && IsDxvkModule(sModule);
    }
    if (bLoaded)
    {
        pfnCreate = (Direct3DCreate9Fn)GetProcAddress(sModule, "Direct3DCreate9");
    }
    if (pfnCreate)
    {
        // 0 without a Vulkan loader or driver; then an adapter count of 0 when
        // no GPU has what DXVK needs
        pD3D = pfnCreate(D3D_SDK_VERSION);
    }
    if (pD3D)
    {
        uiAdapters = pD3D->GetAdapterCount();
    }
    eFail = DisplayLogic::VulkanCheck(bDll, bLoaded, pfnCreate != 0, pD3D != 0, uiAdapters, S_OK);
    if (eFail != DisplayLogic::kVulkanOk && pD3D)
    {
        pD3D->Release();
        pD3D = 0;
    }
    return pD3D;
}

HRESULT ProbeDevice(IDirect3D9* pD3D, unsigned int uiAdapter, HWND hWnd)
{
    D3DCAPS9 caps;
    memset(&caps, 0, sizeof(caps));
    pD3D->GetDeviceCaps(uiAdapter, D3DDEVTYPE_HAL, &caps);
    const DWORD dwBehaviorFlags = (caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
                                      ? D3DCREATE_HARDWARE_VERTEXPROCESSING
                                      : D3DCREATE_SOFTWARE_VERTEXPROCESSING;

    D3DPRESENT_PARAMETERS pp;
    memset(&pp, 0, sizeof(pp));
    pp.BackBufferWidth = 640;
    pp.BackBufferHeight = 480;
    pp.BackBufferFormat = D3DFMT_A8R8G8B8;
    pp.BackBufferCount = 1;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.Windowed = TRUE;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    IDirect3DDevice9* pDevice = 0;
    HRESULT hr = pD3D->CreateDevice(uiAdapter, D3DDEVTYPE_HAL, hWnd, dwBehaviorFlags, &pp, &pDevice);
    if (SUCCEEDED(hr) && !pDevice)
    {
        hr = E_FAIL;
    }
    if (pDevice)
    {
        pDevice->Release();
    }
    return hr;
}

DisplayLogic::Renderer Running()
{
    return seRunning;
}

void SetRunning(DisplayLogic::Renderer eRenderer)
{
    seRunning = eRenderer;
}

}
