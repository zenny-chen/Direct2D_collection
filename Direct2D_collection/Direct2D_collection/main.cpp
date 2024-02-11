// main.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#define _USE_MATH_DEFINES
#include <math.h>
#include <cmath>
#include <utility>
#include <algorithm>

#include <Windows.h>
#include <d3d11_1.h>
#include <dxgi1_4.h>
#include <d2d1.h>
#include <d2d1_1.h>

// Maximum hardware adapter count
static constexpr UINT MAX_HARDWARE_ADAPTER_COUNT = 16U;

// Window Width
static constexpr int WINDOW_WIDTH = 512;

// Window Height
static constexpr int WINDOW_HEIGHT = 512;

// Default swap-chain buffer and render target buffer format
static constexpr DXGI_FORMAT RENDER_TARGET_BUFFER_FOMRAT = DXGI_FORMAT_R8G8B8A8_UNORM;

// Swap-chain back-buffer count
static UINT SWAPCHAIN_BACK_BUFFER_COUNT = 3U;

static D3D_FEATURE_LEVEL s_maxFeatureLevel = D3D_FEATURE_LEVEL_11_0;

static ID2D1Factory1* s_d2dFactory = nullptr;
static IDXGIFactory4* s_factory = nullptr;
static ID3D11Device1* s_d3d11Device = nullptr;
static ID3D11DeviceContext* s_d3d11DeviceContext = nullptr;
static IDXGIDevice* s_dxgiDevice = nullptr;
static ID2D1Device* s_d2d1Device = nullptr;
static ID2D1DeviceContext* s_d2d1Context = nullptr;
static IDXGISwapChain3* s_swapChain = nullptr;
static ID3D11RasterizerState1* s_rasterizerState = nullptr;
static IDXGISurface* s_dxgiBackBuffer = nullptr;
static ID2D1Bitmap1* s_d2dTargetBitmap = nullptr;
static ID2D1SolidColorBrush* s_solidColorBrush = nullptr;

static POINT s_wndMinsize {};

static auto TransWStrToString(char dstBuf[], const WCHAR srcBuf[]) -> void
{
    if (dstBuf == nullptr || srcBuf == nullptr) return;

    const int len = WideCharToMultiByte(CP_UTF8, 0, srcBuf, -1, nullptr, 0, nullptr, nullptr);
    WideCharToMultiByte(CP_UTF8, 0, srcBuf, -1, dstBuf, len, nullptr, nullptr);
    dstBuf[len] = '\0';
}

static auto CreateD2D1DeviceAndContext() -> bool
{
    const D2D1_FACTORY_OPTIONS options{
#if _DEBUG
        .debugLevel = D2D1_DEBUG_LEVEL_INFORMATION
#else
        .debugLevel = D2D1_DEBUG_LEVEL_ERROR
#endif
    };
    HRESULT hRes = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_ID2D1Factory1, &options, (void**)&s_d2dFactory);
    if (FAILED(hRes))
    {
        fprintf(stderr, "D2D1CreateFactory failed: %ld\n", hRes);
        return false;
    }

    hRes = CreateDXGIFactory1(IID_PPV_ARGS(&s_factory));
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateDXGIFactory1 failed: %ld\n", hRes);
        return false;
    }

    // Enumerate the adapters (video cards)
    IDXGIAdapter1* hardwareAdapters[MAX_HARDWARE_ADAPTER_COUNT]{ };
    UINT foundAdapterCount;
    for (foundAdapterCount = 0; foundAdapterCount < MAX_HARDWARE_ADAPTER_COUNT; ++foundAdapterCount)
    {
        hRes = s_factory->EnumAdapters1(foundAdapterCount, &hardwareAdapters[foundAdapterCount]);
        if (FAILED(hRes))
        {
            if (hRes != DXGI_ERROR_NOT_FOUND) {
                printf("WARNING: Some error occurred during enumerating adapters: %ld\n", hRes);
            }
            break;
        }
    }
    if (foundAdapterCount == 0)
    {
        fprintf(stderr, "There are no Direct3D capable adapters found on the current platform...\n");
        return false;
    }

    printf("Found %u Direct3D capable device%s in all.\n", foundAdapterCount, foundAdapterCount > 1 ? "s" : "");

    DXGI_ADAPTER_DESC1 adapterDesc{ };
    char strBuf[512]{ };
    for (UINT i = 0; i < foundAdapterCount; ++i)
    {
        hardwareAdapters[i]->GetDesc1(&adapterDesc);
        TransWStrToString(strBuf, adapterDesc.Description);
        printf("Adapter[%u]: %s\n", i, strBuf);
    }
    printf("Please Choose which adapter to use: ");

    gets_s(strBuf);

    char* endChar = nullptr;
    long selectedAdapterIndex = std::strtol(strBuf, &endChar, 10);
    if (selectedAdapterIndex < 0 || selectedAdapterIndex >= long(foundAdapterCount))
    {
        puts("WARNING: The index you input exceeds the range of available adatper count. So adatper[0] will be used!");
        selectedAdapterIndex = 0;
    }

    hardwareAdapters[selectedAdapterIndex]->GetDesc1(&adapterDesc);
    TransWStrToString(strBuf, adapterDesc.Description);

    printf("\nYou have chosen adapter[%ld]\n", selectedAdapterIndex);
    printf("Adapter description: %s\n", strBuf);
    printf("Dedicated Video Memory: %.1f GB\n", double(adapterDesc.DedicatedVideoMemory) / (1024.0 * 1024.0 * 1024.0));
    printf("Dedicated System Memory: %.1f GB\n", double(adapterDesc.DedicatedSystemMemory) / (1024.0 * 1024.0 * 1024.0));
    printf("Shared System Memory: %.1f GB\n", double(adapterDesc.SharedSystemMemory) / (1024.0 * 1024.0 * 1024.0));

    const D3D_FEATURE_LEVEL featureLevels[]{
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3
    };

    D3D_FEATURE_LEVEL retFeatureLevel = D3D_FEATURE_LEVEL_11_0;
    hRes = D3D11CreateDevice(hardwareAdapters[selectedAdapterIndex], D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT
#if _DEBUG
        | D3D11_CREATE_DEVICE_DEBUG
#endif
        , featureLevels, (UINT)std::size(featureLevels), D3D11_SDK_VERSION, (ID3D11Device**)& s_d3d11Device, & retFeatureLevel, & s_d3d11DeviceContext);
    if (FAILED(hRes))
    {
        fprintf(stderr, "D3D11CreateDevice failed: %ld\n", hRes);
        return false;
    }

    const char* featureStr = "Below D3D_FEATURE_LEVEL_9_3";
    switch (retFeatureLevel)
    {
    case D3D_FEATURE_LEVEL_11_1:
        featureStr = "D3D_FEATURE_LEVEL_11_1";
        break;

    case D3D_FEATURE_LEVEL_11_0:
        featureStr = "D3D_FEATURE_LEVEL_11_0";
        break;

    case D3D_FEATURE_LEVEL_10_1:
        featureStr = "D3D_FEATURE_LEVEL_10_1";
        break;

    case D3D_FEATURE_LEVEL_10_0:
        featureStr = "D3D_FEATURE_LEVEL_10_0";
        break;

    case D3D_FEATURE_LEVEL_9_3:
        featureStr = "D3D_FEATURE_LEVEL_9_3";
        break;

    default:
        break;
    }
    printf("Current device support: %s\n", featureStr);

    if (retFeatureLevel < D3D_FEATURE_LEVEL_11_1)
    {
        fprintf(stderr, "Current device does not support D3D Feature Level 11.1 so that ForcedSampleCount cannot be tested!!\n");
        return false;
    }

    puts("\n================================================\n");

    // Retrieve and create the IDXGIDevice
    hRes = s_d3d11Device->QueryInterface(IID_IDXGIDevice, (void**)&s_dxgiDevice);
    if (FAILED(hRes))
    {
        fprintf(stderr, "s_d3d11Device->QueryInterface failed: %ld\n", hRes);
        return false;
    }

    // ======== Create Direct2D Device ========
    hRes = s_d2dFactory->CreateDevice(s_dxgiDevice, &s_d2d1Device);
    if (FAILED(hRes))
    {
        fprintf(stderr, "s_d2dFactory->CreateDevice failed: %ld\n", hRes);
        return false;
    }

    // ======== Create D2D1 device context ========
    hRes = s_d2d1Device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &s_d2d1Context);
    if (FAILED(hRes))
    {
        fprintf(stderr, "s_d2d1Device->CreateDeviceContext failed: %ld\n", hRes);
        return false;
    }

    return true;
}

static auto CreateSwapChain(HWND hWnd) -> bool
{
    DXGI_SWAP_CHAIN_DESC swapChainDesc{
        .BufferDesc = {.Width = WINDOW_WIDTH, .Height = WINDOW_HEIGHT,
                        .RefreshRate = {.Numerator = 60, .Denominator = 1 },    // refresh rate of 60 FPS
                        .Format = RENDER_TARGET_BUFFER_FOMRAT,
                        .ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
                        .Scaling = DXGI_MODE_SCALING_UNSPECIFIED },
        .SampleDesc = {.Count = 1, .Quality = 0 },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = SWAPCHAIN_BACK_BUFFER_COUNT,
        .OutputWindow = hWnd,
        .Windowed = TRUE,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,    // Discard the contents of the back buffer, especially when MSAA is used.
        .Flags = 0
    };

    IDXGISwapChain* swapChain = nullptr;
    HRESULT hRes = s_factory->CreateSwapChain(s_d3d11Device, &swapChainDesc, &swapChain);
    if (FAILED(hRes) || swapChain == nullptr)
    {
        fprintf(stderr, "CreateSwapChain failed: %ld\n", hRes);
        return false;
    }

    s_swapChain = (IDXGISwapChain3*)swapChain;

    return true;
}

static auto CreateRasterizerStateObject() -> bool
{
    const D3D11_RASTERIZER_DESC1 raterizerDesc{
        .FillMode = D3D11_FILL_SOLID,
        .CullMode = D3D11_CULL_BACK,
        .FrontCounterClockwise = FALSE,
        .DepthBias = 0,
        .DepthBiasClamp = 0.0f,
        .SlopeScaledDepthBias = 0.0f,
        .DepthClipEnable = FALSE,
        .ScissorEnable = TRUE,
        .MultisampleEnable = FALSE,
        .AntialiasedLineEnable = FALSE,
        .ForcedSampleCount = 4U
    };
    HRESULT hRes = s_d3d11Device->CreateRasterizerState1(&raterizerDesc, &s_rasterizerState);
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateRasterizerState failed: %ld\n", hRes);
        return false;
    }

    s_d3d11DeviceContext->RSSetState(s_rasterizerState);

    const D3D11_RECT scissorRects[]{
        {
            .left = 0,
            .top = 0,
            .right = WINDOW_WIDTH,
            .bottom = WINDOW_HEIGHT
        }
    };
    s_d3d11DeviceContext->RSSetScissorRects((UINT)std::size(scissorRects), scissorRects);

    return true;
}

static auto CreateBitmapFromSurface() -> bool
{
    // Direct2D needs the dxgi version of the backbuffer surface pointer.
    HRESULT hRes = s_swapChain->GetBuffer(0U, IID_IDXGISurface, (void**)&s_dxgiBackBuffer);
    if (FAILED(hRes))
    {
        fprintf(stderr, "s_swapChain->GetBuffer failed: %ld\n", hRes);
        return false;
    }

    // Now we set up the Direct2D render target bitmap linked to the swapchain. 
    // Whenever we render to this bitmap, it will be directly rendered to the 
    // swapchain associated with the window.
    const D2D1_BITMAP_PROPERTIES1 bitmapProperties{
        .pixelFormat {.format = RENDER_TARGET_BUFFER_FOMRAT, .alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED },
        .dpiX = 96.0f,
        .dpiY = 96.0f,
        .bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        .colorContext = nullptr
    };

    // Get a D2D surface from the DXGI back buffer to use as the D2D render target.
    hRes = s_d2d1Context->CreateBitmapFromDxgiSurface(s_dxgiBackBuffer, &bitmapProperties, &s_d2dTargetBitmap);
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateBitmapFromDxgiSurface failed: %ld\n", hRes);
        return false;
    }

    // So now we can set the Direct2D render target.
    s_d2d1Context->SetTarget(s_d2dTargetBitmap);

    return true;
}

static auto CreateSolidBrush() -> bool
{
    HRESULT hRes = s_d2d1Context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &s_solidColorBrush);
    if (FAILED(hRes))
    {
        fprintf(stderr, "CreateSolidColorBrush failed: %ld\n", hRes);
        return false;
    }

    return true;
}

static auto DrawGraphics() -> void
{
    if (s_solidColorBrush == nullptr) return;

    s_d2d1Context->BeginDraw();

    s_solidColorBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
    s_d2d1Context->FillRectangle(D2D1::RectF(0.0f, 0.0f, float(WINDOW_WIDTH), float(WINDOW_HEIGHT)), s_solidColorBrush);

    s_solidColorBrush->SetColor(D2D1::ColorF(0.9f, 0.1f, 0.1f, 1.0f));
    s_d2d1Context->DrawRectangle(D2D1::RectF(50.0f, 50.0f, 250.0f, 250.0f), s_solidColorBrush);

    HRESULT hRes = s_d2d1Context->EndDraw();
    if (FAILED(hRes))
    {
        fprintf(stderr, "Draw Graphics failed: %ld\n", hRes);
        return;
    }

    const DXGI_PRESENT_PARAMETERS parameters{
        .DirtyRectsCount = 0U,
        .pDirtyRects = nullptr,
        .pScrollRect = nullptr,
        .pScrollOffset = nullptr
    };
    hRes = s_swapChain->Present1(1U, 0U, &parameters);
    if (FAILED(hRes))
    {
        fprintf(stderr, "Present1 failed: %ld\n", hRes);
        return;
    }
}

static auto DestroyAllResources() -> void
{
    if (s_solidColorBrush != nullptr)
    {
        s_solidColorBrush->Release();
        s_solidColorBrush = nullptr;
    }
    if (s_dxgiBackBuffer != nullptr)
    {
        s_dxgiBackBuffer->Release();
        s_dxgiBackBuffer = nullptr;
    }
    if (s_d2dTargetBitmap != nullptr)
    {
        s_d2dTargetBitmap->Release();
        s_d2dTargetBitmap = nullptr;
    }
    if (s_rasterizerState != nullptr)
    {
        s_rasterizerState->Release();
        s_rasterizerState = nullptr;
    }
    if (s_swapChain != nullptr)
    {
        s_swapChain->Release();
        s_swapChain = nullptr;
    }
    if (s_d2d1Context != nullptr)
    {
        s_d2d1Context->Release();
        s_d2d1Context = nullptr;
    }
    if (s_d3d11DeviceContext != nullptr)
    {
        s_d3d11DeviceContext->Release();
        s_d3d11DeviceContext = nullptr;
    }
    if (s_d3d11Device != nullptr)
    {
        s_d3d11Device->Release();
        s_d3d11Device = nullptr;
    }
    if (s_dxgiDevice != nullptr)
    {
        s_dxgiDevice->Release();
        s_dxgiDevice = nullptr;
    }
    if (s_factory != nullptr)
    {
        s_factory->Release();
        s_factory = nullptr;
    }
    if (s_d2d1Device != nullptr)
    {
        s_d2d1Device->Release();
        s_d2d1Device = nullptr;
    }
    if (s_d2dFactory != nullptr)
    {
        s_d2dFactory->Release();
        s_d2dFactory = nullptr;
    }
}

static auto CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) -> LRESULT
{
    bool displayParams = false;

    switch (uMsg)
    {
    case WM_CREATE:
    {
        RECT windowRect;
        GetWindowRect(hWnd, &windowRect);
        SetWindowLongA(hWnd, GWL_STYLE, GetWindowLongA(hWnd, GWL_STYLE) & ~WS_MINIMIZEBOX);
        SetWindowLongA(hWnd, GWL_STYLE, GetWindowLongA(hWnd, GWL_STYLE) & ~WS_MAXIMIZEBOX);
        SetWindowLongA(hWnd, GWL_STYLE, GetWindowLongA(hWnd, GWL_STYLE) & ~WS_SIZEBOX);
        break;
    }

    case WM_CLOSE:
        DestroyAllResources();
        PostQuitMessage(0);
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // TODO: Add any drawing code that uses hdc here...
        const RECT rect{ .left = 0, .top = 0, .right = WINDOW_WIDTH, .bottom = WINDOW_HEIGHT };
        FillRect(hdc, &rect, HBRUSH(COLOR_GRAYTEXT));

        DrawGraphics();

        EndPaint(hWnd, &ps);

        break;
    }

    case WM_GETMINMAXINFO:  // set window's minimum size
        ((MINMAXINFO*)lParam)->ptMinTrackSize = s_wndMinsize;
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_SIZE:
        // Resize the application to the new window size, except when
        // it was minimized.
        break;

    case WM_KEYDOWN:
        switch (wParam)
        {
        case VK_ESCAPE:
            PostQuitMessage(0);
            break;

        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case 'R':
        case 'L':
        case 'F':
        case 'N':
        case VK_SPACE:
        case VK_RETURN:
            break;

        default:
            break;
        }

        return 0;

    default:
        break;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static auto CreateAndInitializeWindow(HINSTANCE hInstance, LPCSTR appName, int windowWidth, int windowHeight) -> HWND
{
    WNDCLASSEXA win_class;
    // Initialize the window class structure:
    win_class.cbSize = sizeof(WNDCLASSEXA);
    win_class.style = CS_HREDRAW | CS_VREDRAW;
    win_class.lpfnWndProc = WndProc;
    win_class.cbClsExtra = 0;
    win_class.cbWndExtra = 0;
    win_class.hInstance = hInstance;
    win_class.hIcon = LoadIconA(nullptr, IDI_APPLICATION);
    win_class.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    win_class.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    win_class.lpszMenuName = nullptr;
    win_class.lpszClassName = appName;
    win_class.hIconSm = LoadIconA(nullptr, IDI_WINLOGO);
    // Register window class:
    if (!RegisterClassExA(&win_class))
    {
        // It didn't work, so try to give a useful error:
        printf("Unexpected error trying to start the application!\n");
        fflush(stdout);
        exit(1);
    }
    // Create window with the registered class:
    RECT wr = { 0, 0, windowWidth, windowHeight };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    const LONG windowStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_SYSMENU;

    HWND hWnd = CreateWindowExA(
        0,                            // extra style
        appName,                    // class name
        appName,                   // app name
        windowStyle,                   // window style
        CW_USEDEFAULT, CW_USEDEFAULT,     // x, y coords
        windowWidth,                    // width
        windowHeight,                  // height
        nullptr,                        // handle to parent
        nullptr,                            // handle to menu
        hInstance,                            // hInstance
        nullptr);

    if (hWnd == nullptr) {
        // It didn't work, so try to give a useful error:
        puts("Cannot create a window in which to draw!");
    }

    // Window client area size must be at least 1 pixel high, to prevent crash.
    s_wndMinsize.x = GetSystemMetrics(SM_CXMINTRACK);
    s_wndMinsize.y = GetSystemMetrics(SM_CYMINTRACK) + 1;

    return hWnd;
}

auto main() -> int
{
    HINSTANCE wndInstance = nullptr;
    HWND wndHandle = nullptr;

    bool needRender = false;

    do
    {
        if (!CreateD2D1DeviceAndContext()) break;

        // Windows Instance
        wndInstance = GetModuleHandleA(nullptr);

        // window handle
        wndHandle = CreateAndInitializeWindow(wndInstance, "Direct2D Collection", WINDOW_WIDTH, WINDOW_HEIGHT);

        if (!CreateSwapChain(wndHandle)) break;
        if (!CreateRasterizerStateObject()) break;
        if (!CreateBitmapFromSurface()) break;
        if (!CreateSolidBrush()) break;

        needRender = true;
    }
    while (false);

    if (needRender)
    {
        // main message loop
        MSG msg{ };
        bool done = false;
        while (!done)
        {
            PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE);
            // check for a quit message
            if (msg.message == WM_QUIT) {
                done = true;  // if found, quit app
            }
            else
            {
                // Translate and dispatch to event queue
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
            RedrawWindow(wndHandle, nullptr, nullptr, RDW_INTERNALPAINT);
        }
    }

    if (wndHandle != nullptr)
    {
        DestroyWindow(wndHandle);
        wndHandle = nullptr;
    }
    
    DestroyAllResources();
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件

