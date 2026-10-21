#include "../src/grounded/local_dlss.h"
#include <iostream>
namespace fs=std::filesystem;
using namespace grounded::local_dlss;
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int wmain(int argc,wchar_t** argv){try{
 if(argc!=4)return 2;auto home=fs::absolute(argv[1]),older=fs::absolute(argv[2]),newer=fs::absolute(argv[3]);
 Check(!fs::exists(home),"Fresh fixture required");auto plugins=home/L"plugins",base=plugins/L"GroundedDLSS-runtime"/L"nvngx_dlss.dll",cache=home/L"cache";
 fs::create_directories(base.parent_path());fs::copy_file(older,base);
 Check(DriverSupported(61742,51215)&&!DriverSupported(51215,61742),"Minimum driver comparison failed");
 std::cout<<"DRIVER_VERSION="<<DriverVersion()<<"\n";
 Check(Trusted(Inspect(older))&&Trusted(Inspect(newer)),"Signed fixture rejected");Check(Inspect(newer).version>Inspect(older).version,"Fixtures must have ordered versions");
 Check(Select(plugins,cache,{},51214).directory.empty(),"Library with newer minimum driver was selected");
 auto first=Select(plugins,cache);Check(first.directory.empty(),"Legacy private library was used as a fallback");
 auto future=cache/L"new-folder-not-in-any-release-list"/L"files"/L"new-file-name.bin";fs::create_directories(future.parent_path());fs::copy_file(newer,future);
 auto chosen=Select(plugins,cache);Check(chosen.source==future&&chosen.automatic,"Newest external runtime not selected");
 Check(Inspect(chosen.directory/L"nvngx_dlss.dll").version==Inspect(newer).version,"Wrong working version");
 auto repeated=Select(plugins,cache);Check(repeated.source==future,"Working copy was used as a source");
 fs::remove(future);auto absent=Select(plugins,cache);Check(absent.directory.empty(),"Working copy became an offline fallback");
 Check(!fs::exists(chosen.directory/L"nvngx_dlss.dll"),"Stale owned working copy retained after source disappeared");
 auto oldSource=cache/L"older-external"/L"files"/L"model.bin";fs::create_directories(oldSource.parent_path());fs::copy_file(older,oldSource);
 // A version resource alone cannot authenticate a modified executable.
 fs::copy_file(newer,future);{std::fstream f(future,std::ios::binary|std::ios::in|std::ios::out);f.seekp(1024*1024);char c=0x55;f.write(&c,1);}
 Check(!Trusted(Inspect(future)),"Tampered signed library accepted");
 auto damaged=Select(plugins,cache);Check(damaged.source==oldSource,"Corrupt cache displaced valid external source");
 auto unowned=home/L"unowned-plugins";auto autoDir=unowned/L"GroundedDLSS-data"/L"auto-runtime";fs::create_directories(autoDir);{std::ofstream f(autoDir/L"nvngx_dlss.dll");f<<"user file";}
 fs::copy_file(newer,future,fs::copy_options::overwrite_existing);auto refusal=Select(unowned,cache);Check(refusal.directory.empty(),"Unowned automatic file overwritten");Check(fs::file_size(autoDir/L"nvngx_dlss.dll")==9,"User file changed");
 std::wcout<<L"PASS native local discovery, NVIDIA signatures, numeric ordering, unknown cache layout, absence without fallback, tampering, ownership; selected "<<chosen.version<<L"\n";
 return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
