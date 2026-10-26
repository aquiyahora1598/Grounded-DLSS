using System;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Collections.Generic;
using System.Drawing;
using System.Windows.Forms;
using System.Xml.Serialization;
using GroundedDLSSSetup;

class InstallerFramegenTests {
 static int failed,total;static string home;static Dictionary<string,byte[]> payload;
 static readonly string fg=@"Maine\Binaries\Win64\plugins\FSRFrameGeneration\";
 static readonly string[] amd={fg+"amd_fidelityfx_loader_dx12.dll",fg+"amd_fidelityfx_framegeneration_dx12.dll"};
 static void Check(bool value,string message){if(!value)throw new Exception(message);}
 static void Case(string name,Action action){++total;try{action();Console.WriteLine("PASS "+name);}catch(Exception e){++failed;Console.WriteLine("FAIL "+name+": "+e);}}
 static void Refused(Action action){bool refused=false;try{action();}catch(IOException){refused=true;}Check(refused,"Expected refusal");}
 static Engine Fixture(string name,out string root){root=Path.Combine(home,name);string ini=Path.Combine(root,"config","Engine.ini");Directory.CreateDirectory(Path.GetDirectoryName(ini));Directory.CreateDirectory(Path.Combine(root,@"Maine\Binaries\Win64"));File.WriteAllText(Path.Combine(root,@"Maine\Binaries\Win64\Maine-Win64-Shipping.exe"),"fixture");File.WriteAllText(ini,"[SystemSettings]\r\n; user\r\n");return new Engine(root,ini,true);}
 static string StateFile(string root){return Path.Combine(root,@"Maine\Binaries\Win64\GroundedDLSS-install\state.xml");}
 [STAThread] static int Main(string[] args){
  Console.OutputEncoding=new UTF8Encoding(false);home=Path.GetFullPath(args[0]);Check(!Directory.Exists(home),"Fresh fixture required");Directory.CreateDirectory(home);Application.EnableVisualStyles();
  payload=new Dictionary<string,byte[]>(StringComparer.OrdinalIgnoreCase);
  using(var s=PayloadPackage.OpenInstall())using(var z=new ZipArchive(s,ZipArchiveMode.Read))foreach(var x in z.Entries)using(var m=new MemoryStream()){using(var f=x.Open())f.CopyTo(m);payload.Add(x.FullName.Replace('/','\\'),m.ToArray());}
  Case("framegen-payload-and-defaults",()=>{
   Check(amd.All(payload.ContainsKey),"AMD backend absent");
   Check(!payload.Keys.Any(k=>Path.GetFileName(k).Equals("nvngx_dlss.dll",StringComparison.OrdinalIgnoreCase)||k.Contains("nvngx_dlssg")||k.Contains("sl.interposer")),"NVIDIA runtime embedded");
   Check(payload.Keys.Any(k=>k.EndsWith("AMD-FidelityFX-SDK-LICENSE.txt")),"AMD license missing");
   string ini=Encoding.UTF8.GetString(payload[Engine.Allowed[2]]);Check(Engine.GetKey(ini,"Enabled","FrameGeneration")=="1","FG default is not enabled");
  });
  Case("framegen-install-update-uninstall",()=>{
   string root;var e=Fixture("cycle",out root);e.Install(payload,null);e.Install(payload,null);
   foreach(string name in amd)Check(Safe.HashFile(Path.Combine(root,name))==Safe.Hash(payload[name]),"AMD payload differs");
   e.Uninstall();Check(amd.All(n=>!File.Exists(Path.Combine(root,n))),"AMD dependency remained after uninstall");
   Check(File.Exists(Path.Combine(root,@"Maine\Binaries\Win64\Maine-Win64-Shipping.exe")),"Game file removed");
  });
  Case("framegen-upgrade-format3-preserves-user-ini",()=>{
   string root;var e=Fixture("upgrade",out root);
   var legacy=payload.Where(p=>!amd.Contains(p.Key)&&!p.Key.EndsWith("AMD-FidelityFX-SDK-LICENSE.txt")).ToDictionary(p=>p.Key,p=>p.Value);
   e.Install(legacy,null);var serializer=new XmlSerializer(typeof(State));State state;using(var f=File.OpenRead(StateFile(root)))state=(State)serializer.Deserialize(f);state.Format=3;using(var f=File.Create(StateFile(root)))serializer.Serialize(f,state);
   string ini=Path.Combine(root,Engine.Allowed[2]);File.AppendAllText(ini,"\r\n; custom user preference\r\n");byte[] custom=File.ReadAllBytes(ini);
   e.Install(payload,null);Check(File.ReadAllBytes(ini).SequenceEqual(custom),"Upgrade replaced user configuration");
   using(var f=File.OpenRead(StateFile(root)))state=(State)serializer.Deserialize(f);Check(state.Format==4,"New ownership format absent");
   e.Uninstall();Check(amd.All(n=>!File.Exists(Path.Combine(root,n)))&&File.ReadAllBytes(ini).SequenceEqual(custom),"Upgrade uninstall did not preserve edits or remove FG");
  });
  Case("framegen-foreign-dependency-refused-before-writes",()=>{
   string root;var e=Fixture("collision",out root);string target=Path.Combine(root,amd[0]);Directory.CreateDirectory(Path.GetDirectoryName(target));File.WriteAllText(target,"another mod");
   Refused(()=>e.Install(payload,null));Check(File.ReadAllText(target)=="another mod"&&!File.Exists(Path.Combine(root,Engine.Allowed[0])),"Foreign file overwritten or partial installation");
  });
  Case("framegen-modified-dependency-preserved",()=>{
   string root;var e=Fixture("modified",out root);e.Install(payload,null);string target=Path.Combine(root,amd[1]);File.AppendAllText(target,"user modification");string hash=Safe.HashFile(target);
   Refused(()=>e.Install(payload,null));Check(Safe.HashFile(target)==hash,"Update replaced modified AMD file");e.Uninstall();
   Check(Safe.HashFile(target)==hash&&!File.Exists(Path.Combine(root,amd[0])),"Uninstall removed modified file or retained unchanged one");
  });
  Case("framegen-partial-install-rolls-back-amd",()=>{
   string root;var e=Fixture("rollback",out root);e.Fault=n=>{if(n==4)throw new IOException("fixture interruption");};
   Refused(()=>e.Install(payload,null));Check(payload.Keys.All(n=>!File.Exists(Path.Combine(root,n))),"Partial payload remained after rollback");
  });
  foreach(string language in new[]{"es","en","pt-BR"}){string code=language;Case("framegen-prominent-shortcuts-and-license-"+code,()=>{
   I18n.SetLanguage(code);using(var form=new SetupForm()){
    foreach(string name in new[]{"ShortcutDLSS","ShortcutFrameGeneration"}){var label=(Label)form.Controls.Find(name,false).Single();
     Check(label.Font.Bold&&label.Font.Size>=14&&label.Height>=40&&label.Top<form.Controls.OfType<TextBox>().First(x=>x.ReadOnly).Top,"Shortcut not prominent");
     Check(TextRenderer.MeasureText(label.Text,label.Font).Width<label.Width-label.Padding.Horizontal,"Shortcut text clipped");
     Check(label.Text.Contains(name=="ShortcutDLSS"?"F8":"F9"),"Wrong shortcut key");}
    Check(!form.Controls.OfType<CheckBox>().Single().Checked,"Consent preselected");
   }
   using(var view=SetupForm.CreateTermsView()){var tabs=view.Controls.OfType<TabControl>().Single();Check(tabs.TabPages.Count==6,"AMD tab absent");
    Check(((TextBox)tabs.TabPages[5].Controls[0]).Text.Contains("REDISTRIBUTION"),"Original AMD binary license absent");
    string help=((TextBox)tabs.TabPages[0].Controls[0]).Text;Check(help.Contains("F8")&&help.Contains("F9")&&help.Contains("FRAME GENERATION"),"Help omits shortcuts");}
  });}
  Console.WriteLine("Frame Generation installer: "+(total-failed)+" passed, "+failed+" failed.");return failed==0?0:1;
 }
}
