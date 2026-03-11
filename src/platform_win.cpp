#ifdef _WIN32

// Force dedicated GPU on laptops
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement                = 1;
    __declspec(dllexport) unsigned long AmdPowerXpressRequestHighPerformance = 1;
}

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <wrl/client.h>

// DirectX / DirectComposition
#include <dxgi1_3.h>
#include <d3d11.h>
#include <d2d1_2.h>
#include <d2d1_2helper.h>
#include <dcomp.h>
#include <wincodec.h>   // WIC — for loading PNG

#pragma comment(lib, "user32")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d2d1")
#pragma comment(lib, "dcomp")
#pragma comment(lib, "windowscodecs")

#include "app.h"

using Microsoft::WRL::ComPtr;

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────
static void Check(HRESULT hr) {
    if (FAILED(hr)) {
        wchar_t msg[256];
        swprintf_s(msg, L"DirectX error: 0x%08X", (unsigned)hr);
        MessageBoxW(nullptr, msg, L"DesktopPet Error", MB_ICONERROR);
        ExitProcess(1);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Renderer — owns all D3D/D2D/DComp objects
// ─────────────────────────────────────────────────────────────────────────────
struct Renderer {
    ComPtr<ID3D11Device>         d3dDevice;
    ComPtr<IDXGIDevice>          dxgiDevice;
    ComPtr<IDXGIFactory2>        dxgiFactory;
    ComPtr<IDXGISwapChain1>      swapChain;
    ComPtr<ID2D1Factory2>        d2Factory;
    ComPtr<ID2D1Device1>         d2Device;
    ComPtr<ID2D1DeviceContext>   dc;
    ComPtr<IDCompositionDevice>  dcompDevice;
    ComPtr<IDCompositionTarget>  dcompTarget;
    ComPtr<IDCompositionVisual>  dcompVisual;

    // Sprite sheet bitmap
    ComPtr<ID2D1Bitmap>          spriteBitmap;
    int                          sheetW = 0, sheetH = 0;

    UINT   width  = 0;
    UINT   height = 0;

    void init(HWND hwnd, UINT w, UINT h) {
        width = w; height = h;

        // ── Direct3D device ──────────────────────────────────────────────────
        D3D_FEATURE_LEVEL featureLevel;
        Check(D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr, 0,
            D3D11_SDK_VERSION,
            &d3dDevice,
            &featureLevel,
            nullptr
        ));

        Check(d3dDevice.As(&dxgiDevice));

        // ── DXGI factory ────────────────────────────────────────────────────
        Check(CreateDXGIFactory2(0,
            __uuidof(IDXGIFactory2),
            reinterpret_cast<void**>(dxgiFactory.GetAddressOf())
        ));

        // ── Composition swap chain (no redirection surface, premul alpha) ───
        DXGI_SWAP_CHAIN_DESC1 desc = {};
        desc.Width              = width;
        desc.Height             = height;
        desc.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.BufferCount        = 2;
        desc.SampleDesc.Count   = 1;
        desc.AlphaMode          = DXGI_ALPHA_MODE_PREMULTIPLIED;  // ← key

        Check(dxgiFactory->CreateSwapChainForComposition(
            dxgiDevice.Get(), &desc, nullptr, &swapChain
        ));

        // ── Direct2D ────────────────────────────────────────────────────────
        D2D1_FACTORY_OPTIONS opts = {};
        Check(D2D1CreateFactory(
            D2D1_FACTORY_TYPE_SINGLE_THREADED, opts, d2Factory.GetAddressOf()
        ));

        Check(d2Factory->CreateDevice(dxgiDevice.Get(), &d2Device));
        Check(d2Device->CreateDeviceContext(
            D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc
        ));

        // Point D2D device context at swap chain back buffer
        bindBackBuffer();

        // ── DirectComposition ────────────────────────────────────────────────
        Check(DCompositionCreateDevice(
            dxgiDevice.Get(),
            __uuidof(IDCompositionDevice),
            reinterpret_cast<void**>(dcompDevice.GetAddressOf())
        ));

        Check(dcompDevice->CreateTargetForHwnd(hwnd, TRUE, &dcompTarget));
        Check(dcompDevice->CreateVisual(&dcompVisual));
        Check(dcompVisual->SetContent(swapChain.Get()));
        Check(dcompTarget->SetRoot(dcompVisual.Get()));
        Check(dcompDevice->Commit());
    }

    void bindBackBuffer() {
        // Get the swap chain's back buffer as a DXGI surface
        ComPtr<IDXGISurface2> surface;
        Check(swapChain->GetBuffer(
            0, __uuidof(IDXGISurface2),
            reinterpret_cast<void**>(surface.GetAddressOf())
        ));

        // Create a D2D bitmap targeting that surface
        D2D1_BITMAP_PROPERTIES1 bmpProps = {};
        bmpProps.pixelFormat.format    = DXGI_FORMAT_B8G8R8A8_UNORM;
        bmpProps.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
        bmpProps.bitmapOptions         = D2D1_BITMAP_OPTIONS_TARGET |
                                         D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
        // Use screen DPI
        FLOAT dpiX, dpiY;
        d2Factory->GetDesktopDpi(&dpiX, &dpiY);
        bmpProps.dpiX = dpiX;
        bmpProps.dpiY = dpiY;

        ComPtr<ID2D1Bitmap1> backBitmap;
        Check(dc->CreateBitmapFromDxgiSurface(surface.Get(), bmpProps, &backBitmap));
        dc->SetTarget(backBitmap.Get());
    }

    // Load the sprite sheet PNG using WIC
    void loadSpriteSheet(const wchar_t* path) {
        ComPtr<IWICImagingFactory> wic;
        Check(CoCreateInstance(
            CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&wic)
        ));

        ComPtr<IWICBitmapDecoder> decoder;
        Check(wic->CreateDecoderFromFilename(
            path, nullptr, GENERIC_READ,
            WICDecodeMetadataCacheOnLoad, &decoder
        ));

        ComPtr<IWICBitmapFrameDecode> frame;
        Check(decoder->GetFrame(0, &frame));

        ComPtr<IWICFormatConverter> converter;
        Check(wic->CreateFormatConverter(&converter));
        Check(converter->Initialize(
            frame.Get(),
            GUID_WICPixelFormat32bppPBGRA,  // premultiplied BGRA — matches D2D
            WICBitmapDitherTypeNone,
            nullptr, 0.0,
            WICBitmapPaletteTypeMedianCut
        ));

        UINT w, h;
        Check(frame->GetSize(&w, &h));
        sheetW = (int)w;
        sheetH = (int)h;

        Check(dc->CreateBitmapFromWicBitmap(converter.Get(), nullptr, &spriteBitmap));
    }

    // Draw one frame: clear to fully transparent, then draw the sprite frame
    void drawFrame(const FrameInfo& fi) {
        dc->BeginDraw();
        // Clear to transparent — alpha=0, this is what DComp composites away
        dc->Clear(D2D1::ColorF(0.f, 0.f, 0.f, 0.f));

        if (spriteBitmap) {
            // Source rect: one frame inside the sprite sheet
            D2D1_RECT_F src = D2D1::RectF(
                static_cast<float>(fi.col * FRAME_W),
                static_cast<float>(fi.row * FRAME_H),
                static_cast<float>(fi.col * FRAME_W + FRAME_W),
                static_cast<float>(fi.row * FRAME_H + FRAME_H)
            );
            // Destination rect: scaled 2x on screen
            D2D1_RECT_F dst = D2D1::RectF(
                fi.x,
                fi.y,
                fi.x + SPRITE_W,
                fi.y + SPRITE_H
            );
            dc->DrawBitmap(spriteBitmap.Get(), dst, 1.0f,
                D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, src);
        }

        Check(dc->EndDraw());
        // Present — sync interval 1 = vsync
        Check(swapChain->Present(1, 0));
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Win32 window procedure
// ─────────────────────────────────────────────────────────────────────────────
static bool  g_running = true;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_DESTROY:
            g_running = false;
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) {
                g_running = false;
                PostQuitMessage(0);
            }
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ─────────────────────────────────────────────────────────────────────────────
//  WinMain entry point
// ─────────────────────────────────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    CoInitialize(nullptr);  // needed for WIC

    // ── Get screen size ──────────────────────────────────────────────────────
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    // ── Register window class ────────────────────────────────────────────────
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = L"DesktopPetClass";
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    // ── Create window ────────────────────────────────────────────────────────
    // WS_EX_NOREDIRECTIONBITMAP — no opaque redirection surface
    //   → DComp provides the surface directly to the compositor
    // WS_EX_TOPMOST             — always on top
    // WS_EX_TRANSPARENT         — click-through
    HWND hwnd = CreateWindowExW(
        WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOPMOST | WS_EX_TRANSPARENT,
        L"DesktopPetClass",
        L"DesktopPet",
        WS_POPUP,
        0, 0, screenW, screenH,
        nullptr, nullptr, hInstance, nullptr
    );

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // ── Init renderer ────────────────────────────────────────────────────────
    Renderer renderer;
    renderer.init(hwnd, (UINT)screenW, (UINT)screenH);

    // Build path to sprite sheet relative to exe
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    // Replace filename with res\pet_image.png
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *(lastSlash + 1) = L'\0';
    wcscat_s(exePath, L"res\\pet_image.png");
    renderer.loadSpriteSheet(exePath);

    // ── Init game logic ──────────────────────────────────────────────────────
    PetApp pet;
    pet.init((float)screenW, (float)screenH);

    // ── Timer setup ──────────────────────────────────────────────────────────
    LARGE_INTEGER freq, prev, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);

    // ── Main loop ────────────────────────────────────────────────────────────
    MSG msg = {};
    while (g_running) {
        // Drain all pending messages
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        // Delta time
        QueryPerformanceCounter(&now);
        float dt = static_cast<float>(now.QuadPart - prev.QuadPart)
                 / static_cast<float>(freq.QuadPart);
        prev = now;
        if (dt > 0.05f) dt = 0.05f;  // clamp to avoid spiral of death

        // Mouse position in screen coordinates
        POINT cursor;
        GetCursorPos(&cursor);

        // Update game logic
        FrameInfo fi = pet.update(dt,
            static_cast<float>(cursor.x),
            static_cast<float>(cursor.y)
        );

        // Render
        renderer.drawFrame(fi);
    }

    CoUninitialize();
    return 0;
}

#endif // _WIN32
