using System;
using System.IO;
using System.Reflection;
using System.Security.Cryptography;

namespace GroundedDLSSSetup {
 // A sidecar is never executed. Its entire SHA-256 is bound into this executable
 // and verified on the same locked stream that ZipArchive subsequently reads.
 public static class PayloadPackage {
  public const string FileName="GroundedDLSS-payload.zip";
  public static Stream OpenDocuments() {
   var stream=Assembly.GetExecutingAssembly().GetManifestResourceStream("documentation.zip");
   if(stream==null)throw new IOException("Missing integrated documentation.");
   return stream;
  }
  public static Stream OpenInstall() {
   string path=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,FileName);
   Safe.Check(path);
   FileStream stream=null;
   try {
    stream=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.Read);
    if(stream.Length!=PackageIdentity.Bytes)throw new IOException(I18n.T("El paquete de instalación está incompleto o fue modificado."));
    string hash;
    using(var sha=SHA256.Create())hash=BitConverter.ToString(sha.ComputeHash(stream)).Replace("-","").ToLowerInvariant();
    if(!String.Equals(hash,PackageIdentity.Sha256,StringComparison.Ordinal))throw new IOException(I18n.T("El paquete de instalación está incompleto o fue modificado."));
    stream.Position=0;
    return stream;
   } catch(FileNotFoundException) {
    if(stream!=null)stream.Dispose();
    throw new IOException(I18n.T("Para instalar o actualizar, extraé el ZIP completo y ejecutá su instalador. Para desinstalar o recuperar no hace falta el paquete."));
   } catch {
    if(stream!=null)stream.Dispose();
    throw;
   }
  }
 }
}
