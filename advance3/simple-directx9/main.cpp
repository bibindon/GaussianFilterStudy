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
LPD3DXFONT g_pFont = NULL;
bool g_bClose = false;

// オフスクリーン用
LPDIRECT3DTEXTURE9 g_pSceneTex = NULL;
LPDIRECT3DTEXTURE9 g_pTempTex = NULL;

// 開始スケールを 1 / (2^kStartExp) にする
static const int kStartExp = 1;

// レベル数（1/2, 1/4, 1/8, 1/16, 1/32）
static const int kNumLevels = 5;

// 低解像度チェーン
std::vector<LPDIRECT3DTEXTURE9> g_texDown;
std::vector<LPDIRECT3DTEXTURE9> g_texUp;

float g_filterSpacing = 1.0f;
int g_activeBlurLevels = kNumLevels;

const int WINDOW_SIZE_W = 1600;
const int WINDOW_SIZE_H = 900;

struct ScreenVertex {
    float x, y, z, rhw;
    float u, v;
};
#define FVF_SCREENVERTEX (D3DFVF_XYZRHW | D3DFVF_TEX1)

// 現在の RT サイズに合う全画面板を描き、SrcTex, g_TexelSize をセット
static void DrawFullscreenQuadCurrentRT(LPDIRECT3DTEXTURE9 srcTex, const char* tech)
{
    // 現在の RT サイズ
    IDirect3DSurface9* pRT = NULL;
    g_pd3dDevice->GetRenderTarget(0, &pRT);
    D3DSURFACE_DESC rtDesc = {};
    pRT->GetDesc(&rtDesc);
    SAFE_RELEASE(pRT);

    // サンプル元テクスチャのテクセルサイズ
    D3DSURFACE_DESC srcDesc = {};
    srcTex->GetLevelDesc(0, &srcDesc);
    float texelSize[2] = { 1.0f / srcDesc.Width, 1.0f / srcDesc.Height };

    g_pEffect->SetTechnique(tech);
    g_pEffect->SetTexture("g_SrcTex", srcTex);
    g_pEffect->SetFloatArray("g_TexelSize", texelSize, 2);
    g_pEffect->SetFloat("g_FilterSpacing", g_filterSpacing);

    ScreenVertex quad[4] =
    {
        {                -0.5f,                 -0.5f, 0, 1, 0, 0 },
        { (float)rtDesc.Width  - 0.5f,         -0.5f, 0, 1, 1, 0 },
        {                -0.5f, (float)rtDesc.Height - 0.5f, 0, 1, 0, 1 },
        { (float)rtDesc.Width  - 0.5f, (float)rtDesc.Height - 0.5f, 0, 1, 1, 1 }
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


static void InitD3D(HWND hWnd);
static void Cleanup();
static void Render();
static void DrawFullscreenQuad(LPDIRECT3DTEXTURE9 tex, const char* tech);
static void UpdateInput();
static void DrawOverlayText();
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

    hr = D3DXCreateFont(g_pd3dDevice,
                        22,
                        0,
                        FW_NORMAL,
                        1,
                        FALSE,
                        DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS,
                        DEFAULT_QUALITY,
                        DEFAULT_PITCH | FF_DONTCARE,
                        _T("MS Gothic"),
                        &g_pFont);
    assert(SUCCEEDED(hr));

    // オフスクリーン用テクスチャ（サーフェイスは毎回ローカル取得）
    D3DXCreateTexture(g_pd3dDevice,
                      WINDOW_SIZE_W,
                      WINDOW_SIZE_H,
                      1,
                      D3DUSAGE_RENDERTARGET,
                      D3DFMT_A8R8G8B8,
                      D3DPOOL_DEFAULT,
                      &g_pSceneTex);

    D3DXCreateTexture(g_pd3dDevice,
                      WINDOW_SIZE_W,
                      WINDOW_SIZE_H,
                      1,
                      D3DUSAGE_RENDERTARGET,
                      D3DFMT_A8R8G8B8,
                      D3DPOOL_DEFAULT,
                      &g_pTempTex);

    g_texDown.assign(kNumLevels, NULL);
    g_texUp.assign(kNumLevels, NULL);

    for (int level = 0; level < kNumLevels; ++level)
    {
        int levelExp = kStartExp + level;
        int texW = WINDOW_SIZE_W >> levelExp;
        int texH = WINDOW_SIZE_H >> levelExp;

        if (texW < 1) { texW = 1; }
        if (texH < 1) { texH = 1; }

        D3DXCreateTexture(g_pd3dDevice, texW, texH, 1,
                          D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                          D3DPOOL_DEFAULT, &g_texDown[level]);

        D3DXCreateTexture(g_pd3dDevice, texW, texH, 1,
                          D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                          D3DPOOL_DEFAULT, &g_texUp[level]);
    }
}

void Cleanup()
{
    for (auto& t : g_pTextures)
    {
        SAFE_RELEASE(t);
    }

    for (auto& t : g_texDown)
    {
        SAFE_RELEASE(t);
    }
    for (auto& t : g_texUp)
    {
        SAFE_RELEASE(t);
    }

    SAFE_RELEASE(g_pMesh);
    SAFE_RELEASE(g_pFont);
    SAFE_RELEASE(g_pEffect);

    SAFE_RELEASE(g_pSceneTex);
    SAFE_RELEASE(g_pTempTex);

    SAFE_RELEASE(g_pd3dDevice);
    SAFE_RELEASE(g_pD3D);
}

void Render()
{
    UpdateInput();
    RenderSceneToTexture();

    // Down チェーン
    {
        IDirect3DSurface9* renderTarget = NULL;
        LPDIRECT3DTEXTURE9 sourceTex = g_pSceneTex;

        for (int level = 0; level < g_activeBlurLevels; ++level)
        {
            g_texDown[level]->GetSurfaceLevel(0, &renderTarget);
            g_pd3dDevice->SetRenderTarget(0, renderTarget);
            SAFE_RELEASE(renderTarget);

            g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
            g_pd3dDevice->BeginScene();
            DrawFullscreenQuadCurrentRT(sourceTex, "Down3x3");
            g_pd3dDevice->EndScene();

            sourceTex = g_texDown[level];
        }
    }

    // --- Up チェーン（純粋アップサンプル） ---
    if (g_activeBlurLevels > 0)
    {
        IDirect3DSurface9* rt = NULL;
        int last = g_activeBlurLevels - 1;

        // 最下段をコピー
        g_texUp[last]->GetSurfaceLevel(0, &rt);
        g_pd3dDevice->SetRenderTarget(0, rt);
        SAFE_RELEASE(rt);
        g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
        g_pd3dDevice->BeginScene();
        DrawFullscreenQuadCurrentRT(g_texDown[last], "Copy");
        g_pd3dDevice->EndScene();

        // 各段：足し戻し無しで上へ
        for (int level = last - 1; level >= 0; --level)
        {
            g_texUp[level]->GetSurfaceLevel(0, &rt);
            g_pd3dDevice->SetRenderTarget(0, rt);
            SAFE_RELEASE(rt);

            g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
            g_pd3dDevice->BeginScene();
            DrawFullscreenQuadCurrentRT(g_texUp[level + 1], "UpsampleOnly3x3");
            g_pd3dDevice->EndScene();
        }
    }

    // Up チェーン
    if (false)
    {
        IDirect3DSurface9* renderTarget = NULL;

        // 最下段のコピー
        int last = kNumLevels - 1;
        g_texUp[last]->GetSurfaceLevel(0, &renderTarget);
        g_pd3dDevice->SetRenderTarget(0, renderTarget);
        SAFE_RELEASE(renderTarget);

        g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
        g_pd3dDevice->BeginScene();
        DrawFullscreenQuadCurrentRT(g_texDown[last], "Copy");
        g_pd3dDevice->EndScene();

        // ひとつ上へ順次足し戻し
        for (int level = last - 1; level >= 0; --level)
        {
            g_texUp[level]->GetSurfaceLevel(0, &renderTarget);
            g_pd3dDevice->SetRenderTarget(0, renderTarget);
            SAFE_RELEASE(renderTarget);

            g_pEffect->SetTexture("g_SrcTex2", g_texDown[level]);

            g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
            g_pd3dDevice->BeginScene();
            DrawFullscreenQuadCurrentRT(g_texUp[level + 1], "UpsampleAdd3x3");
            g_pd3dDevice->EndScene();
        }
    }

    // 最終出力（フル解像度へアップサンプル）
    {
        IDirect3DSurface9* backBuffer = NULL;
        g_pd3dDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
        g_pd3dDevice->SetRenderTarget(0, backBuffer);
        SAFE_RELEASE(backBuffer);

        g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
        g_pd3dDevice->BeginScene();
        if (g_activeBlurLevels == 0)
        {
            DrawFullscreenQuadCurrentRT(g_pSceneTex, "Copy");
        }
        else
        {
            DrawFullscreenQuadCurrentRT(g_texUp[0], "UpsampleOnly3x3");
        }
        DrawOverlayText();
        g_pd3dDevice->EndScene();
    }

    g_pd3dDevice->Present(NULL, NULL, NULL, NULL);
}

void UpdateInput()
{
    static bool prevKey1 = false;
    static bool prevKey2 = false;
    static bool prevKey3 = false;
    static bool prevKey4 = false;

    const bool key1 = (GetAsyncKeyState('1') & 0x8000) != 0;
    const bool key2 = (GetAsyncKeyState('2') & 0x8000) != 0;
    const bool key3 = (GetAsyncKeyState('3') & 0x8000) != 0;
    const bool key4 = (GetAsyncKeyState('4') & 0x8000) != 0;

    if (key1 && !prevKey1)
    {
        g_filterSpacing -= 0.05f;
    }
    if (key2 && !prevKey2)
    {
        g_filterSpacing += 0.05f;
    }
    if (key3 && !prevKey3)
    {
        --g_activeBlurLevels;
    }
    if (key4 && !prevKey4)
    {
        ++g_activeBlurLevels;
    }

    if (g_filterSpacing < 0.0f) { g_filterSpacing = 0.0f; }
    if (g_filterSpacing > 1.0f) { g_filterSpacing = 1.0f; }
    if (g_activeBlurLevels < 0) { g_activeBlurLevels = 0; }
    if (g_activeBlurLevels > kNumLevels) { g_activeBlurLevels = kNumLevels; }

    prevKey1 = key1;
    prevKey2 = key2;
    prevKey3 = key3;
    prevKey4 = key4;
}

void DrawOverlayText()
{
    if (g_pFont == NULL)
    {
        return;
    }

    TCHAR text[256] = {};
    _stprintf_s(text,
                _T("1/2: sample spacing -/+\n3/4: blur levels -/+\n3x3 spacing: %.2f\nblur levels: %d / %d"),
                g_filterSpacing,
                g_activeBlurLevels,
                kNumLevels);

    RECT shadowRect = { 17, 17, WINDOW_SIZE_W, WINDOW_SIZE_H };
    RECT textRect = { 16, 16, WINDOW_SIZE_W, WINDOW_SIZE_H };

    g_pFont->DrawText(NULL, text, -1, &shadowRect, DT_LEFT | DT_TOP, D3DCOLOR_ARGB(220, 0, 0, 0));
    g_pFont->DrawText(NULL, text, -1, &textRect, DT_LEFT | DT_TOP, D3DCOLOR_ARGB(255, 255, 255, 255));
}

void RenderSceneToTexture()
{
    // ----[変更] オフスクリーンのRT面をローカルで取得 ----
    IDirect3DSurface9* pSceneRT = NULL;
    g_pSceneTex->GetSurfaceLevel(0, &pSceneRT);
    g_pd3dDevice->SetRenderTarget(0, pSceneRT);
    SAFE_RELEASE(pSceneRT);

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
                               (float)WINDOW_SIZE_W / WINDOW_SIZE_H,
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
    g_pEffect->SetFloat("g_FilterSpacing", g_filterSpacing);

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
