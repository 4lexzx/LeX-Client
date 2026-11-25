// Aloja LeXClient.dll en una ventana propia y guarda una captura.
// lex-preview <dx11|dx12> <salida.bmp> <hud|menu|settings|edit> [carpeta config]
#include <windows.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

constexpr UINT W = 1280, H = 720;

static void writeBmp(const char* path, const unsigned char* bgra) {
  FILE* f = fopen(path, "wb");
  if (!f) return;
  BITMAPFILEHEADER fh{};
  BITMAPINFOHEADER ih{};
  fh.bfType = 0x4D42;
  fh.bfOffBits = sizeof(fh) + sizeof(ih);
  fh.bfSize = fh.bfOffBits + W * H * 4;
  ih.biSize = sizeof(ih);
  ih.biWidth = W;
  ih.biHeight = -static_cast<LONG>(H);
  ih.biPlanes = 1;
  ih.biBitCount = 32;
  fwrite(&fh, sizeof(fh), 1, f);
  fwrite(&ih, sizeof(ih), 1, f);
  fwrite(bgra, W * H * 4, 1, f);
  fclose(f);
}

int main(int argc, char** argv) {
  if (argc < 4) return 1;
  const bool dx12 = !strcmp(argv[1], "dx12");
  const std::string mode = argv[3];
  SetProcessDPIAware();
  WNDCLASSW wc{};
  wc.lpfnWndProc = DefWindowProcW;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = L"LexPreview";
  RegisterClassW(&wc);
  HWND hwnd = CreateWindowExW(WS_EX_NOACTIVATE, L"LexPreview", L"LeX preview", WS_POPUP, 0, 0, W, H, nullptr, nullptr, wc.hInstance, nullptr);
  ShowWindow(hwnd, SW_SHOWNOACTIVATE);

  IDXGISwapChain* chain = nullptr;
  ID3D11Device* d11 = nullptr;
  ID3D11DeviceContext* c11 = nullptr;
  ID3D12Device* d12 = nullptr;
  ID3D12CommandQueue* queue = nullptr;
  ID3D12CommandAllocator* alloc = nullptr;
  ID3D12GraphicsCommandList* list = nullptr;
  ID3D12Fence* fence = nullptr;
  ID3D12DescriptorHeap* heap = nullptr;
  UINT64 value = 0;
  if (!dx12) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = W;
    sd.BufferDesc.Height = H;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &chain, &d11, nullptr, &c11))) return 2;
  } else {
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d12)))) return 2;
    D3D12_COMMAND_QUEUE_DESC qd{};
    d12->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue));
    d12->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
    d12->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, nullptr, IID_PPV_ARGS(&list));
    list->Close();
    d12->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    D3D12_DESCRIPTOR_HEAP_DESC hd{};
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    hd.NumDescriptors = 2;
    d12->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap));
    IDXGIFactory4* factory = nullptr;
    CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width = W;
    sd.Height = H;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferCount = 2;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    IDXGISwapChain1* s1 = nullptr;
    if (FAILED(factory->CreateSwapChainForHwnd(queue, hwnd, &sd, nullptr, nullptr, &s1))) return 2;
    chain = s1;
    factory->Release();
  }

  HMODULE dll = LoadLibraryW(L"LeXClient.dll");
  if (!dll) {
    printf("LoadLibrary %lu\n", GetLastError());
    return 3;
  }
  const auto fn = [&](const char* n) { return reinterpret_cast<void*>(GetProcAddress(dll, n)); };
  if (!reinterpret_cast<bool (*)(const char*, const char*)>(fn("lexInitialize"))(argc > 4 ? argv[4] : "preview-config", "client")) return 4;
  if (mode != "hud") reinterpret_cast<void (*)()>(fn("lexToggleGui"))();
  const auto tab = reinterpret_cast<void (*)(int)>(fn("lexSelectTab"));
  tab(mode == "cosmetics" ? 3 : 6);
  if (mode == "settings") reinterpret_cast<void (*)(const char*)>(fn("lexSelectModule"))(argc > 5 ? argv[5] : "Teclas");

  const auto waitGpu = [&] {
    queue->Signal(fence, ++value);
    while (fence->GetCompletedValue() < value) Sleep(1);
  };
  for (int i = 0; i < 180; ++i) {
    MSG m;
    while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&m);
      DispatchMessageW(&m);
    }
    if (i == 90 && mode == "edit") reinterpret_cast<void (*)(bool)>(fn("lexEditHud"))(true);
    if (i == 120) {  // redimensionado real: falla si el cliente retiene el backbuffer
      if (dx12) waitGpu();
      else c11->ClearState();
      const HRESULT hr = chain->ResizeBuffers(2, W, H, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
      printf("ResizeBuffers 0x%lX\n", static_cast<unsigned long>(hr));
      if (FAILED(hr)) return 5;
    }
    const float sky[4] = {0.33f, 0.52f, 0.66f, 1};
    if (!dx12) {
      ID3D11Texture2D* bb;
      ID3D11RenderTargetView* rtv;
      chain->GetBuffer(0, IID_PPV_ARGS(&bb));
      d11->CreateRenderTargetView(bb, nullptr, &rtv);
      c11->ClearRenderTargetView(rtv, sky);
      rtv->Release();
      bb->Release();
    } else {
      IDXGISwapChain3* s3;
      chain->QueryInterface(IID_PPV_ARGS(&s3));
      const UINT index = s3->GetCurrentBackBufferIndex();
      s3->Release();
      ID3D12Resource* bb;
      chain->GetBuffer(index, IID_PPV_ARGS(&bb));
      alloc->Reset();
      list->Reset(alloc, nullptr);
      auto h = heap->GetCPUDescriptorHandleForHeapStart();
      h.ptr += SIZE_T(index) * d12->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
      d12->CreateRenderTargetView(bb, nullptr, h);
      D3D12_RESOURCE_BARRIER b{};
      b.Transition.pResource = bb;
      b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
      b.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
      b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
      list->ResourceBarrier(1, &b);
      list->ClearRenderTargetView(h, sky, 0, nullptr);
      std::swap(b.Transition.StateBefore, b.Transition.StateAfter);
      list->ResourceBarrier(1, &b);
      list->Close();
      ID3D12CommandList* lists[] = {list};
      queue->ExecuteCommandLists(1, lists);
      bb->Release();
    }
    chain->Present(1, 0);
    if (dx12) waitGpu();
  }
  // PW_RENDERFULLCONTENT: captura lo presentado, no el backbuffer interno.
  HDC dc = GetDC(hwnd);
  HDC mem = CreateCompatibleDC(dc);
  BITMAPINFO bi{};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = W;
  bi.bmiHeader.biHeight = -static_cast<LONG>(H);
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  void* bits = nullptr;
  HBITMAP hb = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  SelectObject(mem, hb);
  PrintWindow(hwnd, mem, 1 | 2);
  GdiFlush();
  writeBmp(argv[2], static_cast<unsigned char*>(bits));
  printf("fps %.1f iconos %d\n", reinterpret_cast<float (*)()>(fn("lexFps"))(), reinterpret_cast<int (*)()>(fn("lexIconCount"))());
  reinterpret_cast<void (*)()>(fn("lexShutdown"))();
  puts("shutdown ok");
  return 0;
}
