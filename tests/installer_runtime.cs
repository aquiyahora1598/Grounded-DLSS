using System;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Collections.Generic;
using System.Drawing;
using System.Windows.Forms;
using GroundedDLSSSetup;

class InstallerRuntimeTests {
 static int failed;static string home,official;static Dictionary<string,byte[]> payload;
 static void Check(bool value,string message){if(!value)throw new Exception(message);}
 static void Refused(Action action){bool refused=false;try{action();}catch(IOException){refused=true;}Check(refused,"Expected refusal");}
 static void Case(string name,Action action){try{action();Console.WriteLine("PASS "+name);}catch(Exception ex){++failed;Console.WriteLine("FAIL "+name+": "+ex.Message);}}
 static Dictionary<string,byte[]> Package(){return new Dictionary<string,byte[]>(payload,StringComparer.OrdinalIgnoreCase);}
 [STAThread] static int Main(string[] args){
  Console.OutputEncoding=new UTF8Encoding(false);I18n.SetLanguage("es");home=Path.GetFullPath(args[0]);official=Path.GetFullPath(args[1]);
  Check(!Directory.Exists(home),"Fresh fixture required");Directory.CreateDirectory(home);
  payload=new Dictionary<string,byte[]>(StringComparer.OrdinalIgnoreCase);
  using(var resource=typeof(Engine).Assembly.GetManifestResourceStream("payload.zip"))using(var zip=new ZipArchive(resource,ZipArchiveMode.Read))foreach(var entry in zip.Entries){using(var m=new MemoryStream()){using(var source=entry.Open())source.CopyTo(m);payload.Add(entry.FullName.Replace('/','\\'),m.ToArray());}}
  Case("no-runtime-or-downloader-embedded",()=>{
   Check(!payload.ContainsKey(Engine.Allowed[3])&&payload.Count==12,"Unexpected NVIDIA runtime or package entries");
   Check(typeof(Engine).Assembly.GetType("GroundedDLSSSetup.NvidiaDownload")==null&&typeof(Engine).Assembly.GetType("GroundedDLSSSetup.NvidiaRuntime")==null,"Legacy downloader/import API retained");
  });
  string game=Path.Combine(home,"game"),ini=Path.Combine(home,"config","Engine.ini");
  Directory.CreateDirectory(Path.Combine(game,@"Maine\Binaries\Win64"));Directory.CreateDirectory(Path.GetDirectoryName(ini));
  File.WriteAllText(Path.Combine(game,@"Maine\Binaries\Win64\Maine-Win64-Shipping.exe"),"fixture");
  File.WriteAllText(ini,"[SystemSettings]\r\nr.TemporalAA.Upsampling=0\r\n;user\r\n");byte[] before=File.ReadAllBytes(ini);
  Case("missing-source-does-not-install",()=>{Refused(()=>NvidiaLocalRuntime.Find(Path.Combine(home,"empty-cache"),null,61742));Check(!File.Exists(Path.Combine(game,Engine.Allowed[0]))&&File.ReadAllBytes(ini).SequenceEqual(before),"Missing source mutated installation");});
  Case("wrong-identity-rejected",()=>{string bad=Path.Combine(home,"short.dll");File.WriteAllText(bad,"not a runtime");Refused(()=>NvidiaLocalRuntime.VerifyFile(bad));});
  Case("altered-nvidia-signature-rejected",()=>{string bad=Path.Combine(home,"tampered.dll");byte[] bytes=File.ReadAllBytes(official);bytes[1024*1024]^=1;File.WriteAllBytes(bad,bytes);Refused(()=>NvidiaLocalRuntime.VerifyFile(bad));Check(!File.Exists(Path.Combine(game,Engine.Allowed[0])),"Loader installed despite invalid source");});
  Case("install-update-uninstall-without-private-library",()=>{
   Check(NvidiaLocalRuntime.VerifyFile(official).Version.Major>=2,"Official source rejected");
   var e=new Engine(game,ini,true);e.Install(Package(),null);e.Install(Package(),null);
   Check(!File.Exists(Path.Combine(game,Engine.Allowed[3]))&&File.Exists(Path.Combine(game,Engine.Allowed[0])),"Installer copied a private DLSS library or omitted loader");
   e.Uninstall();Check(File.ReadAllBytes(ini).SequenceEqual(before)&&!File.Exists(Path.Combine(game,Engine.Allowed[0])),"Uninstall failed to restore configuration");Check(File.Exists(official),"NVIDIA source removed");
  });
  Case("license-resources-and-default-consent",()=>{
   foreach(string ending in new[]{"TERMINOS.txt","LICENSE.txt","NVIDIA-RTX-SDK-LICENSE.txt","THIRD-PARTY-NOTICES.txt"})Check(payload.Keys.Any(k=>k.EndsWith("\\"+ending,StringComparison.Ordinal)),"Missing license: "+ending);
   Application.EnableVisualStyles();using(var form=new SetupForm()){
    Check(form.Controls.OfType<CheckBox>().Single().Checked==false,"Acceptance preselected");
    Check(!form.Controls.OfType<Button>().Any(b=>b.Text.IndexOf("descarga",StringComparison.OrdinalIgnoreCase)>=0||b.Text.Contains("DLL")),"Download or manual DLL picker retained");
    Check(form.Controls.OfType<Button>().Any(b=>b.Text=="Ver ayuda, términos y licencias"),"Cannot read terms");
    // Render this new form to a bitmap without opening a window or reading the desktop.
    form.CreateControl();using(var bitmap=new Bitmap(form.Width,form.Height)){
     form.DrawToBitmap(bitmap,new Rectangle(Point.Empty,bitmap.Size));
     // Hidden parents skip child painting in WM_PRINT. Render each actual
     // control separately into its client position, still without Show().
     int border=(form.Width-form.ClientSize.Width)/2,top=form.Height-form.ClientSize.Height-border;
     using(var graphics=Graphics.FromImage(bitmap))foreach(Control control in form.Controls)using(var child=new Bitmap(control.Width,control.Height)){
      control.DrawToBitmap(child,new Rectangle(Point.Empty,child.Size));graphics.DrawImageUnscaled(child,border+control.Left,top+control.Top);
     }
     bitmap.Save(Path.Combine(home,"installer-layout.png"));
    }
   }
  });
  return failed==0?0:1;
 }
}
