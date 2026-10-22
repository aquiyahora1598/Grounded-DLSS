// Exercise the exact injectable backend and AMD proxy on a temporary test HWND.
#define GROUNDED_FSR_FIXTURE_LIBRARY
#include "../tools/framegen_fsr_dispatch.cpp"
#include "../src/framegen/fsr_runtime.h"
#include <cstdarg>
namespace fg=grounded::framegen;
namespace backend=fg::fsr;
void Log(const char* format,...){va_list args;va_start(args,format);vprintf(format,args);va_end(args);putchar('\n');fflush(stdout);}
void Require(bool value,const char* what){if(!value)throw std::runtime_error(what);}
void Drain(){ffxDispatchDescFrameGenerationSwapChainWaitForPresentsDX12 d{};d.header.type=FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_WAIT_FOR_PRESENTS_DX12;
    Api(backend::runtime.dispatch(&backend::runtime.swapContext,&d.header),"drain presents");}
int RunPresent(const fs::path& folder){
    Gpu gpu;ComPtr<IDXGIFactory4> factory;Check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));Require(backend::Install(factory.Get(),folder,Log),"install failed");
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"GroundedFsrFGTest";
    Require(RegisterClassW(&wc)!=0,"register HWND");auto hwnd=CreateWindowW(wc.lpszClassName,L"DLSS mod: FSR FG presentation test",WS_OVERLAPPEDWINDOW,30,30,680,420,nullptr,nullptr,wc.hInstance,nullptr);
    Require(hwnd!=nullptr,"create HWND");ShowWindow(hwnd,SW_SHOWNOACTIVATE);
    DXGI_SWAP_CHAIN_DESC sd{};sd.BufferDesc.Width=W;sd.BufferDesc.Height=H;sd.BufferDesc.Format=DXGI_FORMAT_R10G10B10A2_UNORM;
    sd.SampleDesc.Count=1;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.BufferCount=3;sd.OutputWindow=hwnd;sd.Windowed=TRUE;sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    ComPtr<IDXGISwapChain> chain;Check(factory->CreateSwapChain(gpu.queue.Get(),&sd,&chain));Require(backend::runtime.proxy!=nullptr,"no AMD proxy");
    ComPtr<IDXGISwapChain3> three;Check(chain.As(&three));Check(three->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709));
    auto depth=gpu.Make(DXGI_FORMAT_R32_FLOAT,true),motion=gpu.Make(DXGI_FORMAT_R32G32_FLOAT,true);
    std::vector<float> z(SIZE_T(W)*H,.05f),mv(SIZE_T(W)*H*2,0.f);
    ComPtr<ID3D12DescriptorHeap> heap;D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=1;Check(gpu.device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)));
    fg::Camera camera{};camera.nearPlane=7;camera.fov=1.544741f;camera.deltaMs=1000.f/60;camera.right[0]=1;camera.up[1]=1;camera.forward[2]=1;
    unsigned sequence=0;
    auto frames=[&](unsigned count){for(unsigned i=0;i<count;++i){
        MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        gpu.Begin();gpu.Upload(depth,z.data(),4);gpu.Upload(motion,mv.data(),8);gpu.Transition(depth,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);gpu.Transition(motion,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        camera.worldTime=float(++sequence)/60;backend::runtime.Prepare(gpu.list.Get(),camera,0,0,depth.resource.Get(),motion.resource.Get(),W,H,i==0,false);
        ComPtr<ID3D12Resource> buffer;Check(chain->GetBuffer(three->GetCurrentBackBufferIndex(),IID_PPV_ARGS(&buffer)));
        auto handle=heap->GetCPUDescriptorHandleForHeapStart();gpu.device->CreateRenderTargetView(buffer.Get(),nullptr,handle);
        D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition={buffer.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET};
        gpu.list->ResourceBarrier(1,&barrier);const float color[]={.04f+.001f*(sequence%60),.1f,.3f,1};gpu.list->ClearRenderTargetView(handle,color,0,nullptr);
        backend::runtime.CopyHudless(gpu.list.Get(),buffer.Get());const float white[]={1,1,1,1};D3D12_RECT rect{12,12,220,38};gpu.list->ClearRenderTargetView(handle,white,1,&rect);
        std::swap(barrier.Transition.StateBefore,barrier.Transition.StateAfter);gpu.list->ResourceBarrier(1,&barrier);gpu.Submit();
        ID3D12CommandList* lists[]={gpu.list.Get()};backend::runtime.Submitted(gpu.queue.Get(),1,lists);
        Check(chain->Present(0,0));Sleep(16);
    }Drain();};
    frames(80);auto onNative=backend::runtime.nativePresents.load(),onRendered=backend::runtime.renderedPresents.load();
    Require(backend::runtime.generatedDispatches>=70 && onNative>onRendered*3/2,"FG did not increase native presentations");
    backend::runtime.SetEnabled(false);auto startOff=backend::runtime.nativePresents.load();frames(20);auto offNative=backend::runtime.nativePresents.load()-startOff;
    Require(offNative==20,"FG off must present exactly one real frame");backend::runtime.SetEnabled(true);
    for(auto size:{std::pair<UINT,UINT>{1920,1080},{2560,1440},{3840,2160}}){
        Check(chain->ResizeBuffers(3,size.first,size.second,sd.BufferDesc.Format,0));frames(16);
        Require(backend::runtime.fgWidth==size.first && backend::runtime.fgHeight==size.second,"FG resize mismatch");
        printf("PASS: AMD FG real presentation at %ux%u\n",size.first,size.second);
    }
    Require(backend::runtime.callbackErrors==0 && !backend::runtime.failed,"FG callback failure");
    backend::runtime.SetEnabled(false);Drain();three.Reset();chain.Reset();
    Require(backend::runtime.closed,"proxy final Release did not clean up contexts");
    DestroyWindow(hwnd);UnregisterClassW(wc.lpszClassName,wc.hInstance);
    printf("PASS: FG-on native=%llu rendered=%llu; FG-off native=%llu rendered=20; resize and automatic teardown\n",onNative,onRendered,offNative);return 0;
}
int wmain(int argc,wchar_t** argv){if(argc!=2)return 2;try{return RunPresent(fs::absolute(argv[1]));}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
