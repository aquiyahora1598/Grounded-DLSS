// Exercise actual System32 forwarding, COM results, concurrency and host isolation.
#include <windows.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <atomic>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>
using Microsoft::WRL::ComPtr;
int wmain(int argc,wchar_t** argv){
 if(argc!=2)return 2;
 auto proxy=LoadLibraryExW(std::filesystem::absolute(argv[1]).c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
 if(!proxy){std::cerr<<"LoadLibrary failed "<<GetLastError()<<'\n';return 1;}
 const char* exports[]={"ApplyCompatResolutionQuirking","CompatString","CompatValue","CreateDXGIFactory","CreateDXGIFactory1","CreateDXGIFactory2","DXGID3D10CreateDevice","DXGID3D10CreateLayeredDevice","DXGID3D10GetLayeredDeviceSize","DXGID3D10RegisterLayers","DXGIDeclareAdapterRemovalSupport","DXGIDisableVBlankVirtualization","DXGIDumpJournal","DXGIGetDebugInterface1","DXGIReportAdapterConfiguration","PIXBeginCapture","PIXEndCapture","PIXGetCaptureState","SetAppCompatStringPointer","UpdateHMDEmulationStatus"};
 const WORD ordinals[]={1,2,3,10,11,12,13,14,15,16,17,18,4,19,20,5,6,7,8,9};
 for(unsigned i=0;i<20;++i)if(!GetProcAddress(proxy,exports[i])||GetProcAddress(proxy,exports[i])!=GetProcAddress(proxy,MAKEINTRESOURCEA(ordinals[i]))){std::cerr<<"Missing/name-ordinal mismatch "<<exports[i];return 1;}
 using Factory=HRESULT(WINAPI*)(REFIID,void**);using Factory2=HRESULT(WINAPI*)(UINT,REFIID,void**);
 auto f0=reinterpret_cast<Factory>(GetProcAddress(proxy,"CreateDXGIFactory"));
 auto f1=reinterpret_cast<Factory>(GetProcAddress(proxy,"CreateDXGIFactory1"));
 auto f2=reinterpret_cast<Factory2>(GetProcAddress(proxy,"CreateDXGIFactory2"));
 ComPtr<IDXGIFactory> a;ComPtr<IDXGIFactory1> b;ComPtr<IDXGIFactory2> c;
 if(FAILED(f0(IID_PPV_ARGS(&a)))||FAILED(f1(IID_PPV_ARGS(&b)))||FAILED(f2(0,IID_PPV_ARGS(&c))))return 1;
 ComPtr<IDXGIAdapter1> adapter;if(FAILED(b->EnumAdapters1(0,&adapter)))return 1;
 DXGI_ADAPTER_DESC1 desc{};if(FAILED(adapter->GetDesc1(&desc)))return 1;
 std::atomic<unsigned> success{0};std::vector<std::thread> workers;
 for(unsigned n=0;n<8;++n)workers.emplace_back([&]{for(unsigned i=0;i<12;++i){ComPtr<IDXGIFactory1> item;if(SUCCEEDED(f1(IID_PPV_ARGS(&item))))++success;}});
 for(auto& t:workers)t.join();
 if(success!=96||GetModuleHandleW(L"GroundedDLSS.asi"))return 1;
 std::cout<<"PASS: 20 exports; 3 factory signatures; COM adapter; 96 concurrent forwards; unrelated host did not load the mod.\n";
 return 0;
}
