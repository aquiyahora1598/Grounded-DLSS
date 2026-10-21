// Launch with a child-only AppCompat layer to exercise Windows' pre-DllMain
// SetAppCompatStringPointer notification, then create a genuine DXGI factory.
#include <windows.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <filesystem>
#include <cstdio>
int main(){
 wchar_t path[32768]{};
 if(!GetModuleFileNameW(GetModuleHandleW(L"dxgi.dll"),path,32768))return 1;
 if(!std::filesystem::equivalent(std::filesystem::path(path).parent_path(),std::filesystem::current_path()))return 1;
 if(!GetModuleHandleW(L"AcGenral.dll")){fprintf(stderr,"AppCompat layer was not loaded; regression not exercised.\n");return 1;}
 Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
 if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))return 1;
 Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
 if(FAILED(factory->EnumAdapters1(0,&adapter)))return 1;
 if(GetModuleHandleW(L"GroundedDLSS.asi"))return 1;
 printf("PASS: AppCompat pre-DllMain notification; deferred native compatibility state; genuine DXGI factory/adapter; host isolation.\n");return 0;
}
