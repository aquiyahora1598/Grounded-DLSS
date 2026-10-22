// NVIDIA DLSS is discovered locally. No download or backup-library installation.
using System;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using System.Threading;
using System.Windows.Forms;
using System.Xml.Serialization;

namespace GroundedDLSSSetup {
 public class Item { public string Path, Hash, Backup; public bool Active=true, Config; }
 public class Key { public string Name, Before, After; }
 public class State {
  public int Format=4; public string Root, EnginePath, EngineBeforeHash, EngineAfterHash, SelfHash; public bool EngineExisted,EngineUserChangesSeen;
  public string SettingsBeforeHash,SettingsAfterHash,FsrBefore; public bool SettingsExisted,SettingsUserChangesSeen;
  public List<Item> Items=new List<Item>(); public List<Key> Keys=new List<Key>();
 }
 public sealed class Transaction : IDisposable {
  readonly string dir; readonly List<string> paths=new List<string>(); readonly List<byte[]> old=new List<byte[]>();
  bool committed;
  public Transaction(string folder) { dir=folder; Directory.CreateDirectory(dir); }
  public void Write(string path,byte[] bytes) {
   Safe.Check(path); if(!paths.Contains(path,StringComparer.OrdinalIgnoreCase)) {
    int n=paths.Count; byte[] before=File.Exists(path)?File.ReadAllBytes(path):null;
    if(before!=null)using(var f=new FileStream(System.IO.Path.Combine(dir,n+".before"),FileMode.Create,FileAccess.Write,FileShare.None)){f.Write(before,0,before.Length);f.Flush(true);}
    // Journal precedes mutation. A later run can recover an interrupted operation.
    Append(System.IO.Path.Combine(dir,"journal.txt"),Convert.ToBase64String(Encoding.UTF8.GetBytes(path))+"|"+(before==null?"new":n+".before"));
    paths.Add(path);old.Add(before);
   }
   if(bytes==null){if(File.Exists(path))File.Delete(path);return;}
   Directory.CreateDirectory(System.IO.Path.GetDirectoryName(path));
   // A fixed temporary filename survived a process interruption and prevented
   // every later install. Record a unique owned name before creating it.
   string temp=path+".grounded-dlss-"+Guid.NewGuid().ToString("N")+".tmp";
   Append(System.IO.Path.Combine(dir,"staging.txt"),Convert.ToBase64String(Encoding.UTF8.GetBytes(temp)));
   bool created=false;
   try {using(var f=new FileStream(temp,FileMode.CreateNew,FileAccess.Write,FileShare.None)){created=true;f.Write(bytes,0,bytes.Length);}if(File.Exists(path))File.Replace(temp,path,null);else File.Move(temp,path);}
   finally {if(created&&File.Exists(temp))File.Delete(temp);}
  }
  public void Commit(){File.WriteAllText(System.IO.Path.Combine(dir,"committed"),"ok");committed=true;}
  // Each record reaches the disk before the change it describes begins.
  static void Append(string file,string line){
   byte[] bytes=Encoding.UTF8.GetBytes(line+"\r\n");
   using(var f=new FileStream(file,FileMode.Append,FileAccess.Write,FileShare.Read)){f.Write(bytes,0,bytes.Length);f.Flush(true);}
  }
  // A record is written before its change starts, so an unterminated last
  // record (power loss mid-append) describes a change that never happened.
  // Every terminated record must still be valid.
  static string Decode(string value){try{return Encoding.UTF8.GetString(Convert.FromBase64String(value));}catch(FormatException){throw new IOException(I18n.T("Registro de recuperación inválido."));}}
  public static List<string> Records(string file){
   string text=Encoding.UTF8.GetString(File.ReadAllBytes(file));
   var lines=text.Split('\n').ToList();lines.RemoveAt(lines.Count-1);
   return lines.Select(x=>x.TrimEnd('\r')).ToList();
  }
  public void Dispose(){
   if(!committed)for(int i=paths.Count-1;i>=0;--i){if(old[i]==null){if(File.Exists(paths[i]))File.Delete(paths[i]);}else File.WriteAllBytes(paths[i],old[i]);}
   Directory.Delete(dir,true);
  }
  public static void Recover(string dir,Action<string> validate) {
   Safe.Check(dir); if(!Directory.Exists(dir))return;
   var journal=System.IO.Path.Combine(dir,"journal.txt");
   var staging=System.IO.Path.Combine(dir,"staging.txt");var committed=System.IO.Path.Combine(dir,"committed");
   Safe.Check(journal);Safe.Check(staging);Safe.Check(committed);
   var ownedTemps=new List<string>();
   if(File.Exists(staging))foreach(var line in Records(staging)){
    string temp=Decode(line);const string marker=".grounded-dlss-";
    int index=temp.LastIndexOf(marker,StringComparison.Ordinal);string suffix=index<0?"":temp.Substring(index+marker.Length);
    if(index<0||suffix.Length!=36||!suffix.EndsWith(".tmp",StringComparison.Ordinal)||!suffix.Substring(0,32).All(Uri.IsHexDigit))throw new IOException(I18n.T("Registro de archivo temporal inválido."));
    Safe.Check(temp);validate(temp.Substring(0,index));ownedTemps.Add(temp);
   }
   if(!File.Exists(committed)&&File.Exists(journal)) {
    // Validate and read every entry before restoring any file. A bad or torn
    // later entry must not leave a partially applied recovery operation.
    var restore=new List<Tuple<string,byte[]>>();
    foreach(var line in Enumerable.Reverse(Records(journal))) {
     var p=line.Split('|'); if(p.Length!=2)throw new IOException(I18n.T("Registro de recuperación inválido."));
     string file=Decode(p[0]);Safe.Check(file);validate(file);
     if(p[1]=="new")restore.Add(Tuple.Create(file,(byte[])null));
     else {if(p[1]!=System.IO.Path.GetFileName(p[1])||!p[1].EndsWith(".before"))throw new IOException(I18n.T("Respaldo inválido."));restore.Add(Tuple.Create(file,File.ReadAllBytes(Safe.Child(dir,p[1]))));}
    }
    foreach(var item in restore){
     if(item.Item2==null){if(File.Exists(item.Item1))File.Delete(item.Item1);}
     else File.WriteAllBytes(item.Item1,item.Item2);
    }
   }
   foreach(var temp in ownedTemps)if(File.Exists(temp))File.Delete(temp);
   Directory.Delete(dir,true);
  }
 }
 public static class Safe {
  public static string Hash(byte[] b){using(var h=SHA256.Create())return BitConverter.ToString(h.ComputeHash(b)).Replace("-","").ToLowerInvariant();}
  public static string HashFile(string p){using(var h=SHA256.Create())using(var f=File.OpenRead(p))return BitConverter.ToString(h.ComputeHash(f)).Replace("-","").ToLowerInvariant();}
  public static void Check(string path){
   string p=System.IO.Path.GetFullPath(path);
   if(p.StartsWith(@"\\")||p.Substring(2).Contains(":"))throw new IOException(I18n.T("Usá una carpeta local sin rutas alternativas."));
   for(string q=p;!String.IsNullOrEmpty(q);q=System.IO.Path.GetDirectoryName(q))
    if((Directory.Exists(q)||File.Exists(q))&&(File.GetAttributes(q)&FileAttributes.ReparsePoint)!=0)throw new IOException(I18n.T("No se admiten enlaces o carpetas redirigidas: ")+q);
  }
  public static string Child(string root,string relative) {
   if(String.IsNullOrWhiteSpace(relative)||System.IO.Path.IsPathRooted(relative)||relative.Split('/','\\').Any(x=>x==".."||x=="."||x.Length==0))throw new IOException(I18n.T("Ruta de paquete inválida."));
   string p=System.IO.Path.GetFullPath(System.IO.Path.Combine(root,relative));
   if(!p.StartsWith(root.TrimEnd('\\')+"\\",StringComparison.OrdinalIgnoreCase))throw new IOException(I18n.T("Ruta fuera del destino."));
   Check(p);return p;
  }
 }
 public sealed class Engine {
  public const string ExeHash="5dbc5dd94a2d222e674aaa86bdb195de0cd0b0f14fe765afb6b37dbda8b06306";
  public static readonly string[] Allowed={@"Maine\Binaries\Win64\dxgi.dll",@"Maine\Binaries\Win64\plugins\GroundedDLSS.asi",@"Maine\Binaries\Win64\plugins\GroundedDLSS.ini",@"Maine\Binaries\Win64\plugins\GroundedDLSS-runtime\nvngx_dlss.dll",@"Maine\Binaries\Win64\GroundedDLSS-install\LEEME.txt",@"Maine\Binaries\Win64\GroundedDLSS-install\NVIDIA-RTX-SDK-LICENSE.txt",@"Maine\Binaries\Win64\GroundedDLSS-install\THIRD-PARTY-NOTICES.txt",@"Maine\Binaries\Win64\GroundedDLSS-install\LICENSE.txt",@"Maine\Binaries\Win64\GroundedDLSS-install\TERMINOS.txt",@"Maine\Binaries\Win64\GroundedDLSS-install\LEEME.en.txt",@"Maine\Binaries\Win64\GroundedDLSS-install\LEEME.pt-BR.txt",@"Maine\Binaries\Win64\GroundedDLSS-install\TERMINOS.en.txt",@"Maine\Binaries\Win64\GroundedDLSS-install\TERMINOS.pt-BR.txt",@"Maine\Binaries\Win64\plugins\FSRFrameGeneration\amd_fidelityfx_loader_dx12.dll",@"Maine\Binaries\Win64\plugins\FSRFrameGeneration\amd_fidelityfx_framegeneration_dx12.dll",@"Maine\Binaries\Win64\GroundedDLSS-install\AMD-FidelityFX-SDK-LICENSE.txt"};
  public const string GameSettingsSection="/Script/Maine.MaineGameUserSettings";
  readonly string root, data, stateFile, engine, settings, txdir; readonly bool fixture;
  public Action<string> Log=delegate{}; public Action<int> Fault;
  sealed class Gate : IDisposable {
   readonly Mutex mutex;
   public Gate(string root){mutex=new Mutex(false,"Local\\GroundedDLSS-"+Safe.Hash(Encoding.UTF8.GetBytes(root.ToUpperInvariant())));bool held=false;try{held=mutex.WaitOne(0);}catch(AbandonedMutexException){held=true;}if(!held){mutex.Dispose();throw new IOException(I18n.T("Otra operación de Grounded DLSS sigue abierta."));}}
   public void Dispose(){mutex.ReleaseMutex();mutex.Dispose();}
  }
  public Engine(string game,string enginePath,bool test=false) {
   root=System.IO.Path.GetFullPath(game).TrimEnd('\\');Safe.Check(root);
   data=Safe.Child(root,@"Maine\Binaries\Win64\GroundedDLSS-install");
   stateFile=Safe.Child(data,"state.xml");txdir=Safe.Child(data,"transaction");engine=System.IO.Path.GetFullPath(enginePath);Safe.Check(engine);fixture=test;
   settings=Safe.Child(System.IO.Path.GetDirectoryName(engine),"GameUserSettings.ini");
  }
  void Guard(bool installing) {
   if(!fixture&&Process.GetProcessesByName("Maine-Win64-Shipping").Length>0)throw new IOException(I18n.T("Cerrá Grounded normalmente antes de continuar."));
   foreach(string file in new[]{Allowed[0],Allowed[1]})
    if(File.Exists(Safe.Child(root,file+".grounded-test-off")))throw new IOException(I18n.T("El selector de pruebas dejó el mod desactivado. Con el juego cerrado, ejecutá primero «1 - DLSS.bat» antes de actualizar o desinstalar."));
   string exe=Safe.Child(root,@"Maine\Binaries\Win64\Maine-Win64-Shipping.exe");
   if(installing&&(!File.Exists(exe)||(!fixture&&Safe.HashFile(exe)!=ExeHash)))throw new IOException(I18n.T("Se requiere Grounded para Steam 1.4.7.4815 (ejecutable verificado). Esta versión del juego no es compatible."));
   foreach(string name in new[]{"d3d12.dll","d3d11.dll","version.dll","winmm.dll","winhttp.dll","dinput8.dll","d3d9.dll","dsound.dll","xinput1_3.dll","xinput9_1_0.dll"})
    if(installing&&File.Exists(Safe.Child(root,@"Maine\Binaries\Win64\"+name)))throw new IOException(I18n.T("Se encontró otro posible cargador: ")+name+I18n.T(". Revisá su compatibilidad antes de instalar."));
   if(Directory.Exists(txdir))throw new IOException(I18n.T("Hay una operación interrumpida. Usá Recuperar antes de continuar."));
  }
  State Read() {
   if(!File.Exists(stateFile))return new State {Root=root,EnginePath=engine};
   State s;using(var f=File.OpenRead(stateFile))s=(State)new XmlSerializer(typeof(State)).Deserialize(f);
   if((s.Format!=1&&s.Format!=2&&s.Format!=3&&s.Format!=4)||!String.Equals(s.Root,root,StringComparison.OrdinalIgnoreCase)||!String.Equals(s.EnginePath,engine,StringComparison.OrdinalIgnoreCase))throw new IOException(I18n.T("El registro de instalación no corresponde a esta carpeta o usuario."));
   // Format 2 preserves changes made between install and update. Older setup
   // versions must refuse this state rather than ignore that preservation flag.
   // Format 3 also owns the FSR setting. Older installers must refuse it rather
   // than uninstall without restoring the separately recorded menu preference.
   // Format 4 owns the AMD FG dependencies. Old uninstallers must refuse it.
   s.Format=4;
   foreach(var x in s.Items){if(!Allowed.Contains(x.Path,StringComparer.OrdinalIgnoreCase)||x.Backup!=null)throw new IOException(I18n.T("Registro de archivos inválido."));Safe.Child(root,x.Path);}
   string[] keys={"r.DefaultFeature.AntiAliasing","r.PostProcessAAQuality","r.TemporalAA.Upsampling","r.TemporalAASamples","r.VelocityOutputPass","r.VertexDeformationOutputsVelocity"};
   if(s.Keys.Any(k=>!keys.Contains(k.Name)))throw new IOException(I18n.T("Registro de ajustes inválido."));
   return s;
  }
  static byte[] Serialize(State s){using(var m=new MemoryStream()){new XmlSerializer(typeof(State)).Serialize(m,s);return m.ToArray();}}
  static string[] Lines(string t){return t.Replace("\r\n","\n").Split('\n');}
  // Unreal trims spaces around '=', so "key = value" names the same setting.
  static bool IsKey(string t,string key){int eq=t.IndexOf('=');return eq>0&&t.Substring(0,eq).Trim().Equals(key,StringComparison.OrdinalIgnoreCase);}
  public static string GetKey(string text,string key) {return GetKey(text,key,"SystemSettings");}
  // Section names come from code, never from the installation journal.
  public static string GetKey(string text,string key,string sectionName) {
   bool section=false;string value=null;
   foreach(var line in Lines(text)){string t=line.Trim();if(t.StartsWith("[")){section=t.Equals("["+sectionName+"]",StringComparison.OrdinalIgnoreCase);continue;}
    if(section&&IsKey(t,key))value=t.Substring(t.IndexOf('=')+1).Trim();}
   return value;
  }
  // A new key goes after the section's last setting, not after its blank separator.
  static void AddAtSectionEnd(List<string> lines,string line){int i=lines.Count;while(i>0&&lines[i-1].Trim().Length==0)i--;lines.Insert(i,line);}
  public static string PutKey(string text,string key,string value) {return PutKey(text,key,value,"SystemSettings");}
  public static string PutKey(string text,string key,string value,string sectionName) {
   var result=new List<string>();bool section=false,found=false,inserted=false;
   // Keep the file's own line endings; new files use CRLF like Unreal.
   string newline=text.Contains("\n")&&!text.Contains("\r\n")?"\n":"\r\n";
   // Only edited lines change: blank lines and the final line break stay as they were,
   // so an Engine.ini that already has our values is written back byte for byte.
   bool terminated=text.Length==0||text.EndsWith("\n");
   var lines=text.Length==0?new string[0]:Lines(text);if(terminated&&lines.Length>0)lines=lines.Take(lines.Length-1).ToArray();
   foreach(var line in lines){string t=line.Trim();
    if(t.StartsWith("[")){if(section&&!inserted&&value!=null){AddAtSectionEnd(result,key+"="+value);inserted=true;}section=t.Equals("["+sectionName+"]",StringComparison.OrdinalIgnoreCase);found|=section;}
    if(section&&IsKey(t,key)){
     // Replace the first occurrence in place. Removing then appending on each
     // update stranded the old blank separator and grew Engine.ini forever.
     if(value!=null&&!inserted){result.Add(key+"="+value);inserted=true;}
     continue;
    }result.Add(line);
   }
   if(value!=null&&!inserted){if(!found||!section){result.Add("["+sectionName+"]");result.Add(key+"="+value);}else AddAtSectionEnd(result,key+"="+value);}
   return result.Count==0?"":String.Join(newline,result)+(terminated?newline:"");
  }
  static string Text(byte[] b){using(var r=new StreamReader(new MemoryStream(b),Encoding.UTF8,true))return r.ReadToEnd();}
  // Write Engine.ini back in the encoding it was read in (BOM included).
  public static byte[] Bytes(string text,byte[] original){
   Encoding e=new UTF8Encoding(false);
   if(original!=null&&original.Length>=2){
    if(original[0]==0xFF&&original[1]==0xFE)e=new UnicodeEncoding(false,true);
    else if(original[0]==0xFE&&original[1]==0xFF)e=new UnicodeEncoding(true,true);
    else if(original.Length>=3&&original[0]==0xEF&&original[1]==0xBB&&original[2]==0xBF)e=new UTF8Encoding(true);
   }
   var preamble=e.GetPreamble();var body=e.GetBytes(text);
   var result=new byte[preamble.Length+body.Length];Buffer.BlockCopy(preamble,0,result,0,preamble.Length);Buffer.BlockCopy(body,0,result,preamble.Length,body.Length);
   return result;
  }
  void ValidateJournal(string p){if(p.Equals(engine,StringComparison.OrdinalIgnoreCase)||p.Equals(settings,StringComparison.OrdinalIgnoreCase)||p.Equals(stateFile,StringComparison.OrdinalIgnoreCase)||p.Equals(Safe.Child(data,"Engine.ini.before"),StringComparison.OrdinalIgnoreCase)||p.Equals(Safe.Child(data,"GameUserSettings.ini.before"),StringComparison.OrdinalIgnoreCase)||p.Equals(Safe.Child(data,"Desinstalar Grounded DLSS.exe"),StringComparison.OrdinalIgnoreCase)||Allowed.Any(a=>p.Equals(Safe.Child(root,a),StringComparison.OrdinalIgnoreCase))||new[]{NvidiaLocalRuntime.AutoDll,NvidiaLocalRuntime.AutoOwner}.Any(a=>p.Equals(Safe.Child(root,a),StringComparison.OrdinalIgnoreCase)))return;throw new IOException(I18n.T("El registro intenta acceder a un archivo ajeno. Se conserva para revisión."));}
  public void Recover(){using(var gate=new Gate(root)){if(!fixture&&Process.GetProcessesByName("Maine-Win64-Shipping").Length>0)throw new IOException(I18n.T("Cerrá Grounded antes de recuperar."));Transaction.Recover(txdir,ValidateJournal);Log(I18n.T("Operación interrumpida recuperada."));}}
  public void CheckCanInstall(){using(var gate=new Gate(root)){Guard(true);Read();}}
  public void Install(Dictionary<string,byte[]> payload,byte[] self){using(var gate=new Gate(root)){InstallCore(payload,self);}}
  void InstallCore(Dictionary<string,byte[]> payload, byte[] self) {
   Guard(true);State s=Read();
   if(payload.ContainsKey(Allowed[3]))throw new IOException(I18n.T("El instalador no instala bibliotecas NVIDIA ni copias de respaldo."));
   // Validate every collision and path before writing anything.
   foreach(var p in payload){if(!Allowed.Contains(p.Key,StringComparer.OrdinalIgnoreCase))throw new IOException(I18n.T("Archivo inesperado en el paquete: ")+p.Key);string dest=Safe.Child(root,p.Key);var item=s.Items.SingleOrDefault(x=>x.Path.Equals(p.Key,StringComparison.OrdinalIgnoreCase)&&x.Active);
    if(File.Exists(dest)&&item==null)throw new IOException(I18n.T("Ya existe un archivo ajeno al instalador: ")+p.Key+I18n.T(". Retirá la instalación anterior o el otro mod primero."));
    if(File.Exists(dest)&&item!=null&&!item.Config&&Safe.HashFile(dest)!=item.Hash)throw new IOException(I18n.T("Un archivo instalado fue modificado. Se conserva: ")+p.Key);
   }
   string selfPath=Safe.Child(data,"Desinstalar Grounded DLSS.exe");
   string selfHash=self==null?null:Safe.Hash(self),installedSelfHash=File.Exists(selfPath)?Safe.HashFile(selfPath):null;
   if(self!=null&&installedSelfHash!=null&&installedSelfHash!=s.SelfHash)throw new IOException(I18n.T("Existe otro desinstalador o fue modificado. Se conserva."));
   Directory.CreateDirectory(data);
   using(var tx=new Transaction(txdir)) {
    // Retire the legacy private library only when it still matches our record.
    foreach(var item in s.Items.Where(x=>x.Active&&x.Path.Equals(Allowed[3],StringComparison.OrdinalIgnoreCase))){
     string old=Safe.Child(root,item.Path);
     if(!File.Exists(old)||Safe.HashFile(old)==item.Hash){tx.Write(old,null);item.Active=false;}
     else Log(I18n.T("Se conserva la biblioteca anterior porque fue modificada: ")+item.Path);
    }
    if(NvidiaLocalRuntime.CanRemoveAutomatic(root)){
     tx.Write(Safe.Child(root,NvidiaLocalRuntime.AutoDll),null);tx.Write(Safe.Child(root,NvidiaLocalRuntime.AutoOwner),null);
    }
    int n=0;
    foreach(var p in payload){string dest=Safe.Child(root,p.Key);var item=s.Items.SingleOrDefault(x=>x.Path.Equals(p.Key,StringComparison.OrdinalIgnoreCase)&&x.Active);
     if(item!=null&&item.Config&&File.Exists(dest)){Log(I18n.T("Se conserva tu configuración DLSS."));continue;}
     if(item==null){item=new Item {Path=p.Key,Config=p.Key.EndsWith("GroundedDLSS.ini",StringComparison.OrdinalIgnoreCase)};s.Items.Add(item);}
     tx.Write(dest,p.Value);item.Hash=Safe.Hash(p.Value);if(Fault!=null)Fault(++n);
    }
    byte[] before=File.Exists(engine)?File.ReadAllBytes(engine):new byte[0];string text=Text(before);
    if(s.EngineBeforeHash==null){s.EngineExisted=File.Exists(engine);s.EngineBeforeHash=Safe.Hash(before);tx.Write(Safe.Child(data,"Engine.ini.before"),before);
     s.EngineUserChangesSeen=false;
     string[] keys={"r.DefaultFeature.AntiAliasing","r.PostProcessAAQuality","r.TemporalAA.Upsampling","r.TemporalAASamples","r.VelocityOutputPass","r.VertexDeformationOutputsVelocity"};
     string[] values={"2","3","1","8","1","1"};for(int i=0;i<keys.Length;++i)s.Keys.Add(new Key{Name=keys[i],Before=GetKey(text,keys[i]),After=values[i]});
    }
    else if(Safe.Hash(before)!=s.EngineAfterHash){
     // The original whole-file backup predates these edits. Keep it for
     // reference, but merge on uninstall and preserve the latest user values.
     s.EngineUserChangesSeen=true;
     foreach(var k in s.Keys){string current=GetKey(text,k.Name);if(current!=k.After)k.Before=current;}
    }
    foreach(var k in s.Keys)text=PutKey(text,k.Name,k.After);
    byte[] after=Bytes(text,before);tx.Write(engine,after);s.EngineAfterHash=Safe.Hash(after);
    // Disable only the game's FSR menu option. Keep resolution, render scale,
    // graphics quality, controls and every unrelated preference intact.
    byte[] menuBefore=File.Exists(settings)?File.ReadAllBytes(settings):new byte[0];string menu=Text(menuBefore);
    string fsr=GetKey(menu,"FSRQualityLevel",GameSettingsSection);
    if(s.SettingsBeforeHash==null){
     s.SettingsExisted=File.Exists(settings);s.SettingsBeforeHash=Safe.Hash(menuBefore);
     s.FsrBefore=fsr;s.SettingsUserChangesSeen=false;tx.Write(Safe.Child(data,"GameUserSettings.ini.before"),menuBefore);
    }else if(Safe.Hash(menuBefore)!=s.SettingsAfterHash){
     s.SettingsUserChangesSeen=true;
     if(fsr!="0")s.FsrBefore=fsr;
    }
    byte[] menuAfter=fsr=="0"?menuBefore:Bytes(PutKey(menu,"FSRQualityLevel","0",GameSettingsSection),menuBefore);
    if(!menuAfter.SequenceEqual(menuBefore)||!File.Exists(settings))tx.Write(settings,menuAfter);
    s.SettingsAfterHash=Safe.Hash(menuAfter);
    if(Fault!=null)Fault(++n);
    // The installed copy can also open this UI. Windows locks a running image;
    // no replacement is needed when it is already the exact same installer.
    if(self!=null){if(installedSelfHash!=selfHash)tx.Write(selfPath,self);s.SelfHash=selfHash;}
    tx.Write(stateFile,Serialize(s));tx.Commit();
   }
   Log(I18n.T("Instalación completada. F8: activar/desactivar DLSS/DLAA. F9: activar/desactivar FSR Frame Generation. El escalado FSR del menú quedó apagado. Se conservó tu selección anterior para desinstalar. Iniciá con DirectX 12."));
  }
  public void Uninstall(){using(var gate=new Gate(root)){UninstallCore();}}
  void UninstallCore() {
   Guard(false);State s=Read();if(!File.Exists(stateFile))throw new IOException(I18n.T("No hay una instalación registrada en esta carpeta."));
   string backup=Safe.Child(data,"Engine.ini.before");
   if(s.EngineBeforeHash!=null&&File.Exists(backup)&&Safe.HashFile(backup)!=s.EngineBeforeHash)
    throw new IOException(I18n.T("El respaldo de configuración fue modificado o está dañado. Se conserva la instalación para revisarlo."));
   string settingsBackup=Safe.Child(data,"GameUserSettings.ini.before");
   if(s.SettingsBeforeHash!=null&&(!File.Exists(settingsBackup)||Safe.HashFile(settingsBackup)!=s.SettingsBeforeHash))
    throw new IOException(I18n.T("El respaldo de opciones del juego falta o está dañado. Se conserva la instalación para revisarlo."));
   using(var tx=new Transaction(txdir)) {
    if(NvidiaLocalRuntime.CanRemoveAutomatic(root)){
     tx.Write(Safe.Child(root,NvidiaLocalRuntime.AutoDll),null);tx.Write(Safe.Child(root,NvidiaLocalRuntime.AutoOwner),null);
    }else if(File.Exists(Safe.Child(root,NvidiaLocalRuntime.AutoDll)))Log(I18n.T("Se conserva la biblioteca automática porque cambió o perdió su firma NVIDIA."));
    foreach(var x in s.Items.Where(x=>x.Active)){string dest=Safe.Child(root,x.Path);
     if(File.Exists(dest)&&Safe.HashFile(dest)!=x.Hash){Log(I18n.T("Conservado porque cambió: ")+x.Path);continue;}
     tx.Write(dest,x.Backup==null?null:File.ReadAllBytes(Safe.Child(data,x.Backup)));x.Active=false;
    }
    if(s.EngineBeforeHash!=null&&File.Exists(engine)) {
     byte[] current=File.ReadAllBytes(engine);string text=Text(current);
     if(!s.EngineUserChangesSeen&&Safe.Hash(current)==s.EngineAfterHash&&File.Exists(Safe.Child(data,"Engine.ini.before")))tx.Write(engine,s.EngineExisted?File.ReadAllBytes(Safe.Child(data,"Engine.ini.before")):null);
     else {foreach(var k in s.Keys){if(GetKey(text,k.Name)==k.After)text=PutKey(text,k.Name,k.Before);else Log(I18n.T("Se conserva el ajuste que cambiaste: ")+k.Name);}tx.Write(engine,Bytes(text,current));}
    }
    if(s.SettingsBeforeHash!=null&&File.Exists(settings)){
     byte[] current=File.ReadAllBytes(settings);string text=Text(current);
     if(!s.SettingsUserChangesSeen&&Safe.Hash(current)==s.SettingsAfterHash)tx.Write(settings,s.SettingsExisted?File.ReadAllBytes(settingsBackup):null);
     else if(GetKey(text,"FSRQualityLevel",GameSettingsSection)=="0"){
      string restored=PutKey(text,"FSRQualityLevel",s.FsrBefore,GameSettingsSection);
      if(restored!=text)tx.Write(settings,Bytes(restored,current));
     }else Log(I18n.T("Se conserva la selección FSR que cambiaste después de instalar."));
    }
    s.SettingsBeforeHash=null;s.SettingsAfterHash=null;s.FsrBefore=null;s.SettingsUserChangesSeen=false;
    s.Keys.Clear();s.EngineBeforeHash=null;s.EngineAfterHash=null;s.EngineUserChangesSeen=false;tx.Write(stateFile,Serialize(s));tx.Commit();
   }
   Log(I18n.T("Mod desinstalado. Los archivos modificados, respaldos y este desinstalador se conservan en GroundedDLSS-install."));
  }
 }
 public sealed class MissingRuntimeException : IOException {public MissingRuntimeException():base(I18n.T("No se encontró DLSS de producción compatible en los componentes locales de NVIDIA. No se hizo ningún cambio. Este instalador no descarga bibliotecas; revisá el controlador y los componentes de NVIDIA.")){} }
 public class SetupForm : Form {
  readonly TextBox folder=new TextBox(), log=new TextBox(); readonly Button install=new Button(), remove=new Button(),recover=new Button();readonly CheckBox consent=new CheckBox();
  readonly ComboBox languageSelector=new ComboBox {DropDownStyle=ComboBoxStyle.DropDownList,Name="InstallerLanguage"};
  bool busy;
  string EnginePath {get{return System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),@"Maine\Saved\Config\WindowsNoEditor\Engine.ini");}}
  public SetupForm() {
   Text=I18n.T("Grounded DLSS — beta, revisión legal pendiente");ClientSize=new Size(760,796);MinimumSize=Size;StartPosition=FormStartPosition.CenterScreen;Font=new Font("Segoe UI",10);
   Controls.Add(new Label {Text="Grounded DLSS · 11.24.0 Beta",Font=new Font("Segoe UI",19,FontStyle.Bold),Left=24,Top=20,Width=425,Height=40});
   Controls.Add(new Label {Text=I18n.T("Idioma"),Left=455,Top=32,Width=80,Height=25});
   languageSelector.SetBounds(535,27,199,30);languageSelector.Items.AddRange(new object[]{"Español","English","Português (Brasil)"});
   languageSelector.SelectedIndex=Array.IndexOf(new[]{"es","en","pt-BR"},I18n.Language);Controls.Add(languageSelector);
   Controls.Add(new Label {Text="Steam 1.4.7.4815 · Windows 10/11 x64 · NVIDIA RTX · DirectX 12",Left=24,Top=70,Width=680,Height=30});
   Controls.Add(new Label {Name="ShortcutDLSS",Text=I18n.T("F8  ·  ACTIVAR / DESACTIVAR DLSS / DLAA"),Font=new Font("Segoe UI",14,FontStyle.Bold),BackColor=Color.FromArgb(24,65,112),ForeColor=Color.White,Padding=new Padding(14,0,0,0),TextAlign=ContentAlignment.MiddleLeft,Left=24,Top=106,Width=710,Height=44});
   Controls.Add(new Label {Name="ShortcutFrameGeneration",Text=I18n.T("F9  ·  ACTIVAR / DESACTIVAR FSR FRAME GENERATION"),Font=new Font("Segoe UI",14,FontStyle.Bold),BackColor=Color.FromArgb(24,96,64),ForeColor=Color.White,Padding=new Padding(14,0,0,0),TextAlign=ContentAlignment.MiddleLeft,Left=24,Top=158,Width=710,Height=44});
   Controls.Add(new TextBox {ReadOnly=true,Multiline=true,ScrollBars=ScrollBars.Vertical,Left=24,Top=222,Width=710,Height=145,Text=I18n.T("Beta: revisión de licencias y marcas pendiente.\r\nMod no oficial, sin garantía; no afiliado a NVIDIA, Obsidian ni Xbox.\r\nCerrá el juego. Conservá una copia de tus partidas. El escalado FSR del menú se desactiva automáticamente al instalar; se restaura al desinstalar si no cambiaste esa opción.\r\nLa nitidez está desactivada. Ultra Rendimiento puede perder mucho detalle. La profundidad de campo sigue en evaluación.\r\nInstalador sin firma digital. No cambia drivers, seguridad ni partidas. Usa los componentes locales de NVIDIA; no descarga DLSS ni instala bibliotecas de respaldo.")});
   Controls.Add(new Label {Text=I18n.T("Carpeta del juego (la que contiene Grounded.exe):"),Left=24,Top=383,Width=650,Height=25});
   folder.SetBounds(24,412,560,28);Controls.Add(folder);folder.Text=Detect();
   var browse=new Button {Text=I18n.T("Buscar…"),Left=594,Top=410,Width=100,Height=32};browse.Click+=delegate{using(var d=new FolderBrowserDialog()){d.Description=I18n.T("Elegí la carpeta Grounded");d.SelectedPath=folder.Text;if(d.ShowDialog()==DialogResult.OK)folder.Text=d.SelectedPath;}};Controls.Add(browse);
   Controls.Add(new Label {Text=I18n.T("DLSS se detecta automáticamente entre los componentes de NVIDIA. Si falta una biblioteca compatible, la instalación se detiene sin modificar el juego."),Left=24,Top=453,Width=710,Height=48});
   var terms=new Button {Text=I18n.T("Ver ayuda, términos y licencias"),Left=24,Top=511,Width=270,Height=32};terms.Click+=delegate{using(var view=CreateTermsView())view.ShowDialog(this);};Controls.Add(terms);
   consent.Text=I18n.T("Leí y acepto los términos del mod y las licencias NVIDIA y AMD aplicables.");consent.SetBounds(24,556,710,32);Controls.Add(consent);
   install.Text=I18n.T("Instalar / actualizar");install.SetBounds(24,601,216,36);install.Click+=async delegate {if(!consent.Checked){MessageBox.Show(this,I18n.T("Leé los términos y las licencias NVIDIA y AMD antes de aceptar e instalar."));return;}await Run(true,false);};Controls.Add(install);
   remove.Text=I18n.T("Desinstalar");remove.SetBounds(250,601,216,36);remove.Click+=async delegate{await Run(false,false);};Controls.Add(remove);
   recover.Text=I18n.T("Recuperar operación");recover.SetBounds(476,601,258,36);recover.Click+=async delegate{await Run(false,true);};Controls.Add(recover);
   log.SetBounds(24,656,710,115);log.Multiline=true;log.ReadOnly=true;log.ScrollBars=ScrollBars.Vertical;Controls.Add(log);
   FormClosing+=delegate(object sender,FormClosingEventArgs args){if(busy){args.Cancel=true;log.AppendText(I18n.T("Esperá a que termine la operación antes de cerrar.")+Environment.NewLine);}};
   // Bind only translatable captions. Never reset the selected folder or logs
   // when the user switches languages, and never preselect legal consent.
   foreach(Control control in Controls)if(I18n.Contains(control.Text))control.Tag=I18n.Original(control.Text);
   languageSelector.SelectedIndexChanged+=delegate {
    I18n.SetLanguage(new[]{"es","en","pt-BR"}[languageSelector.SelectedIndex]);
    Text=I18n.T("Grounded DLSS — beta, revisión legal pendiente");
    foreach(Control control in Controls)if(control.Tag is string)control.Text=I18n.T((string)control.Tag);
    consent.Checked=false;
   };
  }
  public static Form CreateTermsView(){
   var view=new Form {Text=I18n.T("Ayuda, términos, licencias y atribuciones"),Width=820,Height=650,StartPosition=FormStartPosition.CenterParent};
   try {
    var tabs=new TabControl {Dock=DockStyle.Fill};view.Controls.Add(tabs);
    using(var resource=Assembly.GetExecutingAssembly().GetManifestResourceStream("payload.zip"))using(var zip=new ZipArchive(resource,ZipArchiveMode.Read))
     foreach(var doc in new[]{new[]{I18n.Document("LEEME"),"Ayuda"},new[]{I18n.Document("TERMINOS"),"Términos del mod"},new[]{"LICENSE.txt","Licencia MIT (original)"},new[]{"NVIDIA-RTX-SDK-LICENSE.txt","NVIDIA RTX SDK (inglés original)"},new[]{"THIRD-PARTY-NOTICES.txt","Avisos de terceros (original)"},new[]{"AMD-FidelityFX-SDK-LICENSE.txt","AMD FidelityFX SDK (inglés original)"}}){
      string name=doc[0];
      var entry=zip.Entries.Single(e=>e.FullName.EndsWith("/"+name,StringComparison.Ordinal));
      using(var reader=new StreamReader(entry.Open(),Encoding.UTF8,true)){var tab=new TabPage(I18n.T(doc[1]));tab.Controls.Add(new TextBox {Multiline=true,ReadOnly=true,ScrollBars=ScrollBars.Both,Dock=DockStyle.Fill,Text=reader.ReadToEnd(),Font=new Font("Segoe UI",10)});tabs.TabPages.Add(tab);}
     }
    return view;
   }catch{view.Dispose();throw;}
  }
  static string Detect(){
   string here=AppDomain.CurrentDomain.BaseDirectory.TrimEnd('\\');string ending=@"\Maine\Binaries\Win64\GroundedDLSS-install";
   if(here.EndsWith(ending,StringComparison.OrdinalIgnoreCase))return here.Substring(0,here.Length-ending.Length);
   foreach(string drive in Environment.GetLogicalDrives()){string candidate=System.IO.Path.Combine(drive,@"SteamLibrary\steamapps\common\Grounded");if(File.Exists(System.IO.Path.Combine(candidate,"Grounded.exe")))return candidate;}
   string steam=System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),@"Steam\steamapps\common\Grounded");return Directory.Exists(steam)?steam:"";
  }
  async Task Run(bool installing,bool recovering){
   busy=true;install.Enabled=remove.Enabled=recover.Enabled=folder.Enabled=languageSelector.Enabled=false;string target=folder.Text;
   try {await Task.Run(delegate {
    var e=new Engine(target,EnginePath);e.Log=t=>Invoke(new Action(()=>log.AppendText(t+Environment.NewLine)));
    if(recovering){e.Recover();return;}if(!installing){e.Uninstall();return;}
    var payload=new Dictionary<string,byte[]>(StringComparer.OrdinalIgnoreCase);
    using(var resource=Assembly.GetExecutingAssembly().GetManifestResourceStream("payload.zip")){
     if(resource==null)throw new IOException(I18n.T("Falta el paquete integrado."));
     using(var zip=new ZipArchive(resource,ZipArchiveMode.Read))foreach(var entry in zip.Entries){if(entry.Name.Length==0)continue;using(var m=new MemoryStream()){using(var source=entry.Open())source.CopyTo(m);payload.Add(entry.FullName.Replace('/','\\'),m.ToArray());}}
    }
    e.CheckCanInstall();
    var runtimeSource=NvidiaLocalRuntime.Find();
    e.Log(I18n.T("DLSS local NVIDIA verificado: ")+runtimeSource.Version+I18n.T(". Se detectará nuevamente al iniciar el juego."));
    e.Install(payload,File.ReadAllBytes(Assembly.GetExecutingAssembly().Location));
   });}catch(Exception ex){log.AppendText(I18n.T("No se completó: ")+ex.Message+Environment.NewLine);MessageBox.Show(this,ex.Message,"Grounded DLSS",MessageBoxButtons.OK,MessageBoxIcon.Warning);}
   finally{busy=false;install.Enabled=remove.Enabled=recover.Enabled=folder.Enabled=languageSelector.Enabled=true;}
  }

 }
 static class Program { [STAThread] static void Main(){Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);Application.Run(new SetupForm());} }
}
