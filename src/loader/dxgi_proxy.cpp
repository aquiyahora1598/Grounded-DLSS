// Standalone Grounded loader. Only the real System32 DXGI and our own ASI are loaded.
#include <windows.h>
#include <string>
#include <array>
#include <filesystem>
#include <cstdio>
namespace {
HMODULE self=nullptr,systemDxgi=nullptr;
INIT_ONCE once=INIT_ONCE_STATIC_INIT;
constexpr const char* names[]={"ApplyCompatResolutionQuirking","CompatString","CompatValue","CreateDXGIFactory","CreateDXGIFactory1","CreateDXGIFactory2","DXGID3D10CreateDevice","DXGID3D10CreateLayeredDevice","DXGID3D10GetLayeredDeviceSize","DXGID3D10RegisterLayers","DXGIDeclareAdapterRemovalSupport","DXGIDisableVBlankVirtualization","DXGIDumpJournal","DXGIGetDebugInterface1","DXGIReportAdapterConfiguration","PIXBeginCapture","PIXEndCapture","PIXGetCaptureState","SetAppCompatStringPointer","UpdateHMDEmulationStatus"};
std::array<FARPROC,20> exports{};
// AppCompat calls this export during loader notifications, before DllMain.
// Loading another graphics DLL at that point fails. Keep its two pointer
// arguments and apply them once the genuine DXGI is available instead.
SRWLOCK compatLock=SRWLOCK_INIT;
void* compatFirst=nullptr;
void* compatSecond=nullptr;
bool compatPending=false;
using CompatSetter=void(WINAPI*)(void*,void*);
CompatSetter compatSetter=nullptr;
BOOL CALLBACK ResolveSystem(PINIT_ONCE,void*,void**) {
 // D3D11 may call an exported DXGI entry while the process is initializing
 // dependencies, BEFORE this DLL's CRT initialization. No C++ runtime, locale,
 // filesystem, allocation or stdio is allowed on this forwarding path.
 wchar_t path[32768]{};UINT length=GetSystemDirectoryW(path,32768);
 if(!length||length+10>=32768)return FALSE;
 path[length++]=L'\\';constexpr wchar_t filename[]=L"dxgi.dll";
 for(unsigned i=0;i<sizeof(filename)/sizeof(filename[0]);++i)path[length+i]=filename[i];
 systemDxgi=LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
 if(!systemDxgi)return FALSE;
 for(unsigned i=0;i<exports.size();++i)exports[i]=GetProcAddress(systemDxgi,names[i]);
 AcquireSRWLockExclusive(&compatLock);
 compatSetter=reinterpret_cast<CompatSetter>(exports[18]);
 if(compatPending&&compatSetter){compatSetter(compatFirst,compatSecond);compatPending=false;}
 ReleaseSRWLockExclusive(&compatLock);
 return TRUE;
}
void Log(const std::filesystem::path& base,const char* line){
 FILE* f=nullptr;_wfopen_s(&f,(base/L"GroundedDLSS-loader.log").c_str(),L"a");
 if(f){fprintf(f,"[%llu] %s\n",GetTickCount64(),line);fclose(f);}
}
DWORD WINAPI LoadRuntime(void*) {
 wchar_t file[32768]{};
 if(!GetModuleFileNameW(self,file,32768))return 0;
 auto folder=std::filesystem::path(file).parent_path();
 auto exe=GetModuleHandleW(nullptr);
 auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(exe);
 if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<=0||dos->e_lfanew>4096)return 0;
 auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(reinterpret_cast<const unsigned char*>(exe)+dos->e_lfanew);
 if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||
    nt->FileHeader.TimeDateStamp!=1745511144||nt->OptionalHeader.SizeOfImage!=111910912){
  Log(folder,"Unsupported executable: DLSS runtime was not loaded.");return 0;
 }
 HMODULE pinned=nullptr;
 if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&LoadRuntime),&pinned))return 0;
 auto runtime=folder/L"plugins"/L"GroundedDLSS.asi";
 auto module=LoadLibraryExW(runtime.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
 Log(folder,module?"Standalone loader: GroundedDLSS.asi loaded; OptiScaler is not used.":"GroundedDLSS.asi could not be loaded; DXGI forwarding remains available.");
 return 0;
}
}
extern "C" void WINAPI GroundedSetAppCompatStringPointer(void* first,void* second) {
 AcquireSRWLockExclusive(&compatLock);
 if(compatSetter)compatSetter(first,second);
 else{compatFirst=first;compatSecond=second;compatPending=true;}
 ReleaseSRWLockExclusive(&compatLock);
}
extern "C" FARPROC ResolveGroundedDxgi(unsigned index) {
 if(index>=exports.size()||!InitOnceExecuteOnce(&once,ResolveSystem,nullptr,nullptr)||!exports[index]){
  RaiseException(ERROR_PROC_NOT_FOUND,EXCEPTION_NONCONTINUABLE,0,nullptr);return nullptr;
 }
 return exports[index];
}
BOOL APIENTRY DllMain(HMODULE module,DWORD reason,LPVOID) {
 if(reason==DLL_PROCESS_ATTACH){
  self=module;DisableThreadLibraryCalls(module);
  wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);
  const auto slash=wcsrchr(path,L'\\');
  if(!_wcsicmp(slash?slash+1:path,L"Maine-Win64-Shipping.exe")){
   auto thread=CreateThread(nullptr,0,LoadRuntime,nullptr,0,nullptr);if(thread)CloseHandle(thread);
  }
 }return TRUE;
}

