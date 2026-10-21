// Unlike LoadLibrary smoke testing, this EXE imports D3D11 and DXGI at startup.
// D3D11 invokes DXGI's private registration exports before the proxy CRT starts.
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <cstdio>
#include <filesystem>
using Microsoft::WRL::ComPtr;
int main(){
 wchar_t path[32768]{};GetModuleFileNameW(GetModuleHandleW(L"dxgi.dll"),path,32768);
 if(!std::filesystem::equivalent(std::filesystem::path(path).parent_path(),std::filesystem::current_path()))return 1;
 ComPtr<IDXGIFactory1> factory;if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))return 1;
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL feature{};
 if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&feature,&context)))return 1;
 if(GetModuleHandleW(L"GroundedDLSS.asi"))return 1;
 printf("PASS: process startup through local DXGI; early D3D11 layer registration; D3D11 device and factory; unrelated host skipped.\n");return 0;
}
