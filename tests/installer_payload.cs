using System;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Collections.Generic;
using System.Windows.Forms;
using GroundedDLSSSetup;

class InstallerPayloadTests {
 static void Check(bool ok,string why){if(!ok)throw new Exception(why);}
 static void Refuse(){bool refused=false;try{using(var s=PayloadPackage.OpenInstall()){} }catch(IOException){refused=true;}Check(refused,"Changed or missing sidecar accepted");}
 [STAThread] static int Main(string[] args){try {
  string path=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,PayloadPackage.FileName),held=path+".test-held";
  Check(!File.Exists(held),"Use a fresh test directory");
  var files=new Dictionary<string,byte[]>();
  using(var stream=PayloadPackage.OpenInstall()) {
   bool locked=false;try{using(var writer=File.Open(path,FileMode.Open,FileAccess.Write,FileShare.ReadWrite)){} }catch(IOException){locked=true;}Check(locked,"Verified archive can be overwritten while in use");
   using(var zip=new ZipArchive(stream,ZipArchiveMode.Read))foreach(var item in zip.Entries)using(var m=new MemoryStream()){using(var s=item.Open())s.CopyTo(m);files.Add(item.FullName.Replace('/','\\'),m.ToArray());}
  }
  Check(files.Count==15&&!files.Keys.Any(x=>x.EndsWith("nvngx_dlss.dll",StringComparison.OrdinalIgnoreCase)),"Payload contents changed");
  File.Move(path,held);
  try {
   Refuse();
   // Help and removal must remain usable from the installed manager even when
   // the original downloaded ZIP is gone. No sidecar is needed for these paths.
   foreach(string language in new[]{"es","en","pt-BR"}){I18n.SetLanguage(language);using(var view=SetupForm.CreateTermsView())Check(view.Controls.OfType<TabControl>().Single().TabPages.Count==6,"Offline licenses missing");}
   byte[] bad=File.ReadAllBytes(held);bad[bad.Length/2]^=1;File.WriteAllBytes(path,bad);Refuse();File.Delete(path);
   File.WriteAllText(path,"truncated");Refuse();File.Delete(path);
   string root=Path.GetFullPath(args[0]),ini=Path.Combine(root,"config","Engine.ini");
   Check(!Directory.Exists(root),"Fresh fixture required");Directory.CreateDirectory(Path.Combine(root,@"Maine\Binaries\Win64"));Directory.CreateDirectory(Path.GetDirectoryName(ini));
   File.WriteAllText(Path.Combine(root,@"Maine\Binaries\Win64\Maine-Win64-Shipping.exe"),"fixture");File.WriteAllText(ini,"[SystemSettings]\r\nr.TemporalAA.Upsampling=0\r\n");
   byte[] original=File.ReadAllBytes(ini);var engine=new Engine(root,ini,true);engine.Install(files,null);engine.Uninstall();
   Check(original.SequenceEqual(File.ReadAllBytes(ini)),"Uninstall without sidecar did not restore settings");
   Console.WriteLine("PASS original payload, stream lock, missing/tampered/truncated rejection, offline licenses and uninstall");
  } finally {if(File.Exists(path))File.Delete(path);File.Move(held,path);}
  return 0;
 }catch(Exception ex){Console.Error.WriteLine(ex);return 1;}}
}
