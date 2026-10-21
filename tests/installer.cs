using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Collections.Generic;
using GroundedDLSSSetup;
class InstallerTests {
 static void Check(bool value,string reason){if(!value)throw new Exception(reason);}
 static byte[] B(string s){return Encoding.UTF8.GetBytes(s);}
 static string folder,ini;static Engine Make(){return new Engine(folder,ini,true);}
 static Dictionary<string,byte[]> Payload(){return new Dictionary<string,byte[]> {{Engine.Allowed[0],B("loader")},{Engine.Allowed[1],B("runtime")},{Engine.Allowed[2],B("[Sharpening]\r\nEnabled=0\r\n")}};}
 static void Refused(Action action,string reason){bool refused=false;try{action();}catch(IOException){refused=true;}Check(refused,reason);}
 static int Main(string[] args){try {
  folder=Path.GetFullPath(args[0]);ini=Path.Combine(folder,"config","Engine.ini");
  if(Directory.Exists(folder))throw new Exception("Use a fresh fixture directory");
  Directory.CreateDirectory(Path.Combine(folder,@"Maine\Binaries\Win64"));Directory.CreateDirectory(Path.GetDirectoryName(ini));
  File.WriteAllText(Path.Combine(folder,@"Maine\Binaries\Win64\Maine-Win64-Shipping.exe"),"fixture");
  byte[] before=B("[Core.System]\r\nPaths=custom\r\n[SystemSettings]\r\nr.TemporalAA.Upsampling=0\r\nr.DepthOfFieldQuality=1\r\n");File.WriteAllBytes(ini,before);
  Make().Install(Payload(),null);Check(Engine.GetKey(File.ReadAllText(ini),"r.TemporalAA.Upsampling")=="1","required key");
  byte[] stable=File.ReadAllBytes(ini);Make().Install(Payload(),null);Make().Install(Payload(),null);
  Check(File.ReadAllBytes(ini).SequenceEqual(stable),"repeated update changed Engine.ini bytes");
  Make().Uninstall();Check(File.ReadAllBytes(ini).SequenceEqual(before),"byte-exact config restore");Check(!File.Exists(Path.Combine(folder,Engine.Allowed[0])),"loader remained");
  Make().Install(Payload(),null);File.AppendAllText(Path.Combine(folder,Engine.Allowed[2]),";user edit\r\n");
  Make().Install(Payload(),null);Check(File.ReadAllText(Path.Combine(folder,Engine.Allowed[2])).Contains(";user edit"),"config lost on update");
  File.AppendAllText(ini,"\r\n[UserSettings]\r\nOther=123\r\n");Make().Uninstall();
  Check(File.ReadAllText(ini).Contains("Other=123")&&Engine.GetKey(File.ReadAllText(ini),"r.TemporalAA.Upsampling")=="0","merge restore lost user changes");
  Check(File.Exists(Path.Combine(folder,Engine.Allowed[2])),"modified config was removed");
  File.Delete(Path.Combine(folder,Engine.Allowed[2]));Make().Uninstall();
  File.WriteAllBytes(Path.Combine(folder,Engine.Allowed[0]),B("foreign"));Refused(()=>Make().Install(Payload(),null),"foreign loader overwritten");Check(File.ReadAllText(Path.Combine(folder,Engine.Allowed[0]))=="foreign","collision mutated");File.Delete(Path.Combine(folder,Engine.Allowed[0]));
  var failing=Make();failing.Fault=i=>{if(i==2)throw new IOException("injected write failure");};byte[] checkpoint=File.ReadAllBytes(ini);
  Refused(()=>failing.Install(Payload(),null),"failure not raised");Check(!File.Exists(Path.Combine(folder,Engine.Allowed[0]))&&!File.Exists(Path.Combine(folder,Engine.Allowed[1]))&&File.ReadAllBytes(ini).SequenceEqual(checkpoint),"partial install not rolled back");
  Refused(()=>Make().Install(new Dictionary<string,byte[]>{{@"..\outside.txt",B("bad")}},null),"path traversal allowed");
  Refused(()=>Make().Install(new Dictionary<string,byte[]>{{@"Maine\Binaries\Win64\PartyWin.dll",B("bad")}},null),"foreign game file allowed");
  Make().Install(Payload(),null);File.WriteAllText(Path.Combine(folder,Engine.Allowed[1]),"foreign update");Make().Uninstall();Check(File.ReadAllText(Path.Combine(folder,Engine.Allowed[1]))=="foreign update","modified binary removed");
  string tx=Path.Combine(folder,@"Maine\Binaries\Win64\GroundedDLSS-install\transaction");Directory.CreateDirectory(tx);
  string target=Path.Combine(folder,Engine.Allowed[0]);File.WriteAllText(target,"partial");File.WriteAllBytes(Path.Combine(tx,"0.before"),B("checkpoint"));
  File.WriteAllText(Path.Combine(tx,"journal.txt"),Convert.ToBase64String(B(target))+"|0.before\r\n");Make().Recover();Check(File.ReadAllText(target)=="checkpoint","crash recovery failed");
  // An unterminated last record describes a change that never started.
  Directory.CreateDirectory(tx);File.WriteAllText(target,"partial");File.WriteAllBytes(Path.Combine(tx,"0.before"),B("checkpoint"));
  File.WriteAllText(Path.Combine(tx,"journal.txt"),Convert.ToBase64String(B(target))+"|0.before\r\nQUJD");Make().Recover();
  Check(File.ReadAllText(target)=="checkpoint"&&!Directory.Exists(tx),"torn last journal record blocked recovery");
  Directory.CreateDirectory(tx);File.WriteAllText(target,"partial");File.WriteAllBytes(Path.Combine(tx,"0.before"),B("checkpoint"));
  File.WriteAllBytes(Path.Combine(tx,"journal.txt"),B(Convert.ToBase64String(B(target))+"|0.before\r\n").Concat(new byte[16]).ToArray());Make().Recover();
  Check(File.ReadAllText(target)=="checkpoint"&&!Directory.Exists(tx),"zero-filled journal tail blocked recovery");
  Directory.CreateDirectory(tx);File.WriteAllText(Path.Combine(tx,"staging.txt"),"QUJ");Make().Recover();Check(!Directory.Exists(tx),"torn staging record blocked recovery");
  // A damaged terminated record is still refused and kept for review.
  Directory.CreateDirectory(tx);File.WriteAllText(Path.Combine(tx,"journal.txt"),"QUJD\r\n"+Convert.ToBase64String(B(target))+"|0.before\r\n");
  Refused(()=>Make().Recover(),"damaged journal record accepted");Check(Directory.Exists(tx)&&File.ReadAllText(target)=="checkpoint","damaged journal changed files");Directory.Delete(tx,true);
  // Unreal accepts spaces around '=', keeps LF files and UTF-16 files with BOM.
  Check(Engine.PutKey("[SystemSettings]\r\nr.TemporalAA.Upsampling = 0\r\n","r.TemporalAA.Upsampling","1")=="[SystemSettings]\r\nr.TemporalAA.Upsampling=1\r\n","spaced key duplicated");
  Check(Engine.GetKey("[SystemSettings]\r\nr.TemporalAA.Upsampling = 0\r\n","r.TemporalAA.Upsampling")=="0","spaced key not read");
  Check(Engine.GetKey("[SystemSettings]\r\nr.TemporalAA.UpsamplingX=0\r\n","r.TemporalAA.Upsampling")==null,"key prefix matched");
  Check(Engine.PutKey("[SystemSettings]\nA=1\n","B","2")=="[SystemSettings]\nA=1\nB=2\n","LF line endings changed");
  const string spaced="[SystemSettings]\r\nA=1\r\n\r\n[Next]\r\nX=1\r\n",added="[SystemSettings]\r\nA=1\r\nB=2\r\n\r\n[Next]\r\nX=1\r\n";
  Check(Engine.PutKey(spaced,"B","2")==added&&Engine.PutKey(added,"B",null)==spaced,"new key not kept with its section");
  const string settled="[SystemSettings]\r\nA=1\r\n\r\n";
  Check(Engine.PutKey(settled,"A","1")==settled&&Engine.PutKey("[SystemSettings]\nA=1","B","2")=="[SystemSettings]\nA=1\nB=2"&&Engine.PutKey("","A","1")=="[SystemSettings]\r\nA=1\r\n","unchanged Engine.ini bytes rewritten");
  File.Delete(target);File.Delete(Path.Combine(folder,Engine.Allowed[1]));
  var wide=new UnicodeEncoding(false,true);byte[] utf16=wide.GetPreamble().Concat(wide.GetBytes("[SystemSettings]\r\nr.TemporalAA.Upsampling = 0\r\nName=Á\r\n")).ToArray();File.WriteAllBytes(ini,utf16);
  Make().Install(Payload(),null);byte[] wideAfter=File.ReadAllBytes(ini);string wideText=wide.GetString(wideAfter,2,wideAfter.Length-2);
  Check(wideAfter[0]==0xFF&&wideAfter[1]==0xFE&&wideText.Contains("Name=Á")&&Engine.GetKey(wideText,"r.TemporalAA.Upsampling")=="1"&&wideText.Split('\n').Count(l=>l.Contains("r.TemporalAA.Upsampling"))==1,"UTF-16 Engine.ini or spaced key mishandled");
  Make().Uninstall();Check(File.ReadAllBytes(ini).SequenceEqual(utf16),"UTF-16 Engine.ini not restored byte-exact");
  Console.WriteLine("PASS: byte-exact uninstall, update preserves custom INI, merge restores only owned settings, collisions, modified files, rollback, path allowlist, crash recovery, torn journal tail, spaced keys, line endings, unchanged bytes kept, UTF-16 Engine.ini.");return 0;
 }catch(Exception e){Console.Error.WriteLine(e);return 1;}}
}
