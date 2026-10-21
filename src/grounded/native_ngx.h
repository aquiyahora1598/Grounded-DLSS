#pragma once
#include <windows.h>
#include <filesystem>
#include <algorithm>
#include <cwctype>

namespace grounded {
// Same file on disk, independent of how each path is spelled.
inline bool SameFile(const wchar_t* a,const wchar_t* b) {
    auto open=[](const wchar_t* p){return CreateFileW(p,0,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);};
    HANDLE x=open(a),y=open(b);BY_HANDLE_FILE_INFORMATION i{},j{};
    const bool same=x!=INVALID_HANDLE_VALUE && y!=INVALID_HANDLE_VALUE && GetFileInformationByHandle(x,&i) && GetFileInformationByHandle(y,&j) &&
        i.dwVolumeSerialNumber==j.dwVolumeSerialNumber && i.nFileIndexHigh==j.nFileIndexHigh && i.nFileIndexLow==j.nFileIndexLow;
    if(x!=INVALID_HANDLE_VALUE)CloseHandle(x);
    if(y!=INVALID_HANDLE_VALUE)CloseHandle(y);
    return same;
}
// Bootstrap the NGX runtime shipped with the already loaded NVIDIA D3D12
// driver. Never search a game directory or PATH for a driver DLL.
inline HMODULE NativeNgxModule() {
    auto driver=GetModuleHandleW(L"nvwgf2umx.dll");if(!driver)return nullptr;
    wchar_t driverName[32768]{},systemName[32768]{};
    if(!GetModuleFileNameW(driver,driverName,32768) || !GetSystemDirectoryW(systemName,32768))return nullptr;
    auto fold=[](std::wstring text){std::transform(text.begin(),text.end(),text.begin(),[](wchar_t c){return wchar_t(towlower(c));});return text;};
    auto allowed=fold((std::filesystem::path(systemName)/L"DriverStore"/L"FileRepository").wstring())+L"\\";
    const auto driverPath=std::filesystem::path(driverName);
    if(fold(driverPath.wstring()).find(allowed)!=0)return nullptr;
    auto runtime=driverPath.parent_path()/L"_nvngx.dll";
    // An already loaded NGX core is used only if it is the active driver's own
    // copy; a same-named module from anywhere else is refused.
    if(auto module=GetModuleHandleW(L"_nvngx.dll")){
        wchar_t loaded[32768]{};
        return GetModuleFileNameW(module,loaded,32768) && (fold(loaded)==fold(runtime.wstring()) || SameFile(loaded,runtime.c_str()))?module:nullptr;
    }
    return LoadLibraryExW(runtime.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
}
}
