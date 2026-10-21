using System;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Collections.Generic;
using System.Xml.Serialization;
using GroundedDLSSSetup;
class InstallerCacheTests {
 static int failed;static string home,game,cache,ini,first,second;static byte[] cached,older;
 static void Check(bool value,string why){if(!value)throw new Exception(why);}
 static void Refused(Action action){bool ok=false;try{action();}catch(IOException){ok=true;}Check(ok,"Expected refusal");}
 static void Case(string name,Action action){try{action();Console.WriteLine("PASS "+name);}catch(Exception ex){++failed;Console.WriteLine("FAIL "+name+": "+ex.Message);}}
 static Dictionary<string,byte[]> Payload(){var p=new Dictionary<string,byte[]>(StringComparer.OrdinalIgnoreCase);using(var s=typeof(Engine).Assembly.GetManifestResourceStream("payload.zip"))using(var z=new ZipArchive(s,ZipArchiveMode.Read))foreach(var x in z.Entries)using(var m=new MemoryStream()){using(var f=x.Open())f.CopyTo(m);p.Add(x.FullName.Replace('/','\\'),m.ToArray());}return p;}
 static void Legacy(string root,byte[] data,bool alter){
  string state=Path.Combine(root,@"Maine\Binaries\Win64\GroundedDLSS-install\state.xml");var serializer=new XmlSerializer(typeof(State));State s;using(var f=File.OpenRead(state))s=(State)serializer.Deserialize(f);
  s.Items.Add(new Item{Path=Engine.Allowed[3],Hash=Safe.Hash(data),Active=true});using(var f=File.Create(state))serializer.Serialize(f,s);
  string dll=Path.Combine(root,Engine.Allowed[3]);Directory.CreateDirectory(Path.GetDirectoryName(dll));File.WriteAllBytes(dll,data);if(alter)File.AppendAllText(dll,"user edit");
 }
 static int Main(string[] args){
  Console.OutputEncoding=new UTF8Encoding(false);home=Path.GetFullPath(args[0]);Check(!Directory.Exists(home),"Fresh fixture required");game=Path.Combine(home,"game");cache=Path.Combine(home,"cache");ini=Path.Combine(home,"config","Engine.ini");
  Directory.CreateDirectory(Path.Combine(game,@"Maine\Binaries\Win64"));Directory.CreateDirectory(Path.GetDirectoryName(ini));File.WriteAllText(Path.Combine(game,@"Maine\Binaries\Win64\Maine-Win64-Shipping.exe"),"fixture");File.WriteAllText(ini,"[SystemSettings]\r\nr.TemporalAA.Upsampling=0\r\n;preserve\r\n".Replace("\\r\\n","\r\n"));byte[] before=File.ReadAllBytes(ini);
  cached=File.ReadAllBytes(args[1]);older=File.ReadAllBytes(args[2]);
  first=Path.Combine(cache,@"future-layout\files\future-model.bin");second=Path.Combine(cache,@"old-layout\files\old-model.bin");Directory.CreateDirectory(Path.GetDirectoryName(first));Directory.CreateDirectory(Path.GetDirectoryName(second));
  Case("missing-source-has-no-download-or-private-fallback",()=>{Refused(()=>NvidiaLocalRuntime.Find(cache,null,61742));Check(File.ReadAllBytes(ini).SequenceEqual(before)&&!File.Exists(Path.Combine(game,Engine.Allowed[0])),"Missing source changed installation");});
  File.WriteAllBytes(first,cached);File.WriteAllBytes(second,older);
  Case("latest-external-production-source-selected",()=>{var found=NvidiaLocalRuntime.Find(cache,null,61742);Check(found.Path==first,"Newest numeric source not selected");});
  Case("unsupported-driver-refused",()=>Refused(()=>NvidiaLocalRuntime.Find(cache,null,51214)));
  Case("corrupt-cache-does-not-replace-valid-source",()=>{byte[] bad=(byte[])cached.Clone();bad[1024*1024]^=1;File.WriteAllBytes(first,bad);Check(NvidiaLocalRuntime.Find(cache,null,61742).Path==second,"Corrupt cache accepted");Check(File.ReadAllBytes(first).SequenceEqual(bad),"NVIDIA cache changed");File.WriteAllBytes(first,cached);});
  Case("private-copies-do-not-satisfy-missing-external-source",()=>{string old=Path.Combine(game,Engine.Allowed[3]);Directory.CreateDirectory(Path.GetDirectoryName(old));File.WriteAllBytes(old,cached);Refused(()=>NvidiaLocalRuntime.Find(Path.Combine(home,"no-external-source"),null,61742));File.Delete(old);});
  Case("install-update-uninstall-does-not-copy-nvidia-library",()=>{var e=new Engine(game,ini,true);e.Install(Payload(),null);e.Install(Payload(),null);Check(!File.Exists(Path.Combine(game,Engine.Allowed[3])),"Private backup installed");e.Uninstall();Check(File.ReadAllBytes(ini).SequenceEqual(before)&&File.ReadAllBytes(first).SequenceEqual(cached),"Config or NVIDIA source changed");});
  Case("upgrade-removes-only-owned-legacy-backup",()=>{var e=new Engine(game,ini,true);e.Install(Payload(),null);Legacy(game,older,false);e.Install(Payload(),null);Check(!File.Exists(Path.Combine(game,Engine.Allowed[3])),"Legacy backup retained");e.Uninstall();e.Install(Payload(),null);Legacy(game,older,true);e.Install(Payload(),null);Check(File.Exists(Path.Combine(game,Engine.Allowed[3])),"Modified legacy file removed");e.Uninstall();File.Delete(Path.Combine(game,Engine.Allowed[3]));});
  Case("working-copy-cleanup-preserves-foreign-files",()=>{var e=new Engine(game,ini,true);e.Install(Payload(),null);string dll=Path.Combine(game,NvidiaLocalRuntime.AutoDll),owner=Path.Combine(game,NvidiaLocalRuntime.AutoOwner);Directory.CreateDirectory(Path.GetDirectoryName(dll));File.WriteAllBytes(dll,cached);File.WriteAllText(owner,NvidiaLocalRuntime.Owner);string user=Path.Combine(Path.GetDirectoryName(dll),"user.txt");File.WriteAllText(user,"keep");e.Uninstall();Check(!File.Exists(dll)&&!File.Exists(owner)&&File.ReadAllText(user)=="keep","Working cleanup changed foreign files");e.Install(Payload(),null);File.WriteAllText(dll,"modified");File.WriteAllText(owner,NvidiaLocalRuntime.Owner);e.Uninstall();Check(File.ReadAllText(dll)=="modified"&&File.Exists(owner),"Modified working file removed");});
  return failed==0?0:1;
 }
}
