#pragma once
// Local, offline discovery. Trust NVIDIA's production signature rather than a
// frozen list of release hashes. Never load a library to inspect its identity.
#include <windows.h>
#include <wintrust.h>
#include <softpub.h>
#include <wincrypt.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <algorithm>
#include <string>
#include <cstdint>
#include <cwctype>
#pragma comment(lib,"wintrust.lib")
#pragma comment(lib,"crypt32.lib")
#pragma comment(lib,"version.lib")
#pragma comment(lib,"shell32.lib")

namespace grounded::local_dlss {
namespace fs=std::filesystem;
constexpr char Owner[]="Grounded DLSS NVIDIA working library v2\r\n";
constexpr char LegacyOwner[]="Grounded DLSS automatic NVIDIA runtime v1\r\n";
constexpr uint64_t MaxBytes=512ull*1024*1024;
struct Library {fs::path path;uint64_t version=0;std::wstring text;unsigned minimumDriver=0;explicit operator bool()const{return version!=0;}};
struct Handle {HANDLE h=INVALID_HANDLE_VALUE;explicit Handle(HANDLE v):h(v){}~Handle(){if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);}operator HANDLE()const{return h;}};
inline bool Safe(const fs::path& path) {
    const auto text=fs::absolute(path).wstring();
    if(text.size()<3||text[1]!=L':'||text.find(L':',2)!=std::wstring::npos||text.rfind(L"\\\\",0)==0)return false;
    for(auto p=fs::absolute(path);!p.empty();){auto a=GetFileAttributesW(p.c_str());
        if(a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_REPARSE_POINT))return false;
        auto parent=p.parent_path();if(parent==p)break;p=parent;
    }return true;
}
inline std::wstring Fold(std::wstring s){for(auto& c:s)c=wchar_t(towlower(c));return s;}
inline Library Inspect(const fs::path& path) {
    if(!Safe(path))return {};
    WIN32_FILE_ATTRIBUTE_DATA attr{};if(!GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&attr)||(attr.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))return {};
    uint64_t bytes=(uint64_t(attr.nFileSizeHigh)<<32)|attr.nFileSizeLow;if(bytes<1024*1024||bytes>MaxBytes)return {};
    std::ifstream file(path,std::ios::binary);IMAGE_DOS_HEADER dos{};file.read(reinterpret_cast<char*>(&dos),sizeof(dos));
    if(!file||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0||uint64_t(dos.e_lfanew)>bytes-sizeof(IMAGE_NT_HEADERS64))return {};
    IMAGE_NT_HEADERS64 pe{};file.seekg(dos.e_lfanew);file.read(reinterpret_cast<char*>(&pe),sizeof(pe));
    if(!file||pe.Signature!=IMAGE_NT_SIGNATURE||pe.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||!(pe.FileHeader.Characteristics&IMAGE_FILE_DLL)||pe.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC)return {};
    DWORD unused=0,size=GetFileVersionInfoSizeW(path.c_str(),&unused);if(!size||size>1024*1024)return {};
    std::vector<unsigned char> data(size);if(!GetFileVersionInfoW(path.c_str(),0,size,data.data()))return {};
    VS_FIXEDFILEINFO* fixed=nullptr;UINT length=0;
    if(!VerQueryValueW(data.data(),L"\\",reinterpret_cast<void**>(&fixed),&length)||length<sizeof(*fixed)||fixed->dwSignature!=0xfeef04bd)return {};
    struct Translation{WORD lang,code;};Translation* translations=nullptr;
    if(!VerQueryValueW(data.data(),L"\\VarFileInfo\\Translation",reinterpret_cast<void**>(&translations),&length)||length<sizeof(Translation))return {};
    auto value=[&](const wchar_t* name){wchar_t key[128]{};swprintf_s(key,L"\\StringFileInfo\\%04x%04x\\%ls",translations[0].lang,translations[0].code,name);wchar_t* v=nullptr;UINT n=0;
        if(!VerQueryValueW(data.data(),key,reinterpret_cast<void**>(&v),&n)||!v||!n)return std::wstring();
        const auto start=reinterpret_cast<uintptr_t>(data.data()),end=start+data.size(),at=reinterpret_cast<uintptr_t>(v);
        return at>=start&&at<=end&&n<=(end-at)/sizeof(wchar_t)?std::wstring(v,wcsnlen_s(v,n)):std::wstring();};
    if(Fold(value(L"InternalName"))!=L"dlss"||Fold(value(L"FileDescription")).find(L"production")==std::wstring::npos)return {};
    const uint64_t version=(uint64_t(fixed->dwFileVersionMS)<<32)|fixed->dwFileVersionLS;
    if(version<(2ull<<48))return {};
    unsigned major=0,minor=0;auto minimum=value(L"NGXMinimumDriverVersion");
    const auto required=swscanf_s(minimum.c_str(),L"%u.%u",&major,&minor)==2&&major<10000&&minor<100?major*100+minor:0;
    return {path,version,std::to_wstring(HIWORD(fixed->dwFileVersionMS))+L"."+std::to_wstring(LOWORD(fixed->dwFileVersionMS))+L"."+std::to_wstring(HIWORD(fixed->dwFileVersionLS))+L"."+std::to_wstring(LOWORD(fixed->dwFileVersionLS)),required};
}
inline unsigned DriverVersion(){
    auto module=LoadLibraryExW(L"nvapi64.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!module)return 0;
    using Query=void*(__cdecl*)(unsigned);using Init=int(__cdecl*)();using Version=int(__cdecl*)(unsigned*,char*);
    auto query=reinterpret_cast<Query>(GetProcAddress(module,"nvapi_QueryInterface"));
    auto init=query?reinterpret_cast<Init>(query(0x0150e828)):nullptr;
    auto get=query?reinterpret_cast<Version>(query(0x2926aaad)):nullptr;
    auto unload=query?reinterpret_cast<Init>(query(0xd22bdd7e)):nullptr;unsigned version=0;char branch[64]{};
    if(init&&init()==0){if(!get||get(&version,branch)!=0)version=0;if(unload)unload();}FreeLibrary(module);return version;
}
inline bool DriverSupported(unsigned driver,unsigned required){return !driver||!required||driver>=required;}
inline bool Trusted(const Library& lib,HANDLE locked) {
    if(!lib||locked==INVALID_HANDLE_VALUE)return false;
    WINTRUST_FILE_INFO file{};file.cbStruct=sizeof(file);file.pcwszFilePath=lib.path.c_str();file.hFile=locked;
    WINTRUST_DATA data{};data.cbStruct=sizeof(data);data.dwUIChoice=WTD_UI_NONE;data.dwUnionChoice=WTD_CHOICE_FILE;data.pFile=&file;
    data.dwStateAction=WTD_STATEACTION_VERIFY;data.dwProvFlags=WTD_CACHE_ONLY_URL_RETRIEVAL|WTD_REVOCATION_CHECK_NONE;
    GUID action=WINTRUST_ACTION_GENERIC_VERIFY_V2;
    const LONG result=WinVerifyTrust(reinterpret_cast<HWND>(INVALID_HANDLE_VALUE),&action,&data);bool nvidia=false;
    if(result==0){auto provider=WTHelperProvDataFromStateData(data.hWVTStateData);auto signer=provider?WTHelperGetProvSignerFromChain(provider,0,FALSE,0):nullptr;
        if(signer&&signer->csCertChain&&signer->pasCertChain){wchar_t name[256]{};
            CertGetNameStringW(signer->pasCertChain[0].pCert,CERT_NAME_SIMPLE_DISPLAY_TYPE,0,nullptr,name,256);nvidia=_wcsicmp(name,L"NVIDIA Corporation")==0;}}
    data.dwStateAction=WTD_STATEACTION_CLOSE;WinVerifyTrust(reinterpret_cast<HWND>(INVALID_HANDLE_VALUE),&action,&data);return result==0&&nvidia;
}
inline bool Trusted(const Library& lib){if(!lib)return false;Handle h(CreateFileW(lib.path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));return Trusted(lib,h);}
inline fs::path CacheRoot(){wchar_t p[MAX_PATH]{};if(FAILED(SHGetFolderPathW(nullptr,CSIDL_COMMON_APPDATA,nullptr,SHGFP_TYPE_CURRENT,p)))return {};return fs::path(p)/L"NVIDIA"/L"NGX"/L"models"/L"dlss"/L"versions";}
inline std::vector<Library> Discover(const fs::path& root){
    std::vector<Library> result;std::error_code ec;if(root.empty()||!Safe(root)||!fs::is_directory(root,ec))return result;
    size_t dirs=0,files=0;
    for(fs::directory_iterator i(root,fs::directory_options::skip_permission_denied,ec),end;i!=end&&!ec&&dirs<1024;i.increment(ec)){
        ++dirs;const auto folder=i->path()/L"files";if(!Safe(folder)||!fs::is_directory(folder,ec)){ec.clear();continue;}
        std::error_code fe;
        for(fs::directory_iterator f(folder,fs::directory_options::skip_permission_denied,fe);f!=end&&!fe&&files<8192;f.increment(fe)){
            ++files;auto ext=Fold(f->path().extension().wstring());if(ext!=L".bin"&&ext!=L".dll")continue;
            if(auto lib=Inspect(f->path()))result.push_back(std::move(lib));
        }
    }return result;
}
inline bool Owned(const fs::path& dir){if(!Safe(dir/L"owner.txt"))return false;std::ifstream f(dir/L"owner.txt",std::ios::binary);std::string s(128,0);f.read(s.data(),s.size());s.resize(size_t(f.gcount()));return s==Owner||s==LegacyOwner;}
inline bool Prepare(const Library& source,const fs::path& dir){
    if(!Safe(dir)||!Safe(dir/L"nvngx_dlss.dll"))return false;
    const auto target=dir/L"nvngx_dlss.dll",marker=dir/L"owner.txt";
    if(fs::exists(target)&&(!Owned(dir)||!Trusted(Inspect(target))))return false;
    if(fs::exists(marker)&&!Owned(dir))return false;
    fs::create_directories(dir);
    if(!fs::exists(marker)){Handle owner(CreateFileW(marker.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));DWORD written=0;
        if(owner.h==INVALID_HANDLE_VALUE||!WriteFile(owner,Owner,sizeof(Owner)-1,&written,nullptr)||written!=sizeof(Owner)-1)return false;}
    Handle input(CreateFileW(source.path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
    // Lock before re-reading metadata and validating its production signature.
    const auto locked=Inspect(source.path);if(locked.version!=source.version||!Trusted(locked,input))return false;
    // Signature verification may read through the same handle; copy from byte 0.
    LARGE_INTEGER start{},size{};
    if(!SetFilePointerEx(input,start,nullptr,FILE_BEGIN)||!GetFileSizeEx(input,&size))return false;
    const auto temp=dir/(L"nvngx_dlss."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetTickCount64())+L".tmp");
    bool ok=false;
    {Handle output(CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));
        if(output.h==INVALID_HANDLE_VALUE)return false;
        std::vector<unsigned char> buffer(1024*1024);DWORD read=0,written=0;uint64_t total=0;ok=true;
        while(true){if(!ReadFile(input,buffer.data(),DWORD(buffer.size()),&read,nullptr)){ok=false;break;}if(!read)break;
            total+=read;if(total>MaxBytes||!WriteFile(output,buffer.data(),read,&written,nullptr)||written!=read){ok=false;break;}}
        ok=ok&&total==uint64_t(size.QuadPart)&&FlushFileBuffers(output);
    }
    // The copy itself must carry the same production identity and a valid
    // NVIDIA signature (which hashes the image) before it replaces anything.
    if(ok){const auto copy=Inspect(temp);ok=copy.version==source.version&&Trusted(copy);}
    if(ok)ok=MoveFileExW(temp.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok)DeleteFileW(temp.c_str());return ok;
}
inline void RemoveWorking(const fs::path& dir){
    if(!Owned(dir)||!Safe(dir/L"nvngx_dlss.dll"))return;
    const auto dll=dir/L"nvngx_dlss.dll";
    if(fs::exists(dll)&&!Trusted(Inspect(dll)))return;
    if(!fs::exists(dll)||DeleteFileW(dll.c_str()))DeleteFileW((dir/L"owner.txt").c_str());
}
struct Selection {fs::path directory,source;std::wstring version,warning;bool automatic=false;};
inline Selection Select(const fs::path& plugins,const fs::path& cache,const fs::path& driver={},unsigned driverVersion=DriverVersion()){
    const auto working=plugins/L"GroundedDLSS-data"/L"auto-runtime";
    try{
        auto candidates=Discover(cache);
        if(!driver.empty())if(auto lib=Inspect(driver/L"nvngx_dlss.dll"))candidates.push_back(std::move(lib));
        std::stable_sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){return a.version>b.version;});
        std::wstring warning;
        for(const auto& lib:candidates){if(!DriverSupported(driverVersion,lib.minimumDriver)||!Trusted(lib))continue;
            if(Prepare(lib,working))return {working,lib.path,lib.text,warning,true};
            warning=L"Cannot prepare NVIDIA working library; preserving existing files";
        }
        RemoveWorking(working);
        return {{},{},{},warning.empty()?L"No compatible DLSS in local NVIDIA components; no backup or download is used":warning,false};
    }catch(...){return {{},{},{},L"Local NVIDIA discovery failed; no private backup is used",false};}
}
}
