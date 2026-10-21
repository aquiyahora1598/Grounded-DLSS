using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Drawing;
using System.Windows.Forms;
using System.Collections.Generic;
using GroundedDLSSSetup;
class InstallerLanguageTests {
 static int failed,total;static string home;
 static void Check(bool ok,string reason){if(!ok)throw new Exception(reason);}
 static void Case(string name,Action action){++total;try{action();Console.WriteLine("PASS "+name);}catch(Exception ex){++failed;Console.WriteLine("FAIL "+name+": "+ex);}}
 static void Render(Form form,string path){
  form.CreateControl();using(var bitmap=new Bitmap(form.Width,form.Height)){
   form.DrawToBitmap(bitmap,new Rectangle(Point.Empty,bitmap.Size));
   int border=(form.Width-form.ClientSize.Width)/2,top=form.Height-form.ClientSize.Height-border;
   using(var graphics=Graphics.FromImage(bitmap))foreach(Control control in form.Controls)using(var child=new Bitmap(control.Width,control.Height)){
    control.DrawToBitmap(child,new Rectangle(Point.Empty,child.Size));graphics.DrawImageUnscaled(child,border+control.Left,top+control.Top);
   }
   bitmap.Save(path);
  }
 }
 [STAThread] static int Main(string[] args){
  Console.OutputEncoding=new UTF8Encoding(false);home=Path.GetFullPath(args[0]);Check(!Directory.Exists(home),"Fresh fixture required");Directory.CreateDirectory(home);Application.EnableVisualStyles();
  Case("language-detects-es-en-pt-and-fallback",()=>{
   Check(I18n.FromCulture("es-AR")=="es"&&I18n.FromCulture("en-US")=="en"&&I18n.FromCulture("pt-BR")=="pt-BR"&&I18n.FromCulture("pt-PT")=="pt-BR"&&I18n.FromCulture("de-DE")=="en","Wrong initial language");
   I18n.SetLanguage("es");bool refused=false;try{I18n.SetLanguage("xx");}catch(ArgumentException){refused=true;}Check(refused&&I18n.Language=="es","Invalid language changed state");
  });
  Case("language-dictionaries-complete-and-roundtrip",()=>{
   foreach(string language in new[]{"es","en","pt-BR"}){I18n.SetLanguage(language);foreach(string key in I18n.Keys){string value=I18n.T(key);Check(!String.IsNullOrWhiteSpace(value),"Empty translation");Check(I18n.Original(value)==key,"Ambiguous translation: "+key);}}
  });
  Case("language-switch-preserves-folder-and-clears-consent",()=>{
   I18n.SetLanguage("es");using(var form=new SetupForm()){
    var choice=form.Controls.OfType<ComboBox>().Single();var folder=form.Controls.OfType<TextBox>().Single(x=>!x.ReadOnly);var consent=form.Controls.OfType<CheckBox>().Single();
    folder.Text=@"C:\Games\Grounded";
    foreach(int index in new[]{1,2,0}){consent.Checked=true;choice.SelectedIndex=index;
     string expected=index==1?"Install / update":index==2?"Instalar / atualizar":"Instalar / actualizar";
     Check(form.Controls.OfType<Button>().Any(x=>x.Text==expected),"Main action not translated");
     Check(folder.Text==@"C:\Games\Grounded"&&!consent.Checked,"Language reset folder or retained acceptance");
    }
   }
  });
  string nvidiaOriginal=null;
  foreach(string language in new[]{"es","en","pt-BR"}){
   string code=language;
   Case("language-help-terms-and-original-nvidia-"+code,()=>{
    I18n.SetLanguage(code);using(var view=SetupForm.CreateTermsView()){
     var tabs=view.Controls.OfType<TabControl>().Single();Check(tabs.TabPages.Count==5,"Help or license tab missing");
     string help=((TextBox)tabs.TabPages[0].Controls[0]).Text,terms=((TextBox)tabs.TabPages[1].Controls[0]).Text,nvidia=((TextBox)tabs.TabPages[3].Controls[0]).Text;
     Check(help.Contains(code=="es"?"INSTALAR O ACTUALIZAR":code=="en"?"INSTALL OR UPDATE":"INSTALAR OU ATUALIZAR"),"Wrong help language");
     Check(terms.Contains(code=="es"?"CODIGO PROPIO":code=="en"?"ORIGINAL PROJECT CODE":"CÓDIGO PRÓPRIO"),"Wrong terms language");
     Check(terms.Contains("Grounded DLSS isn't endorsed by Obsidian"),"Required original attribution missing");
     if(nvidiaOriginal==null)nvidiaOriginal=nvidia;else Check(nvidia==nvidiaOriginal,"NVIDIA license text changed with language");
    }
   });
   Case("language-ui-render-and-translated-errors-"+code,()=>{
    I18n.SetLanguage(code);using(var form=new SetupForm()){
     Check(!form.Controls.OfType<CheckBox>().Single().Checked,"Consent was preselected");
     foreach(var control in form.Controls.OfType<Button>())Check(TextRenderer.MeasureText(control.Text,control.Font).Width<=control.Width-6,"Button text clipped: "+control.Text);
     Render(form,Path.Combine(home,"installer-"+code+".png"));
    }
    string expected=code=="es"?"No se encontró":code=="en"?"No compatible":"Nenhuma biblioteca";
    Check(new MissingRuntimeException().Message.StartsWith(expected,StringComparison.Ordinal),"Missing-library error not translated");
    bool refused=false;try{Safe.Child(home,@"..\escape");}catch(IOException e){refused=true;Check(e.Message==I18n.T("Ruta de paquete inválida."),"Path rejection not translated");}Check(refused,"Path guard changed");
   });
   Case("language-install-uninstall-fsr-logs-"+code,()=>{
    I18n.SetLanguage(code);string root=Path.Combine(home,code),ini=Path.Combine(root,"config","Engine.ini"),menu=Path.Combine(root,"config","GameUserSettings.ini");
    Directory.CreateDirectory(Path.GetDirectoryName(ini));Directory.CreateDirectory(Path.Combine(root,@"Maine\Binaries\Win64"));
    File.WriteAllText(Path.Combine(root,@"Maine\Binaries\Win64\Maine-Win64-Shipping.exe"),"fixture");
    File.WriteAllText(ini,"[SystemSettings]\r\nr.TemporalAA.Upsampling=0\r\n");File.WriteAllText(menu,"[/Script/Maine.MaineGameUserSettings]\r\nFSRQualityLevel=3\r\nMouseSensitivity=0.6\r\n");
    byte[] before=File.ReadAllBytes(menu);var logs=new List<string>();var engine=new Engine(root,ini,true);engine.Log=logs.Add;
    var payload=new Dictionary<string,byte[]> {{Engine.Allowed[0],Encoding.UTF8.GetBytes("loader")},{Engine.Allowed[1],Encoding.UTF8.GetBytes("runtime")}};
    engine.Install(payload,null);Check(Engine.GetKey(File.ReadAllText(menu),"FSRQualityLevel",Engine.GameSettingsSection)=="0","FSR no longer disabled");engine.Uninstall();
    Check(File.ReadAllBytes(menu).SequenceEqual(before),"Uninstall did not restore preferences");
    Check(logs.Any(x=>x.StartsWith(code=="es"?"Instalación completada.":code=="en"?"Installation complete.":"Instalação concluída.",StringComparison.Ordinal)),"Success log not translated");
   });
  }
  I18n.SetLanguage("es");Console.WriteLine("Installer language regressions: "+(total-failed)+" passed, "+failed+" failed.");return failed==0?0:1;
 }
}
