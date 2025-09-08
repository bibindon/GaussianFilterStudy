#pragma comment( lib, "d3d9.lib" )
#if defined(DEBUG) || defined(_DEBUG)
#pragma comment( lib, "d3dx9d.lib" )
#else
#pragma comment( lib, "d3dx9.lib" )
#endif

#include <d3d9.h>
#include <d3dx9.h>
#include <string>
#include <tchar.h>
#include <cassert>
#include <vector>

#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = NULL; } }

LPDIRECT3D9 g_pD3D = NULL;
LPDIRECT3DDEVICE9 g_pd3dDevice = NULL;
LPD3DXMESH g_pMesh = NULL;
std::vector<D3DMATERIAL9> g_pMaterials;
std::vector<LPDIRECT3DTEXTURE9> g_pTextures;
DWORD g_dwNumMaterials = 0;
LPD3DXEFFECT g_pEffect = NULL;
bool g_bClose = false;

// バックバッファとデプスバッファを保存
LPDIRECT3DSURFACE9 g_pBackBuffer = NULL;
LPDIRECT3DSURFACE9 g_pDepthBuffer = NULL;

// オフスクリーン用
LPDIRECT3DTEXTURE9 g_pSceneTex = NULL;
LPDIRECT3DSURFACE9 g_pSceneSurface = NULL;
LPDIRECT3DSURFACE9 g_pSceneDepth = NULL;

// 一時バッファ（ブラー用）
LPDIRECT3DTEXTURE9 g_pTempTex = NULL;
LPDIRECT3DSURFACE9 g_pTempSurface = NULL;
LPDIRECT3DSURFACE9 g_pTempDepth = NULL;

const int WINDOW_SIZE_W = WINDOW_SIZE_W;
const int WINDOW_SIZE_H = WINDOW_SIZE_H;

struct ScreenVertex {
    float x, y, z, rhw;
    float u, v;
};
#define FVF_SCREENVERTEX (D3DFVF_XYZRHW | D3DFVF_TEX1)

static void InitD3D(HWND hWnd);
static void Cleanup();
static void Render();
static void DrawFullscreenQuad(LPDIRECT3DTEXTURE9 tex, const char* tech);
static void RenderSceneToTexture();

LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

extern int WINAPI _tWinMain(_In_ HINSTANCE hInstance,
                            _In_opt_ HINSTANCE hPrevInstance,
                            _In_ LPTSTR lpCmdLine,
                            _In_ int nCmdShow);

int WINAPI _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR lpCmdLine,
                     _In_ int nCmdShow)
{
    WNDCLASSEX wc { };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = MsgProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = _T("Window1");

    ATOM atom = RegisterClassEx(&wc);
    assert(atom != 0);

    RECT rect;
    SetRect(&rect, 0, 0, WINDOW_SIZE_W, WINDOW_SIZE_H);
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    rect.right = rect.right - rect.left;
    rect.bottom = rect.bottom - rect.top;

    HWND hWnd = CreateWindow(_T("Window1"),
                             _T("Gaussian Blur Sample"),
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             rect.right,
                             rect.bottom,
                             NULL,
                             NULL,
                             wc.hInstance,
                             NULL);

    InitD3D(hWnd);
    ShowWindow(hWnd, SW_SHOWDEFAULT);
    UpdateWindow(hWnd);

    MSG msg;
    while (true)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            DispatchMessage(&msg);
        }
        else
        {
            Sleep(16);
            Render();
        }
        if (g_bClose) break;
    }

    Cleanup();
    UnregisterClass(_T("Window1"), wc.hInstance);
    return 0;
}

void InitD3D(HWND hWnd)
{
    g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    assert(g_pD3D != NULL);

    D3DPRESENT_PARAMETERS d3dpp = {};
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    d3dpp.hDeviceWindow = hWnd;

    HRESULT hr = g_pD3D->CreateDevice(D3DADAPTER_DEFAULT,
                                      D3DDEVTYPE_HAL,
                                      hWnd,
                                      D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                      &d3dpp,
                                      &g_pd3dDevice);
    assert(SUCCEEDED(hr));

    g_pd3dDevice->GetRenderTarget(0, &g_pBackBuffer);
    g_pd3dDevice->GetDepthStencilSurface(&g_pDepthBuffer);

    // Mesh 読み込み
    LPD3DXBUFFER pMtrlBuffer = NULL;
    hr = D3DXLoadMeshFromX(_T("cube.x"),
                           D3DXMESH_SYSTEMMEM,
                           g_pd3dDevice,
                           NULL,
                           &pMtrlBuffer,
                           NULL,
                           &g_dwNumMaterials,
                           &g_pMesh);
    assert(SUCCEEDED(hr));

    D3DXMATERIAL* d3dxMaterials = (D3DXMATERIAL*)pMtrlBuffer->GetBufferPointer();
    g_pMaterials.resize(g_dwNumMaterials);
    g_pTextures.resize(g_dwNumMaterials);

    for (DWORD i = 0; i < g_dwNumMaterials; i++)
    {
        g_pMaterials[i] = d3dxMaterials[i].MatD3D;
        g_pMaterials[i].Ambient = g_pMaterials[i].Diffuse;
        g_pTextures[i] = NULL;
        if (d3dxMaterials[i].pTextureFilename)
        {
            hr = D3DXCreateTextureFromFileA(g_pd3dDevice,
                                            d3dxMaterials[i].pTextureFilename,
                                            &g_pTextures[i]);
        }
    }
    pMtrlBuffer->Release();

    // エフェクト読み込み
    hr = D3DXCreateEffectFromFile(g_pd3dDevice,
                                  _T("simple.fx"),
                                  NULL, NULL,
                                  D3DXSHADER_DEBUG,
                                  NULL,
                                  &g_pEffect,
                                  NULL);
    assert(SUCCEEDED(hr));

    // オフスクリーン用テクスチャ＋デプス
    D3DXCreateTexture(g_pd3dDevice,
                      WINDOW_SIZE_W,
                      WINDOW_SIZE_H,
                      1,
                      D3DUSAGE_RENDERTARGET,
                      D3DFMT_A8R8G8B8,
                      D3DPOOL_DEFAULT,
                      &g_pSceneTex);

    g_pSceneTex->GetSurfaceLevel(0, &g_pSceneSurface);

    g_pd3dDevice->CreateDepthStencilSurface(WINDOW_SIZE_W,
                                            WINDOW_SIZE_H,
                                            D3DFMT_D16,
                                            D3DMULTISAMPLE_NONE,
                                            0,
                                            TRUE,
                                            &g_pSceneDepth,
                                            NULL);

    // ブラー用一時テクスチャ＋デプス
    D3DXCreateTexture(g_pd3dDevice,
                      WINDOW_SIZE_W,
                      WINDOW_SIZE_H,
                      1,
                      D3DUSAGE_RENDERTARGET,
                      D3DFMT_A8R8G8B8,
                      D3DPOOL_DEFAULT,
                      &g_pTempTex);

    g_pTempTex->GetSurfaceLevel(0, &g_pTempSurface);

    g_pd3dDevice->CreateDepthStencilSurface(WINDOW_SIZE_W,
                                            WINDOW_SIZE_H,
                                            D3DFMT_D16,
                                            D3DMULTISAMPLE_NONE,
                                            0,
                                            TRUE,
                                            &g_pTempDepth,
                                            NULL);
}

void Cleanup()
{
    for (auto& t : g_pTextures)
    {
        SAFE_RELEASE(t);
    }

    SAFE_RELEASE(g_pMesh);
    SAFE_RELEASE(g_pEffect);

    SAFE_RELEASE(g_pSceneSurface);
    SAFE_RELEASE(g_pSceneTex);
    SAFE_RELEASE(g_pSceneDepth);

    SAFE_RELEASE(g_pTempSurface);
    SAFE_RELEASE(g_pTempTex);
    SAFE_RELEASE(g_pTempDepth);

    SAFE_RELEASE(g_pBackBuffer);
    SAFE_RELEASE(g_pDepthBuffer);

    SAFE_RELEASE(g_pd3dDevice);
    SAFE_RELEASE(g_pD3D);
}

void Render()
{
    // 1. シーンをオフスクリーンに描画
    RenderSceneToTexture();

    // 2. 横方向ブラー → g_pTempTex
    g_pd3dDevice->SetRenderTarget(0, g_pTempSurface);
    g_pd3dDevice->SetDepthStencilSurface(g_pTempDepth);
    g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
    g_pd3dDevice->BeginScene();
    DrawFullscreenQuad(g_pSceneTex, "GaussianH");
    g_pd3dDevice->EndScene();

    // 3. 縦方向ブラー → バックバッファ
    g_pd3dDevice->SetRenderTarget(0, g_pBackBuffer);
    g_pd3dDevice->SetDepthStencilSurface(g_pDepthBuffer);
    g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
    g_pd3dDevice->BeginScene();
    DrawFullscreenQuad(g_pTempTex, "GaussianV");

    g_pd3dDevice->EndScene();

    g_pd3dDevice->Present(NULL, NULL, NULL, NULL);
}

void RenderSceneToTexture()
{
    g_pd3dDevice->SetRenderTarget(0, g_pSceneSurface);
    g_pd3dDevice->SetDepthStencilSurface(g_pSceneDepth);

    g_pd3dDevice->Clear(0,
                        NULL,
                        D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                        D3DCOLOR_XRGB(100, 100, 100),
                        1.0f,
                        0);

    g_pd3dDevice->BeginScene();

    static float f = 0.0f;
    f += 0.02f;
    D3DXMATRIX View, Proj, World, mat;
    D3DXVECTOR3 eye(6 * sinf(f), 3, -6 * cosf(f));
    D3DXVECTOR3 at(0, 0, 0);
    D3DXVECTOR3 up(0, 1, 0);
    D3DXMatrixLookAtLH(&View, &eye, &at, &up);
    D3DXMatrixPerspectiveFovLH(&Proj,
                               D3DXToRadian(45),
                               WINDOW_SIZE_W / WINDOW_SIZE_H,
                               1.0f,
                               100.0f);

    D3DXMatrixIdentity(&World);
    mat = World * View * Proj;
    g_pEffect->SetMatrix("g_matWorldViewProj", &mat);

    g_pEffect->SetTechnique("Technique1");
    UINT numPass;
    g_pEffect->Begin(&numPass, 0);
    g_pEffect->BeginPass(0);
    for (DWORD i = 0; i < g_dwNumMaterials; i++)
    {
        g_pEffect->SetTexture("texture1", g_pTextures[i]);
        g_pEffect->CommitChanges();
        g_pMesh->DrawSubset(i);
    }
    g_pEffect->EndPass();
    g_pEffect->End();

    g_pd3dDevice->EndScene();
}

void DrawFullscreenQuad(LPDIRECT3DTEXTURE9 tex, const char* tech)
{
    g_pEffect->SetTechnique(tech);
    g_pEffect->SetTexture("g_SrcTex", tex);

    float texelSize[2] = { 1.0f / WINDOW_SIZE_W, 1.0f / WINDOW_SIZE_H };
    g_pEffect->SetFloatArray("g_TexelSize", texelSize, 2);

    ScreenVertex quad[4] = {
        {                -0.5f,                -0.5f, 0, 1, 0, 0 },
        { WINDOW_SIZE_W - 0.5f,                -0.5f, 0, 1, 1, 0 },
        {                -0.5f, WINDOW_SIZE_H - 0.5f, 0, 1, 0, 1 },
        { WINDOW_SIZE_W - 0.5f, WINDOW_SIZE_H - 0.5f, 0, 1, 1, 1 }
    };

    g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    g_pd3dDevice->SetFVF(FVF_SCREENVERTEX);
    g_pEffect->Begin(NULL, 0);
    g_pEffect->BeginPass(0);
    g_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(ScreenVertex));
    g_pEffect->EndPass();
    g_pEffect->End();
    g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
}

LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_DESTROY)
    {
        PostQuitMessage(0);
        g_bClose = true;
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}
