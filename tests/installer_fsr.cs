using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Xml.Serialization;
using System.Collections.Generic;
using GroundedDLSSSetup;
class InstallerFsrTests {
 static string suite;static int total,failed;
 const string Section="[/Script/Maine.MaineGameUserSettings]";
 static byte[] B(string s){return new UTF8Encoding(false).GetBytes(s);}
 static void Check(bool ok,string why){if(!ok)throw new Exception(why);}
 static string Menu(string ini){return Path.Combine(Path.GetDirectoryName(ini),"GameUserSettings.ini");}
 static string Data(string root){return Path.Combine(root,@"Maine\Binaries\Win64\GroundedDLSS-install");}
 static string Get(string text){return Engine.GetKey(text,"FSRQualityLevel",Engine.GameSettingsSection);}
 static Dictionary<string,byte[]> Payload(){return new Dictionary<string,byte[]> {{Engine.Allowed[0],B("loader")},{Engine.Allowed[1],B("runtime")},{Engine.Allowed[2],B("[Sharpening]\r\nEnabled=0\r\n")}};}
 static void Refused(Action action){bool refused=false;try{action();}catch(IOException){refused=true;}Check(refused,"Operation unexpectedly succeeded");}
 static void Case(string name,Action<string,string,Engine> action){
  ++total;string root=Path.Combine(suite,name),ini=Path.Combine(root,"config","Engine.ini");
  Directory.CreateDirectory(Path.GetDirectoryName(ini));Directory.CreateDirectory(Path.Combine(root,@"Maine\Binaries\Win64"));
  File.WriteAllText(Path.Combine(root,@"Maine\Binaries\Win64\Maine-Win64-Shipping.exe"),"fixture");
  File.WriteAllBytes(ini,B("[SystemSettings]\r\nr.TemporalAA.Upsampling=0\r\n"));
  try{action(root,ini,new Engine(root,ini,true));Console.WriteLine("PASS "+name);}catch(Exception ex){++failed;Console.WriteLine("FAIL "+name+": "+ex);}
 }
 static int Main(string[] args){
  suite=Path.GetFullPath(args[0]);if(Directory.Exists(suite))throw new Exception("Fresh fixture required");
  Case("fsr-on-installs-off-restores-exact-settings",(root,ini,e)=>{
   string menu=Menu(ini);byte[] original=B("[ScalabilityGroups]\r\nsg.ResolutionQuality=58\r\n"+Section+"\r\nFSRQualityLevel=2\r\nResolutionSizeX=2560\r\nResolutionSizeY=1440\r\nMouseSensitivity=0.8\r\n;personal\r\n");
   File.WriteAllBytes(menu,original);e.Install(Payload(),null);
   Check(File.ReadAllText(menu)==Encoding.UTF8.GetString(original).Replace("FSRQualityLevel=2","FSRQualityLevel=0"),"Unrelated graphics or controls changed");
   Check(File.ReadAllBytes(Path.Combine(Data(root),"GameUserSettings.ini.before")).SequenceEqual(original),"Original menu not backed up");
   byte[] stable=File.ReadAllBytes(menu);e.Install(Payload(),null);e.Install(Payload(),null);Check(File.ReadAllBytes(menu).SequenceEqual(stable),"Update changed menu bytes");
   e.Uninstall();Check(File.ReadAllBytes(menu).SequenceEqual(original),"Original FSR settings not restored byte-exact");
   e.Uninstall();Check(File.ReadAllBytes(menu).SequenceEqual(original),"Repeated uninstall changed menu");
  });
  Case("fsr-already-off-keeps-spaces-bom-and-final-line",(root,ini,e)=>{
   string menu=Menu(ini),text=Section+"\n  FSRQualityLevel = 0\nLanguage=Español\n\n";
   var enc=new UTF8Encoding(true);byte[] original=enc.GetPreamble().Concat(enc.GetBytes(text)).ToArray();File.WriteAllBytes(menu,original);
   e.Install(Payload(),null);Check(File.ReadAllBytes(menu).SequenceEqual(original),"Already-off menu was rewritten");
   e.Uninstall();Check(File.ReadAllBytes(menu).SequenceEqual(original),"Already-off menu restore changed bytes");
  });
  Case("fsr-only-edits-correct-section-and-duplicates",(root,ini,e)=>{
   string menu=Menu(ini),text="[Other]\nFSRQualityLevel=9\n"+Section+"\nFSRQualityLevel = 2\nFSRQualityLevel=3\n[Next]\nFSRQualityLevel=8";
   File.WriteAllBytes(menu,B(text));e.Install(Payload(),null);string actual=File.ReadAllText(menu);
   Check(actual=="[Other]\nFSRQualityLevel=9\n"+Section+"\nFSRQualityLevel=0\n[Next]\nFSRQualityLevel=8","Wrong section edited, duplicate retained, or final newline added");
   e.Uninstall();Check(File.ReadAllText(menu)==text,"Original duplicate layout not restored");
  });
  foreach(bool big in new[]{false,true})Case("fsr-preserves-utf16-"+(big?"be":"le"),(root,ini,e)=>{
   string menu=Menu(ini);var enc=new UnicodeEncoding(big,true);string text=Section+"\r\nFSRQualityLevel = 4\r\nLanguage=Español\r\n";
   byte[] original=enc.GetPreamble().Concat(enc.GetBytes(text)).ToArray();File.WriteAllBytes(menu,original);e.Install(Payload(),null);
   byte[] actual=File.ReadAllBytes(menu);Check(actual.Take(2).SequenceEqual(original.Take(2))&&Get(enc.GetString(actual,2,actual.Length-2))=="0"&&File.ReadAllText(menu).Contains("Español"),"Encoding or FSR mismatch");
   e.Uninstall();Check(File.ReadAllBytes(menu).SequenceEqual(original),"UTF-16 not restored");
  });
  Case("fsr-new-settings-file-and-missing-key",(root,ini,e)=>{
   string menu=Menu(ini);e.Install(Payload(),null);Check(Get(File.ReadAllText(menu))=="0","First-run FSR not disabled");
   e.Uninstall();Check(!File.Exists(menu),"Generated menu file not removed");
   File.WriteAllText(menu,"[Other]\nKeep=1");byte[] original=File.ReadAllBytes(menu);e.Install(Payload(),null);Check(Get(File.ReadAllText(menu))=="0","Missing FSR not set");
   e.Uninstall();Check(File.ReadAllBytes(menu).SequenceEqual(original),"Missing original key/section not restored");
  });
  Case("fsr-game-created-settings-survive-uninstall",(root,ini,e)=>{
   string menu=Menu(ini);e.Install(Payload(),null);File.AppendAllText(menu,"Language=Español\r\nResolutionSizeX=3840\r\n");
   e.Uninstall();string text=File.ReadAllText(menu);Check(Get(text)==null&&text.Contains("ResolutionSizeX=3840")&&text.Contains("Language=Español"),"First-run game settings discarded");
  });
  Case("fsr-uninstall-merges-new-resolution-and-controls",(root,ini,e)=>{
   string menu=Menu(ini);File.WriteAllText(menu,Section+"\r\nFSRQualityLevel=2\r\nResolutionSizeX=1920\r\n");e.Install(Payload(),null);
   File.WriteAllText(menu,File.ReadAllText(menu).Replace("ResolutionSizeX=1920","ResolutionSizeX=3840")+"MouseSensitivity=0.4\r\n");
   e.Uninstall();string text=File.ReadAllText(menu);Check(Get(text)=="2"&&text.Contains("ResolutionSizeX=3840")&&text.Contains("MouseSensitivity=0.4"),"Uninstall replaced player preferences");
  });
  Case("fsr-keeps-new-user-choice-on-uninstall",(root,ini,e)=>{
   string menu=Menu(ini);File.WriteAllText(menu,Section+"\nFSRQualityLevel=2\n");e.Install(Payload(),null);
   File.WriteAllText(menu,Section+"\nFSRQualityLevel=3\nLanguage=English\n");byte[] changed=File.ReadAllBytes(menu);
   e.Uninstall();Check(File.ReadAllBytes(menu).SequenceEqual(changed),"Latest player FSR choice overwritten");
  });
  Case("fsr-update-remembers-new-choice-and-other-edits",(root,ini,e)=>{
   string menu=Menu(ini);File.WriteAllText(menu,Section+"\nFSRQualityLevel=2\nResolutionSizeX=1920\n");e.Install(Payload(),null);
   File.WriteAllText(menu,Section+"\nFSRQualityLevel=3\nResolutionSizeX=2560\n");e.Install(Payload(),null);Check(Get(File.ReadAllText(menu))=="0","Update did not disable FSR");
   e.Install(Payload(),null);e.Uninstall();Check(Get(File.ReadAllText(menu))=="3"&&File.ReadAllText(menu).Contains("ResolutionSizeX=2560"),"Update lost latest preferences");
  });
  Case("fsr-update-does-not-revert-unrelated-edits",(root,ini,e)=>{
   string menu=Menu(ini);File.WriteAllText(menu,Section+"\nFSRQualityLevel=4\n");e.Install(Payload(),null);
   File.AppendAllText(menu,"MouseSensitivity=0.7\n");e.Install(Payload(),null);e.Uninstall();
   Check(Get(File.ReadAllText(menu))=="4"&&File.ReadAllText(menu).Contains("MouseSensitivity=0.7"),"Update replaced pre-update user edits");
  });
  Case("fsr-upgrade-state-v2-and-refuse-downgrade",(root,ini,e)=>{
   e.Install(Payload(),null);e.Uninstall();string menu=Menu(ini);File.WriteAllText(menu,Section+"\nFSRQualityLevel=2\n");
   string stateFile=Path.Combine(Data(root),"state.xml");State state;var xml=new XmlSerializer(typeof(State));
   using(var f=File.OpenRead(stateFile))state=(State)xml.Deserialize(f);state.Format=2;
   using(var f=File.Create(stateFile))xml.Serialize(f,state);
   e.Install(Payload(),null);Check(Get(File.ReadAllText(menu))=="0","Old state not upgraded");
   Check(File.ReadAllText(stateFile).Contains("<Format>3</Format>"),"Older installers would silently ignore FSR ownership");
   e.Uninstall();Check(Get(File.ReadAllText(menu))=="2","Migrated FSR preference not restored");
  });
  Case("fsr-failure-rolls-back-menu-engine-and-payload",(root,ini,e)=>{
   string menu=Menu(ini);File.WriteAllText(menu,Section+"\r\nFSRQualityLevel=3\r\n");byte[] before=File.ReadAllBytes(menu),engine=File.ReadAllBytes(ini);
   e.Fault=n=>{if(n==Payload().Count+1)throw new IOException("failure after settings changes");};
   Refused(()=>e.Install(Payload(),null));Check(File.ReadAllBytes(menu).SequenceEqual(before)&&File.ReadAllBytes(ini).SequenceEqual(engine)&&!File.Exists(Path.Combine(root,Engine.Allowed[0])),"Partial menu change survived rollback");
  });
  foreach(bool missing in new[]{false,true})Case("fsr-backup-"+(missing?"missing":"damaged")+"-refuses-before-removal",(root,ini,e)=>{
   string menu=Menu(ini);File.WriteAllText(menu,Section+"\nFSRQualityLevel=2\n");e.Install(Payload(),null);byte[] installed=File.ReadAllBytes(menu);
   string backup=Path.Combine(Data(root),"GameUserSettings.ini.before");if(missing)File.Delete(backup);else File.WriteAllText(backup,"damaged");
   Refused(()=>e.Uninstall());Check(File.Exists(Path.Combine(root,Engine.Allowed[0]))&&File.ReadAllBytes(menu).SequenceEqual(installed),"Invalid backup caused partial uninstall");
  });
  Case("fsr-menu-and-backup-support-crash-recovery",(root,ini,e)=>{
   string menu=Menu(ini),backup=Path.Combine(Data(root),"GameUserSettings.ini.before"),tx=Path.Combine(Data(root),"transaction");
   File.WriteAllText(menu,Section+"\nFSRQualityLevel=2\n");byte[] original=File.ReadAllBytes(menu);
   var interrupted=new Transaction(tx);interrupted.Write(backup,original);interrupted.Write(menu,B(Section+"\nFSRQualityLevel=0\n"));
   e.Recover();Check(File.ReadAllBytes(menu).SequenceEqual(original)&&!File.Exists(backup),"Menu recovery failed");
  });
  Console.WriteLine("FSR installer regressions: "+(total-failed)+" passed, "+failed+" failed.");return failed==0?0:1;
 }
}
