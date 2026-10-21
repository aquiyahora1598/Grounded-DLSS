using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Collections.Generic;
using GroundedDLSSSetup;
class InstallerReviewTests {
 static string home;static int failed,total;
 static void Check(bool ok,string why){if(!ok)throw new Exception(why);}
 static byte[] B(string s){return Encoding.UTF8.GetBytes(s);}
 static Dictionary<string,byte[]> Payload(){return new Dictionary<string,byte[]> {{Engine.Allowed[0],B("loader")},{Engine.Allowed[1],B("runtime")},{Engine.Allowed[2],B("[Sharpening]\r\nEnabled=0\r\n")}};}
 static void Refused(Action f){bool refused=false;try{f();}catch(IOException){refused=true;}Check(refused,"Operation should refuse before modifying files");}
 static void Case(string name,Action<string,string,Engine> test){
  ++total;
  string root=Path.Combine(home,name),ini=Path.Combine(root,"config","Engine.ini");
  Directory.CreateDirectory(Path.GetDirectoryName(ini));Directory.CreateDirectory(Path.Combine(root,@"Maine\Binaries\Win64"));
  File.WriteAllText(Path.Combine(root,@"Maine\Binaries\Win64\Maine-Win64-Shipping.exe"),"fixture");
  File.WriteAllText(ini,"[SystemSettings]\r\nr.TemporalAA.Upsampling=0\r\n");
  try{test(root,ini,new Engine(root,ini,true));Console.WriteLine("PASS "+name);}catch(Exception ex){++failed;Console.WriteLine("FAIL "+name+": "+ex.Message);}
 }
 static string Data(string root){return Path.Combine(root,@"Maine\Binaries\Win64\GroundedDLSS-install");}
 static int Main(string[] args){
  Console.OutputEncoding=new UTF8Encoding(false);
  home=Path.GetFullPath(args[0]);if(Directory.Exists(home))throw new Exception("Fresh fixture directory required");
  if(args.Length>1)using(var stream=typeof(Engine).Assembly.GetManifestResourceStream("payload.zip")){
   Check(stream!=null,"Packaged installer has no embedded payload");using(var memory=new MemoryStream()){stream.CopyTo(memory);Check(memory.ToArray().SequenceEqual(File.ReadAllBytes(args[1])),"Embedded payload differs from the release package");}
  }
  Case("repeat-uninstall-preserves-encoding",(root,ini,e)=>{
   File.WriteAllText(ini,"[SystemSettings]\r\nr.TemporalAA.Upsampling=0\r\n;opción\r\n",Encoding.Unicode);
   byte[] original=File.ReadAllBytes(ini);e.Install(Payload(),null);e.Uninstall();Check(File.ReadAllBytes(ini).SequenceEqual(original),"Initial restore differed");
   e.Uninstall();Check(File.ReadAllBytes(ini).SequenceEqual(original),"Second uninstall rewrote the restored INI");
  });
  Case("damaged-backup-refuses-before-removal",(root,ini,e)=>{
   e.Install(Payload(),null);byte[] installed=File.ReadAllBytes(ini);
   File.WriteAllText(Path.Combine(Data(root),"Engine.ini.before"),"damaged backup");
   Refused(()=>e.Uninstall());Check(File.ReadAllBytes(ini).SequenceEqual(installed)&&File.Exists(Path.Combine(root,Engine.Allowed[0])),"Uninstall partially modified installation");
  });
  Case("unchanged-running-uninstaller-update",(root,ini,e)=>{
   byte[] self=B("unchanged installer image");e.Install(Payload(),self);
   string path=Path.Combine(Data(root),"Desinstalar Grounded DLSS.exe");
   // A running Windows image permits reads but does not permit replacement.
   using(var locked=File.Open(path,FileMode.Open,FileAccess.Read,FileShare.Read))e.Install(Payload(),self);
   Check(File.ReadAllBytes(path).SequenceEqual(self),"Self copy changed");
  });
  Case("recovery-preflights-entire-journal",(root,ini,e)=>{
   string tx=Path.Combine(Data(root),"transaction");Directory.CreateDirectory(tx);
   string target=Path.Combine(root,Engine.Allowed[0]);File.WriteAllText(target,"partial");File.WriteAllBytes(Path.Combine(tx,"0.before"),B("checkpoint"));
   string alien=Path.Combine(root,"unrelated.txt");
   File.WriteAllText(Path.Combine(tx,"journal.txt"),Convert.ToBase64String(B(alien))+"|new\r\n"+Convert.ToBase64String(B(target))+"|0.before\r\n");
   Refused(()=>e.Recover());Check(File.ReadAllText(target)=="partial","Recovery mutated a file before discovering an invalid entry");
  });
  Case("interrupted-staging-does-not-block-reinstall",(root,ini,e)=>{
   string target=Path.Combine(root,Engine.Allowed[0]),tx=Path.Combine(Data(root),"transaction");
   // Leave an actual uncommitted journal, then model a partially written
   // staging file from the same operation. No child process is terminated.
   var interrupted=new Transaction(tx);interrupted.Write(target,B("partial install"));
   string owned=Encoding.UTF8.GetString(Convert.FromBase64String(File.ReadAllLines(Path.Combine(tx,"staging.txt")).Single()));
   File.WriteAllText(owned,"partial write");
   string foreign=target+".grounded-dlss-tmp";File.WriteAllText(foreign,"unrelated legacy temporary file");
   e.Recover();Check(!File.Exists(target)&&!File.Exists(owned),"Recovery left an owned staging file");
   Check(File.ReadAllText(foreign)=="unrelated legacy temporary file","Recovery deleted an unowned file");
   e.Install(Payload(),null);Check(File.Exists(target),"Reinstall blocked by a stale temporary name");
  });
  Case("disabled-comparison-mode-refuses-duplicates",(root,ini,e)=>{
   e.Install(Payload(),null);byte[] installed=File.ReadAllBytes(ini);
   string active=Path.Combine(root,Engine.Allowed[1]),disabled=active+".grounded-test-off";File.Move(active,disabled);
   Refused(()=>e.Install(Payload(),null));Refused(()=>e.Uninstall());
   Check(!File.Exists(active)&&File.Exists(disabled)&&File.ReadAllBytes(ini).SequenceEqual(installed),"Comparison files were duplicated or orphaned");
  });
  Case("update-retains-user-edits-for-later-uninstall",(root,ini,e)=>{
   e.Install(Payload(),null);
   File.WriteAllText(ini,File.ReadAllText(ini).Replace("r.TemporalAA.Upsampling=1","r.TemporalAA.Upsampling=2")+"\r\n[UserSettings]\r\nKeepAfterUpdate=123\r\n");
   e.Install(Payload(),null);e.Uninstall();string restored=File.ReadAllText(ini);
   Check(restored.Contains("KeepAfterUpdate=123"),"Uninstall lost unrelated edits made before the update");
   Check(Engine.GetKey(restored,"r.TemporalAA.Upsampling")=="2","Uninstall lost the user's latest owned-key preference");
  });
  Console.WriteLine("Review regressions: "+(total-failed)+" passed, "+failed+" failed.");return failed==0?0:1;
 }
}
