using System;
using System.IO;
using System.Linq;
using System.Diagnostics;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using Microsoft.Win32;

namespace GroundedDLSSSetup {
 // Production libraries are authenticated by Windows and NVIDIA's publisher
 // identity. No release version or cache directory identifier is an allowlist.
 public static class NvidiaLocalRuntime {
  public const string AutoDll=@"Maine\Binaries\Win64\plugins\GroundedDLSS-data\auto-runtime\nvngx_dlss.dll";
  public const string AutoOwner=@"Maine\Binaries\Win64\plugins\GroundedDLSS-data\auto-runtime\owner.txt";
  public const string Owner="Grounded DLSS NVIDIA working library v2\r\n";
  const string LegacyOwner="Grounded DLSS automatic NVIDIA runtime v1\r\n";
  const long MaxBytes=512L*1024*1024;
  public sealed class Library {public string Path;public Version Version;public uint MinimumDriver;}
  public static string CacheRoot {get{return System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData),@"NVIDIA\NGX\models\dlss\versions");}}
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr LoadLibraryExW(string name,IntPtr reserved,uint flags);
  [DllImport("kernel32.dll",CharSet=CharSet.Ansi,ExactSpelling=true)] static extern IntPtr GetProcAddress(IntPtr module,string name);
  [DllImport("kernel32.dll")] static extern bool FreeLibrary(IntPtr module);
  [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate IntPtr Query(uint id);
  [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int Initialize();
  [UnmanagedFunctionPointer(CallingConvention.Cdecl,CharSet=CharSet.Ansi)] delegate int Driver(out uint version,[Out] StringBuilder branch);
  [DllImport("version.dll",CharSet=CharSet.Unicode)] static extern uint GetFileVersionInfoSizeW(string path,out uint ignored);
  [DllImport("version.dll",CharSet=CharSet.Unicode)] static extern bool GetFileVersionInfoW(string path,uint ignored,uint size,IntPtr data);
  [DllImport("version.dll",CharSet=CharSet.Unicode)] static extern bool VerQueryValueW(IntPtr data,string key,out IntPtr value,out uint length);
  public static uint DriverVersion(){
   IntPtr module=LoadLibraryExW("nvapi64.dll",IntPtr.Zero,0x800);if(module==IntPtr.Zero)return 0;
   try{
    IntPtr entry=GetProcAddress(module,"nvapi_QueryInterface");if(entry==IntPtr.Zero)return 0;
    var query=(Query)Marshal.GetDelegateForFunctionPointer(entry,typeof(Query));IntPtr init=query(0x0150e828),get=query(0x2926aaad),unload=query(0xd22bdd7e);
    if(init==IntPtr.Zero||get==IntPtr.Zero||((Initialize)Marshal.GetDelegateForFunctionPointer(init,typeof(Initialize)))()!=0)return 0;
    try{uint version;return ((Driver)Marshal.GetDelegateForFunctionPointer(get,typeof(Driver)))(out version,new StringBuilder(64))==0?version:0;}
    finally{if(unload!=IntPtr.Zero)((Initialize)Marshal.GetDelegateForFunctionPointer(unload,typeof(Initialize)))();}
   }finally{FreeLibrary(module);}
  }
  static uint MinimumDriver(string path){
   uint ignored,size=GetFileVersionInfoSizeW(path,out ignored);if(size==0||size>1024*1024)return 0;
   IntPtr data=Marshal.AllocHGlobal((int)size);
   try{IntPtr value;uint length;if(!GetFileVersionInfoW(path,0,size,data)||!VerQueryValueW(data,@"\VarFileInfo\Translation",out value,out length)||length<4)return 0;
    string key=String.Format(@"\StringFileInfo\{0:x4}{1:x4}\NGXMinimumDriverVersion",(ushort)Marshal.ReadInt16(value),(ushort)Marshal.ReadInt16(value,2));
    if(!VerQueryValueW(data,key,out value,out length)||length<2||length>64)return 0;
    string[] parts=(Marshal.PtrToStringUni(value)??"").Split('.');uint major,minor;
    return parts.Length==2&&UInt32.TryParse(parts[0],out major)&&UInt32.TryParse(parts[1],out minor)&&major<10000&&minor<100?major*100+minor:0;
   }finally{Marshal.FreeHGlobal(data);}
  }
  static string ActiveDriverDirectory(){
   try{
    string image=Registry.GetValue(@"HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\nvlddmkm","ImagePath",null) as string;
    if(String.IsNullOrWhiteSpace(image))return null;
    if(image.StartsWith(@"\SystemRoot\",StringComparison.OrdinalIgnoreCase))image=System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows),image.Substring(12));
    else if(image.StartsWith(@"\??\",StringComparison.Ordinal))image=image.Substring(4);
    image=System.IO.Path.GetFullPath(image);string allowed=System.IO.Path.Combine(Environment.SystemDirectory,@"DriverStore\FileRepository")+"\\";
    if(!image.StartsWith(allowed,StringComparison.OrdinalIgnoreCase))return null;
    Safe.Check(image);return System.IO.Path.GetDirectoryName(image);
   }catch(IOException){return null;}catch(UnauthorizedAccessException){return null;}catch(System.Security.SecurityException){return null;}catch(ArgumentException){return null;}
  }
  [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct FileInfoNative {public uint Size;[MarshalAs(UnmanagedType.LPWStr)] public string Path;public IntPtr Handle,Subject;}
  [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct TrustData {public uint Size;public IntPtr Policy,Sip;public uint UI,Revocation,Choice;public IntPtr File;public uint StateAction;public IntPtr State,Url;public uint Flags,Context;public IntPtr SignatureSettings;}
  [DllImport("wintrust.dll",ExactSpelling=true,CharSet=CharSet.Unicode)] static extern int WinVerifyTrust(IntPtr window,[In] ref Guid action,[In,Out] ref TrustData data);
  static bool Trusted(string path,FileStream locked){
   var file=new FileInfoNative {Size=(uint)Marshal.SizeOf(typeof(FileInfoNative)),Path=path,Handle=locked.SafeFileHandle.DangerousGetHandle()};
   IntPtr memory=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(FileInfoNative)));
   try{
    Marshal.StructureToPtr(file,memory,false);
    var data=new TrustData {Size=(uint)Marshal.SizeOf(typeof(TrustData)),UI=2,Choice=1,File=memory,StateAction=1,Flags=0x1000|0x10};
    var action=new Guid("00AAC56B-CD44-11D0-8CC2-00C04FC295EE");
    try{
     if(WinVerifyTrust(new IntPtr(-1),ref action,ref data)!=0)return false;
     using(var original=X509Certificate.CreateFromSignedFile(path))using(var signer=new X509Certificate2(original))
      return String.Equals(signer.GetNameInfo(X509NameType.SimpleName,false),"NVIDIA Corporation",StringComparison.OrdinalIgnoreCase);
    }finally{data.StateAction=2;WinVerifyTrust(new IntPtr(-1),ref action,ref data);}
   }finally{Marshal.DestroyStructure(memory,typeof(FileInfoNative));Marshal.FreeHGlobal(memory);}
  }
  static Library Inspect(string path){
   path=System.IO.Path.GetFullPath(path);Safe.Check(path);
   var f=new FileInfo(path);if(!f.Exists||f.Length<1024*1024||f.Length>MaxBytes)throw new IOException(I18n.T("El archivo no es una biblioteca DLSS de producción válida."));
   using(var stream=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.Read))using(var reader=new BinaryReader(stream)){
    if(reader.ReadUInt16()!=0x5a4d)throw new IOException(I18n.T("DLSS no es una biblioteca Windows válida."));
    stream.Position=0x3c;int offset=reader.ReadInt32();if(offset<0||offset>stream.Length-264)throw new IOException(I18n.T("Cabecera DLSS inválida."));
    stream.Position=offset;if(reader.ReadUInt32()!=0x4550||reader.ReadUInt16()!=0x8664)throw new IOException(I18n.T("Se requiere DLSS x64."));
    stream.Position=offset+22;if((reader.ReadUInt16()&0x2000)==0||reader.ReadUInt16()!=0x20b)throw new IOException(I18n.T("Se requiere una DLL DLSS x64."));
   }
   var info=FileVersionInfo.GetVersionInfo(path);
   if(!String.Equals(info.InternalName,"DLSS",StringComparison.OrdinalIgnoreCase)||(info.FileDescription??"").IndexOf("production",StringComparison.OrdinalIgnoreCase)<0||info.FileMajorPart<2)
    throw new IOException(I18n.T("Se requiere NVIDIA DLSS Super Resolution de producción; no se admiten otras bibliotecas ni versiones de desarrollo."));
   return new Library {Path=path,Version=new Version(info.FileMajorPart,info.FileMinorPart,info.FileBuildPart,info.FilePrivatePart),MinimumDriver=MinimumDriver(path)};
  }
  public static Library VerifyFile(string path){
   path=System.IO.Path.GetFullPath(path);Safe.Check(path);
   using(var stream=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.Read)){
    var info=Inspect(path);
    if(!Trusted(path,stream))throw new IOException(I18n.T("La firma de DLSS no es válida o no pertenece a NVIDIA. Se conserva el archivo."));
    return info;
   }
  }
  static IEnumerable<Library> Discover(string root){
   root=System.IO.Path.GetFullPath(root);Safe.Check(root);var result=new List<Library>();
   if(!Directory.Exists(root))return result;
   string[] directories;try{directories=Directory.EnumerateDirectories(root).Take(1024).ToArray();}catch(IOException){return result;}catch(UnauthorizedAccessException){return result;}
   int count=0;
   foreach(var dir in directories){try{
    string files=Safe.Child(dir,"files");if(!Directory.Exists(files))continue;
    foreach(var path in Directory.EnumerateFiles(files).Take(8192-count)){
     ++count;string extension=System.IO.Path.GetExtension(path);if(!extension.Equals(".bin",StringComparison.OrdinalIgnoreCase)&&!extension.Equals(".dll",StringComparison.OrdinalIgnoreCase))continue;
     try{result.Add(Inspect(path));}catch(IOException){}catch(UnauthorizedAccessException){}
    }
   }catch(IOException){}catch(UnauthorizedAccessException){}if(count>=8192)break;}
   return result;
  }
  public static Library Find(){return Find(CacheRoot,ActiveDriverDirectory(),DriverVersion());}
  // An explicit cache/driver root permits isolated tests without private libraries.
  public static Library Find(string cacheRoot,string driverDirectory,uint driver){
   var candidates=Discover(cacheRoot).ToList();
   if(!String.IsNullOrEmpty(driverDirectory))try{candidates.Add(Inspect(System.IO.Path.Combine(driverDirectory,"nvngx_dlss.dll")));}catch(IOException){}catch(UnauthorizedAccessException){}
   foreach(var candidate in candidates.OrderByDescending(x=>x.Version)){
    if(driver!=0&&candidate.MinimumDriver!=0&&driver<candidate.MinimumDriver)continue;
    try{return VerifyFile(candidate.Path);}catch(IOException){}catch(UnauthorizedAccessException){}catch(CryptographicException){}
   }
   throw new MissingRuntimeException();
  }
  public static bool HasAutomaticOwner(string game){
   try{string marker=Safe.Child(System.IO.Path.GetFullPath(game),AutoOwner);if(!File.Exists(marker)||new FileInfo(marker).Length>128)return false;string value=File.ReadAllText(marker);return value==Owner||value==LegacyOwner;}
   catch(IOException){return false;}catch(UnauthorizedAccessException){return false;}
  }
  public static bool CanRemoveAutomatic(string game){
   try{
    string root=System.IO.Path.GetFullPath(game),marker=Safe.Child(root,AutoOwner),dll=Safe.Child(root,AutoDll);
    if(!HasAutomaticOwner(game))return false;
    if(File.Exists(dll))VerifyFile(dll);return true;
   }catch(IOException){return false;}catch(UnauthorizedAccessException){return false;}catch(CryptographicException){return false;}
  }
 }
}
