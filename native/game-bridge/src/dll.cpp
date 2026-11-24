// LEX CLIENT — Puente de juego inyectado en Minecraft Bedrock.
// Autorizado por el usuario: inyección/hook para FPS reales y efectos de render.
// Engancha IDXGISwapChain::Present (vtable 8) y las creaciones de swapchain
// (D3D11CreateDeviceAndSwapChain / IDXGIFactory::CreateSwapChain*).
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include "MinHook.h"

namespace {

// ---------- estado global ----------
volatile LONG64 g_frames = 0;
volatile LONG64 g_fps = 0;
volatile LONG64 g_lastWrite = 0;
volatile LONG64 g_windowStart = 0;
volatile LONG64 g_framesAtWindow = 0;
volatile LONG g_presentHooked = 0;
IDXGISwapChain* g_swap = nullptr;
IDXGISwapChain* g_dummySwap = nullptr;   // swapchain propio para derivar la vtable de Present
ID3D11Device* g_dev11 = nullptr;
volatile LONG g_realSwapCaptured = 0;
const char* g_api = "unknown";

// Hooks de usuario dentro del juego (necesarios para soltar el puntero del
// menú modal: ClipCursor/ShowCursor son estado por hilo, no globales).
typedef BOOL(WINAPI* ClipCursor_t)(const RECT*);
typedef INT(WINAPI* ShowCursor_t)(BOOL);
typedef HWND(WINAPI* SetCapture_t)(HWND);
ClipCursor_t fpClipCursor = nullptr;
ShowCursor_t fpShowCursor = nullptr;
SetCapture_t fpSetCapture = nullptr;

// toggles leídos de modules.json (se refrescan cada ~250 ms)
volatile LONG g_motionBlur = 0;
volatile LONG g_fog = 0;
volatile LONG g_colors = 0;
volatile LONG g_zoom = 0;
volatile LONG g_uiMenu = 0;   // menú modal F8 del overlay (ui-state.json)

// ---------- logging ----------
std::string appDataPath() {
  const char* p = std::getenv("APPDATA");
  return p ? std::string(p) : std::string("C:\\Temp");
}

static std::string leafPath(const char* leaf) {
  std::string s = appDataPath() + "\\lex-client\\";
  CreateDirectoryA((s).c_str(), nullptr);
  return s + leaf;
}

void logLine(const char* msg) {
  FILE* f = fopen(leafPath("game-bridge.log").c_str(), "a");
  if (!f) return;
  fprintf(f, "%llu %s\n", (unsigned long long)GetTickCount64(), msg);
  fclose(f);
}

long long epochMs() {
  FILETIME ft{};
  GetSystemTimeAsFileTime(&ft);
  ULARGE_INTEGER u{};
  u.LowPart = ft.dwLowDateTime;
  u.HighPart = ft.dwHighDateTime;
  // Unix epoch ms (el overlay compara contra system_clock, no FILETIME).
  return (long long)(u.QuadPart / 10000ULL) - 11644473600000LL;
}

// ---------- lectura de toggles del catálogo ----------
bool moduleEnabled(const std::string& json, const std::string& id) {
  const std::string key = "\"" + id + "\"";
  size_t p = json.find(key);
  if (p == std::string::npos) return false;
  p = json.find("\"enabled\"", p);
  if (p == std::string::npos || p > json.size()) return false;
  p = json.find(':', p);
  if (p == std::string::npos) return false;
  ++p;
  while (p < json.size() && (json[p] == ' ' || json[p] == '\r' || json[p] == '\n' || json[p] == '\t')) ++p;
  return json.compare(p, 4, "true") == 0;
}

void refreshToggles() {
  FILE* f = fopen(leafPath("modules.json").c_str(), "rb");
  if (!f) return;
  std::string json;
  char buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) json.append(buf, n);
  fclose(f);
  InterlockedExchange(&g_motionBlur, moduleEnabled(json, "visual-motionblur") ? 1 : 0);
  InterlockedExchange(&g_fog, moduleEnabled(json, "visual-fog") ? 1 : 0);
  InterlockedExchange(&g_colors, moduleEnabled(json, "visual-colors") ? 1 : 0);
  InterlockedExchange(&g_zoom, moduleEnabled(json, "visual-zoom") ? 1 : 0);
}

bool jsonBool(const std::string& json, const std::string& key, bool fallback) {
  const std::string needle = "\"" + key + "\"";
  size_t p = json.find(needle);
  if (p == std::string::npos) return fallback;
  p = json.find(':', p + needle.size());
  if (p == std::string::npos) return fallback;
  ++p;
  while (p < json.size() && (json[p] == ' ' || json[p] == '\r' || json[p] == '\n' || json[p] == '\t')) ++p;
  if (json.compare(p, 4, "true") == 0) return true;
  if (json.compare(p, 5, "false") == 0) return false;
  return fallback;
}

void refreshUiState() {
  FILE* f = fopen(leafPath("ui-state.json").c_str(), "rb");
  if (!f) return;
  std::string json;
  char buf[2048];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) json.append(buf, n);
  fclose(f);

  const bool menu = jsonBool(json, "menu", false) || jsonBool(json, "hudEdit", false);
  const bool was = InterlockedCompareExchange(&g_uiMenu, 0, 0) != 0;
  InterlockedExchange(&g_uiMenu, menu ? 1 : 0);
  if (menu == was) return;

  // Flarial libera el cursor dentro del hilo del juego: ClipCursor(NULL),
  // SetCapture(NULL) y ShowCursor(TRUE). Desde otro proceso no funciona.
  if (menu) {
    if (fpClipCursor) fpClipCursor(nullptr);
    ReleaseCapture();
    if (fpShowCursor) for (int i = 0; i < 4; ++i) fpShowCursor(TRUE);
    SetCursor(LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW));
    logLine("menu LEX abierto: cursor liberado");
  } else {
    if (fpShowCursor) for (int i = 0; i < 4; ++i) fpShowCursor(FALSE);
    logLine("menu LEX cerrado: cursor devuelto al juego");
  }
}

void writeStatus() {
  refreshToggles();
  refreshUiState();
  const std::string path = leafPath("game-bridge.json");
  const std::string tmp = path + ".tmp";
  FILE* f = fopen(tmp.c_str(), "w");
  if (!f) return;
  fprintf(f, "{\n  \"fps\": %lld,\n  \"api\": \"%s\",\n  \"frames\": %lld,\n"
             "  \"blur\": %ld,\n  \"fog\": %ld,\n  \"colors\": %ld,\n  \"zoom\": %ld,\n"
             "  \"timestamp\": %lld\n}\n",
          (long long)g_fps, g_api, (long long)g_frames,
          InterlockedCompareExchange(&g_motionBlur, 0, 0),
          InterlockedCompareExchange(&g_fog, 0, 0),
          InterlockedCompareExchange(&g_colors, 0, 0),
          InterlockedCompareExchange(&g_zoom, 0, 0),
          epochMs());
  fclose(f);
  MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
}

// ---------- shader de post-proceso (blur/color/fog/zoom) ----------
struct Params { float blur, sat, fog, zoom; };

struct Render11 {
  ID3D11Device* dev = nullptr;
  ID3D11DeviceContext* ctx = nullptr;
  ID3D11Buffer* cbuf = nullptr;
  ID3D11VertexShader* vs = nullptr;
  ID3D11PixelShader* ps = nullptr;
  ID3D11SamplerState* samp = nullptr;
  ID3D11Texture2D* copy = nullptr;
  ID3D11ShaderResourceView* srv = nullptr;
  ID3D11RenderTargetView* rtv = nullptr;
  UINT w = 0, h = 0;
  DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
  bool ready = false;
  bool failed = false;
} g_r11;

static const char* kShaderSrc = R"HLSL(
struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
VSOut VS(uint id : SV_VertexID) {
  float2 p = float2(id == 1 ? 3.0 : -1.0, id == 2 ? 3.0 : -1.0);
  VSOut o;
  o.pos = float4(p, 0.0, 1.0);
  o.uv = float2(p.x * 0.5 + 0.5, 0.5 - p.y * 0.5);
  return o;
}
Texture2D tex : register(t0);
SamplerState samp : register(s0);
cbuffer Params : register(b0) { float blur; float sat; float fog; float zoom; };
float4 PS(VSOut i) : SV_Target {
  float2 uv = (i.uv - 0.5) / max(zoom, 0.01) + 0.5;
  float3 c;
  if (blur > 0.01) {
    float2 px = float2(blur / 1280.0, blur / 720.0);
    c  = tex.Sample(samp, uv).rgb * 0.36;
    c += tex.Sample(samp, uv + float2( px.x, 0)).rgb * 0.16;
    c += tex.Sample(samp, uv + float2(-px.x, 0)).rgb * 0.16;
    c += tex.Sample(samp, uv + float2(0,  px.y)).rgb * 0.16;
    c += tex.Sample(samp, uv + float2(0, -px.y)).rgb * 0.16;
  } else {
    c = tex.Sample(samp, uv).rgb;
  }
  float lum = dot(c, float3(0.299, 0.587, 0.114));
  c = lerp(float3(lum, lum, lum), c, sat);
  if (fog > 0.01) {
    float f = saturate((uv.y - 0.25) * 1.7) * fog;
    c = lerp(c, float3(0.42, 0.60, 0.78), f * 0.6);
  }
  return float4(c, 1.0);
}
)HLSL";

bool compileShader(const char* src, size_t len, const char* entry, const char* target, ID3DBlob** out) {
  ID3DBlob* err = nullptr;
  HRESULT hr = D3DCompile(src, len, nullptr, nullptr, nullptr, entry, target, 0, 0, out, &err);
  if (FAILED(hr) && err) {
    char msg[512];
    snprintf(msg, sizeof(msg), "D3DCompile %s: %s", entry, (const char*)err->GetBufferPointer());
    logLine(msg);
  }
  if (err) err->Release();
  return SUCCEEDED(hr);
}

void dropRender11() {
  if (g_r11.srv) g_r11.srv->Release();
  if (g_r11.copy) g_r11.copy->Release();
  if (g_r11.rtv) g_r11.rtv->Release();
  g_r11.srv = nullptr;
  g_r11.copy = nullptr;
  g_r11.rtv = nullptr;
  g_r11.ready = false;
}

void initRender11(IDXGISwapChain* sw) {
  if (g_r11.failed || g_r11.ready) return;
  ID3D11Texture2D* bb = nullptr;
  if (FAILED(sw->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb))) return;
  D3D11_TEXTURE2D_DESC desc{};
  bb->GetDesc(&desc);
  g_r11.w = desc.Width;
  g_r11.h = desc.Height;
  g_r11.fmt = desc.Format;

  ID3D11Device* dev = nullptr;
  if (FAILED(sw->GetDevice(__uuidof(ID3D11Device), (void**)&dev))) { bb->Release(); return; }
  g_r11.dev = dev;
  dev->GetImmediateContext(&g_r11.ctx);

  ID3DBlob* vsb = nullptr, * psb = nullptr;
  const bool compiled = compileShader(kShaderSrc, strlen(kShaderSrc), "VS", "vs_5_0", &vsb) &&
                        compileShader(kShaderSrc, strlen(kShaderSrc), "PS", "ps_5_0", &psb);
  if (compiled) {
    dev->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &g_r11.vs);
    dev->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &g_r11.ps);
  }
  if (vsb) vsb->Release();
  if (psb) psb->Release();

  D3D11_BUFFER_DESC cbDesc{};
  cbDesc.ByteWidth = sizeof(Params);
  cbDesc.Usage = D3D11_USAGE_DYNAMIC;
  cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  dev->CreateBuffer(&cbDesc, nullptr, &g_r11.cbuf);

  D3D11_SAMPLER_DESC sd{};
  sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
  dev->CreateSamplerState(&sd, &g_r11.samp);

  // Copia del backbuffer con SRV (el backbuffer de la swapchain no lo permite).
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  desc.CPUAccessFlags = 0;
  desc.MiscFlags = 0;
  if (SUCCEEDED(dev->CreateTexture2D(&desc, nullptr, &g_r11.copy))) {
    dev->CreateShaderResourceView(g_r11.copy, nullptr, &g_r11.srv);
  }
  if (FAILED(dev->CreateRenderTargetView(bb, nullptr, &g_r11.rtv))) logLine("CreateRenderTargetView falló");
  bb->Release();

  g_r11.ready = g_r11.vs && g_r11.ps && g_r11.cbuf && g_r11.samp && g_r11.srv && g_r11.rtv && g_r11.ctx;
  if (!g_r11.ready) logLine("post-proceso D3D11 no disponible (FPS seguirá funcionando)");
}

void applyEffects(IDXGISwapChain* sw) {
  if (!g_r11.dev || !sw) return;
  ID3D11Texture2D* bb = nullptr;
  if (FAILED(sw->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb))) return;
  D3D11_TEXTURE2D_DESC desc{};
  bb->GetDesc(&desc);
  if (!g_r11.ready || desc.Width != g_r11.w || desc.Height != g_r11.h || desc.Format != g_r11.fmt) {
    dropRender11();
    initRender11(sw);
  }
  const float blur = InterlockedCompareExchange(&g_motionBlur, 0, 0) ? 7.f : 0.f;
  const float sat = InterlockedCompareExchange(&g_colors, 0, 0) ? 1.35f : 1.f;
  const float fog = InterlockedCompareExchange(&g_fog, 0, 0) ? 1.f : 0.f;
  const float zoom = InterlockedCompareExchange(&g_zoom, 0, 0) ? 1.35f : 1.f;
  if (!g_r11.ready || (blur == 0 && sat == 1 && fog == 0 && zoom == 1)) { bb->Release(); return; }

  ID3D11DeviceContext* ctx = g_r11.ctx;
  ctx->CopyResource(g_r11.copy, bb);
  D3D11_MAPPED_SUBRESOURCE ms{};
  if (SUCCEEDED(ctx->Map(g_r11.cbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
    Params p{blur, sat, fog, zoom};
    memcpy(ms.pData, &p, sizeof(p));
    ctx->Unmap(g_r11.cbuf, 0);
  }
  D3D11_VIEWPORT vp{0.f, 0.f, (float)g_r11.w, (float)g_r11.h, 0.f, 1.f};
  ctx->RSSetViewports(1, &vp);
  ctx->OMSetRenderTargets(1, &g_r11.rtv, nullptr);
  ctx->VSSetShader(g_r11.vs, nullptr, 0);
  ctx->PSSetShader(g_r11.ps, nullptr, 0);
  ctx->PSSetConstantBuffers(0, 1, &g_r11.cbuf);
  ctx->PSSetShaderResources(0, 1, &g_r11.srv);
  ctx->PSSetSamplers(0, 1, &g_r11.samp);
  ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  ctx->Draw(3, 0);
  ctx->PSSetShaderResources(0, 1, nullptr);
  ctx->OMSetRenderTargets(0, nullptr, nullptr);
  bb->Release();
}

// ---------- hooks ----------
typedef HRESULT(STDMETHODCALLTYPE* Present_t)(IDXGISwapChain*, UINT, UINT);
typedef HRESULT(STDMETHODCALLTYPE* ResizeBuffers_t)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
typedef HRESULT(STDMETHODCALLTYPE* CreateSwapChainForHwnd_t)(IDXGIFactory2*, HWND, const DXGI_SWAP_CHAIN_DESC1*,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**);
typedef HRESULT(STDMETHODCALLTYPE* CreateSwapChainForCoreWindow_t)(IDXGIFactory2*, IUnknown*,
    const DXGI_SWAP_CHAIN_DESC1*, IDXGIOutput*, IDXGISwapChain1**);
typedef HRESULT(STDMETHODCALLTYPE* CreateSwapChainForComposition_t)(IDXGIFactory2*, IUnknown*,
    const DXGI_SWAP_CHAIN_DESC1*, IDXGIOutput*, IDXGISwapChain1**);
typedef HRESULT(STDMETHODCALLTYPE* CreateSwapChain_t)(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
typedef HRESULT(WINAPI* CreateDXGIFactory2_t)(UINT, REFIID, void**);
typedef HRESULT(WINAPI* CreateDXGIFactory1_t)(REFIID, void**);
typedef HRESULT(WINAPI* CreateDXGIFactory_t)(REFIID, void**);
typedef HRESULT(WINAPI* D3D11CreateDeviceAndSwapChain_t)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT,
    const D3D_FEATURE_LEVEL*, UINT, UINT, const DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**,
    ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

Present_t fpPresent = nullptr;
ResizeBuffers_t fpResizeBuffers = nullptr;
CreateSwapChainForHwnd_t fpCreateForHwnd = nullptr;
CreateSwapChainForCoreWindow_t fpCreateForCore = nullptr;
CreateSwapChainForComposition_t fpCreateForComposition = nullptr;
CreateSwapChain_t fpCreateSwapChain = nullptr;
CreateDXGIFactory2_t fpCreateFactory2 = nullptr;
CreateDXGIFactory1_t fpCreateFactory1 = nullptr;
CreateDXGIFactory_t fpCreateFactory = nullptr;
D3D11CreateDeviceAndSwapChain_t fpD3D11CreateWithSwap = nullptr;

std::vector<void*> g_hookedVtables;
std::vector<void*> g_hookedIfVtables;

// Declaraciones adelantadas (las definiciones usan los punteros originales de arriba).
HRESULT STDMETHODCALLTYPE PresentDetour(IDXGISwapChain*, UINT, UINT);
HRESULT STDMETHODCALLTYPE ResizeBuffersDetour(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
HRESULT STDMETHODCALLTYPE CreateSwapChainForHwndDetour(IDXGIFactory2*, HWND,
    const DXGI_SWAP_CHAIN_DESC1*, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**);
HRESULT STDMETHODCALLTYPE CreateSwapChainForCoreWindowDetour(IDXGIFactory2*, IUnknown*,
    const DXGI_SWAP_CHAIN_DESC1*, IDXGIOutput*, IDXGISwapChain1**);
HRESULT STDMETHODCALLTYPE CreateSwapChainForCompositionDetour(IDXGIFactory2*, IUnknown*,
    const DXGI_SWAP_CHAIN_DESC1*, IDXGIOutput*, IDXGISwapChain1**);
HRESULT STDMETHODCALLTYPE CreateSwapChainDetour(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);

void hookFactoryVtable(void* factory, int version) {
  if (!factory) return;
  // Los hooks de vtable de factory revientan dxgi.dll en Bedrock (error
  // 0xc0000005 en 0x202d8): desactivados. Los exports se instalan abajo.
  logLine("vtable de factory omitida (insegura)");
}

// Creamos una swapchain dummy propia. La vtable de IDXGISwapChain es común a
// todas las instancias de dxgi.dll, así que enganchar su Present intercepta
// también el Present del juego, incluso si ya tenía swapchain creada.
void createDummySwapChainAndHook(HMODULE d3d11);

void createDummySwapChainAndHook(HMODULE d3d11) {
  if (!d3d11) return;
  FARPROC create = GetProcAddress(d3d11, "D3D11CreateDeviceAndSwapChain");
  if (!create) return;
  typedef HRESULT(WINAPI* CreateFn)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT,
      const D3D_FEATURE_LEVEL*, UINT, UINT, const DXGI_SWAP_CHAIN_DESC*,
      IDXGISwapChain**, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);
  CreateFn fn = reinterpret_cast<CreateFn>(create);

  static HWND dummy = nullptr;
  if (!dummy) {
    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "LexBridgeDummy";
    RegisterClassA(&wc);
    dummy = CreateWindowA("LexBridgeDummy", nullptr, WS_OVERLAPPEDWINDOW,
                          0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr);
  }
  if (!dummy) { logLine("dummy hwnd falló"); return; }

  DXGI_SWAP_CHAIN_DESC desc{};
  desc.BufferDesc.Width = 1;
  desc.BufferDesc.Height = 1;
  desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.BufferDesc.RefreshRate.Numerator = 60;
  desc.BufferDesc.RefreshRate.Denominator = 1;
  desc.SampleDesc.Count = 1;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.BufferCount = 2;
  desc.OutputWindow = dummy;
  desc.Windowed = TRUE;
  desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  desc.Flags = 0;

  ID3D11Device* dev = nullptr;
  ID3D11DeviceContext* ctx = nullptr;
  HRESULT hr = fn(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                  D3D11_SDK_VERSION, &desc, &g_dummySwap, &dev, nullptr, &ctx);
  if (FAILED(hr)) {
    hr = fn(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, &desc, &g_dummySwap, &dev, nullptr, &ctx);
  }
  if (FAILED(hr) || !g_dummySwap) {
    char m[128];
    snprintf(m, sizeof(m), "dummy swapchain hr=0x%lX", (unsigned long)hr);
    logLine(m);
    return;
  }
  g_dev11 = dev;
  g_api = "d3d11";
  void** vt = *(void***)g_dummySwap;
  const MH_STATUS sp = MH_CreateHook(vt[8], (void*)&PresentDetour, (void**)&fpPresent);
  const MH_STATUS sr = MH_CreateHook(vt[13], (void*)&ResizeBuffersDetour, (void**)&fpResizeBuffers);
  MH_EnableHook(vt[8]);
  MH_EnableHook(vt[13]);
  char m[160];
  snprintf(m, sizeof(m), "Present vtable dummy sp=%d sr=%d vt=%p", sp, sr, vt);
  logLine(m);
}

void attachSwapChain(IDXGISwapChain* sw) {
  if (!sw || g_swap == sw) return;
  g_swap = sw;
  ID3D11Device* dev = nullptr;
  if (SUCCEEDED(sw->GetDevice(__uuidof(ID3D11Device), (void**)&dev))) {
    g_dev11 = dev;
    g_api = "d3d11";
  } else {
    g_api = GetModuleHandleW(L"d3d12.dll") ? "d3d12" : "dxgi";
  }
  logLine(g_api);
  if (!InterlockedCompareExchange(&g_presentHooked, 1, 0)) {
    void** vt = *(void***)sw;
    MH_CreateHook(vt[8], (void*)&PresentDetour, (void**)&fpPresent);
    MH_CreateHook(vt[13], (void*)&ResizeBuffersDetour, (void**)&fpResizeBuffers);
    logLine("Present enganchado");
  }
  writeStatus();
}

HRESULT STDMETHODCALLTYPE PresentDetour(IDXGISwapChain* sw, UINT sync, UINT flags) {
  // La primera llamada real nos da la swapchain del juego (la nuestra nunca hace
  // Present), así podemos aplicar efectos sobre el backbuffer correcto.
  if (sw && sw != g_dummySwap && !InterlockedCompareExchange(&g_realSwapCaptured, 1, 0)) {
    g_swap = sw;
    logLine("swapchain real capturada");
    writeStatus();
  }
  const LONG64 frames = InterlockedIncrement64(&g_frames);
  const ULONGLONG now = GetTickCount64();
  if (now - (ULONGLONG)g_windowStart >= 500) {
    const LONG64 delta = frames - g_framesAtWindow;
    InterlockedExchange64(&g_fps, (LONG64)(delta * 1000 / (now - (ULONGLONG)g_windowStart)));
    InterlockedExchange64(&g_windowStart, (LONG64)now);
    InterlockedExchange64(&g_framesAtWindow, frames);
  }
  if ((long long)now - (long long)g_lastWrite >= 250) {
    InterlockedExchange64(&g_lastWrite, (LONG64)now);
    writeStatus();
  }
  applyEffects(sw);
  return fpPresent ? fpPresent(sw, sync, flags) : S_OK;
}

HRESULT STDMETHODCALLTYPE ResizeBuffersDetour(IDXGISwapChain* sw, UINT count, UINT w, UINT h,
                                              DXGI_FORMAT fmt, UINT flags) {
  dropRender11();
  return fpResizeBuffers ? fpResizeBuffers(sw, count, w, h, fmt, flags) : S_OK;
}

HRESULT STDMETHODCALLTYPE CreateSwapChainForHwndDetour(IDXGIFactory2* self, HWND hwnd,
    const DXGI_SWAP_CHAIN_DESC1* desc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fs,
    IDXGIOutput* out, IDXGISwapChain1** swap) {
  HRESULT hr = fpCreateForHwnd ? fpCreateForHwnd(self, hwnd, desc, fs, out, swap) : E_FAIL;
  logLine("CreateSwapChainForHwnd llamado");
  if (SUCCEEDED(hr) && swap && *swap) attachSwapChain(*swap);
  return hr;
}

HRESULT STDMETHODCALLTYPE CreateSwapChainForCoreWindowDetour(IDXGIFactory2* self, IUnknown* win,
    const DXGI_SWAP_CHAIN_DESC1* desc, IDXGIOutput* out, IDXGISwapChain1** swap) {
  HRESULT hr = fpCreateForCore ? fpCreateForCore(self, win, desc, out, swap) : E_FAIL;
  logLine("CreateSwapChainForCoreWindow llamado");
  if (SUCCEEDED(hr) && swap && *swap) attachSwapChain(*swap);
  return hr;
}

HRESULT STDMETHODCALLTYPE CreateSwapChainForCompositionDetour(IDXGIFactory2* self, IUnknown* obj,
    const DXGI_SWAP_CHAIN_DESC1* desc, IDXGIOutput* out, IDXGISwapChain1** swap) {
  HRESULT hr = fpCreateForComposition ? fpCreateForComposition(self, obj, desc, out, swap) : E_FAIL;
  logLine("CreateSwapChainForComposition llamado");
  if (SUCCEEDED(hr) && swap && *swap) attachSwapChain(*swap);
  return hr;
}

HRESULT STDMETHODCALLTYPE CreateSwapChainDetour(IDXGIFactory* self, IUnknown* dev,
    DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** swap) {
  HRESULT hr = fpCreateSwapChain ? fpCreateSwapChain(self, dev, desc, swap) : E_FAIL;
  logLine("CreateSwapChain llamado");
  if (SUCCEEDED(hr) && swap && *swap) attachSwapChain(*swap);
  return hr;
}

HRESULT WINAPI CreateDXGIFactory2Detour(UINT flags, REFIID riid, void** factory) {
  HRESULT hr = fpCreateFactory2(flags, riid, factory);
  logLine("CreateDXGIFactory2 llamado");
  writeStatus();
  return hr;
}

HRESULT WINAPI CreateDXGIFactory1Detour(REFIID riid, void** factory) {
  HRESULT hr = fpCreateFactory1(riid, factory);
  logLine("CreateDXGIFactory1 llamado");
  writeStatus();
  return hr;
}

HRESULT WINAPI CreateDXGIFactoryDetour(REFIID riid, void** factory) {
  HRESULT hr = fpCreateFactory(riid, factory);
  logLine("CreateDXGIFactory llamado");
  return hr;
}

// ---- Control del cursor del juego mientras F8 está abierto ----
BOOL WINAPI ClipCursorDetour(const RECT* rect) {
  if (InterlockedCompareExchange(&g_uiMenu, 0, 0)) return FALSE;
  return fpClipCursor ? fpClipCursor(rect) : TRUE;
}

INT WINAPI ShowCursorDetour(BOOL show) {
  if (InterlockedCompareExchange(&g_uiMenu, 0, 0)) return 0;  // visible
  return fpShowCursor ? fpShowCursor(show) : 0;
}

HWND WINAPI SetCaptureDetour(HWND hwnd) {
  if (InterlockedCompareExchange(&g_uiMenu, 0, 0)) return nullptr;
  return fpSetCapture ? fpSetCapture(hwnd) : nullptr;
}

HRESULT WINAPI D3D11CreateWithSwapDetour(IDXGIAdapter* adapter, D3D_DRIVER_TYPE type, HMODULE sw,
    UINT flags, const D3D_FEATURE_LEVEL* levels, UINT nlevels, UINT sdk,
    const DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** swap, ID3D11Device** dev,
    D3D_FEATURE_LEVEL* used, ID3D11DeviceContext** ctx) {
  HRESULT hr = fpD3D11CreateWithSwap(adapter, type, sw, flags, levels, nlevels, sdk, desc, swap, dev, used, ctx);
  {
    char m[160];
    snprintf(m, sizeof(m), "D3D11CreateDeviceAndSwapChain hr=0x%lX swap=%p", (unsigned long)hr, swap ? (void*)*swap : nullptr);
    logLine(m);
  }
  if (SUCCEEDED(hr) && swap && *swap) attachSwapChain(*swap);
  return hr;
}

void installHooks() {
  MH_STATUS st = MH_Initialize();
  if (st != MH_OK && st != MH_ERROR_ALREADY_INITIALIZED) {
    logLine("MH_Initialize falló");
    return;
  }
  HMODULE dxgi = GetModuleHandleW(L"dxgi.dll");
  HMODULE d3d11 = GetModuleHandleW(L"d3d11.dll");
  if (!dxgi) dxgi = LoadLibraryW(L"dxgi.dll");
  if (!d3d11) d3d11 = LoadLibraryW(L"d3d11.dll");
  if (!dxgi) { logLine("sin dxgi.dll"); return; }

  // Reintentar D3D11CreateDeviceAndSwapChain si aún no está cargado.
  for (int i = 0; i < 40 && !d3d11; ++i) { Sleep(50); d3d11 = GetModuleHandleW(L"d3d11.dll"); }
  if (!d3d11) d3d11 = LoadLibraryW(L"d3d11.dll");

  auto hookExport = [](HMODULE mod, const char* name, void* detour, void** original) -> bool {
    if (!mod) return false;
    FARPROC p = GetProcAddress(mod, name);
    if (!p) return false;
    MH_STATUS s = MH_CreateHook(reinterpret_cast<LPVOID>(p), detour, original);
    return s == MH_OK || s == MH_ERROR_ALREADY_CREATED;
  };

  const bool h2 = hookExport(dxgi, "CreateDXGIFactory2", (void*)&CreateDXGIFactory2Detour, (void**)&fpCreateFactory2);
  const bool h1 = hookExport(dxgi, "CreateDXGIFactory1", (void*)&CreateDXGIFactory1Detour, (void**)&fpCreateFactory1);
  const bool h0 = hookExport(dxgi, "CreateDXGIFactory",  (void*)&CreateDXGIFactoryDetour,  (void**)&fpCreateFactory);
  const bool h11 = hookExport(d3d11, "D3D11CreateDeviceAndSwapChain", (void*)&D3D11CreateWithSwapDetour, (void**)&fpD3D11CreateWithSwap);
  HMODULE user32 = GetModuleHandleW(L"user32.dll");
  const bool hc = hookExport(user32, "ClipCursor", (void*)&ClipCursorDetour, (void**)&fpClipCursor);
  const bool hs = hookExport(user32, "ShowCursor", (void*)&ShowCursorDetour, (void**)&fpShowCursor);
  const bool hcap = hookExport(user32, "SetCapture", (void*)&SetCaptureDetour, (void**)&fpSetCapture);
  {
    char m[192];
    snprintf(m, sizeof(m), "exports dxgi2=%d dxgi1=%d dxgi0=%d d3d11=%d ui=%d/%d/%d",
             h2, h1, h0, h11, hc, hs, hcap);
    logLine(m);
  }
  MH_EnableHook(MH_ALL_HOOKS);

  // Enganchar la vtable de Present de una swapchain propia: segura y funciona
  // aunque el juego creara la suya antes de inyectar.
  createDummySwapChainAndHook(d3d11);
  logLine("hooks instalados");
  writeStatus();
}

DWORD WINAPI initThread(LPVOID) {
  installHooks();
  return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(h);
    CreateThread(nullptr, 0, &initThread, nullptr, 0, nullptr);
  }
  return TRUE;
}
