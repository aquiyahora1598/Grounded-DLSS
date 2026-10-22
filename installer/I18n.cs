using System;
using System.Collections.Generic;
using System.Globalization;

namespace GroundedDLSSSetup {
 public static class I18n {
  static string language=FromCulture(CultureInfo.CurrentUICulture.Name);
  public static string Language {get{return language;}}
  public static string FromCulture(string name){return name!=null&&name.StartsWith("es",StringComparison.OrdinalIgnoreCase)?"es":name!=null&&name.StartsWith("pt",StringComparison.OrdinalIgnoreCase)?"pt-BR":"en";}
  public static void SetLanguage(string value){if(value!="es"&&value!="en"&&value!="pt-BR")throw new ArgumentException("Unsupported installer language");language=value;}
  public static string Document(string stem){return stem+(language=="es"?"":"."+language)+".txt";}
  public static string T(string original){string[] pair;return language=="es"?original:translations.TryGetValue(original,out pair)?pair[language=="pt-BR"?1:0]:original;}
  public static string Original(string text){if(translations.ContainsKey(text))return text;foreach(var item in translations)if(item.Value[0]==text||item.Value[1]==text)return item.Key;return text;}
  public static bool Contains(string text){return translations.ContainsKey(Original(text));}
  public static IEnumerable<string> Keys {get{return translations.Keys;}}
  static readonly Dictionary<string,string[]> translations=new Dictionary<string,string[]>(StringComparer.Ordinal){
   {"F8  ·  ACTIVAR / DESACTIVAR DLSS / DLAA",new[]{"F8  ·  ENABLE / DISABLE DLSS / DLAA","F8  ·  ATIVAR / DESATIVAR DLSS / DLAA"}},
   {"F9  ·  ACTIVAR / DESACTIVAR FSR FRAME GENERATION",new[]{"F9  ·  ENABLE / DISABLE FSR FRAME GENERATION","F9  ·  ATIVAR / DESATIVAR FSR FRAME GENERATION"}},
   {"AMD FidelityFX SDK (inglés original)",new[]{"AMD FidelityFX SDK (original English)","AMD FidelityFX SDK (inglês original)"}},
   {"Idioma",new[]{"Language","Idioma"}},
   {"Grounded DLSS — beta, revisión legal pendiente",new[]{"Grounded DLSS — beta, legal review pending","Grounded DLSS — beta, revisão jurídica pendente"}},
   {"Beta: revisión de licencias y marcas pendiente.\r\nMod no oficial, sin garantía; no afiliado a NVIDIA, Obsidian ni Xbox.\r\nCerrá el juego. Conservá una copia de tus partidas. El escalado FSR del menú se desactiva automáticamente al instalar; se restaura al desinstalar si no cambiaste esa opción.\r\nLa nitidez está desactivada. Ultra Rendimiento puede perder mucho detalle. La profundidad de campo sigue en evaluación.\r\nInstalador sin firma digital. No cambia drivers, seguridad ni partidas. Usa los componentes locales de NVIDIA; no descarga DLSS ni instala bibliotecas de respaldo.",new[]{
    "Beta: license and trademark review pending.\r\nUnofficial mod, without warranty; not affiliated with NVIDIA, Obsidian or Xbox.\r\nClose the game and back up your saves. Installation turns the in-game FSR upscaler off automatically; uninstall restores it if you have not changed that option.\r\nSharpening is off. Ultra Performance may lose substantial detail. Depth of field is still being evaluated.\r\nUnsigned installer. Does not change drivers, security or saves. Uses local NVIDIA components; does not download DLSS or install backup libraries.",
    "Beta: revisão de licenças e marcas pendente.\r\nMod não oficial, sem garantia; sem vínculo com NVIDIA, Obsidian ou Xbox.\r\nFeche o jogo e faça uma cópia dos saves. A instalação desativa automaticamente o escalonamento FSR do menu; a desinstalação o restaura se você não tiver alterado essa opção.\r\nA nitidez está desativada. Ultra Desempenho pode perder muitos detalhes. A profundidade de campo ainda está em avaliação.\r\nInstalador sem assinatura digital. Não altera drivers, segurança ou saves. Usa componentes locais da NVIDIA; não baixa o DLSS nem instala bibliotecas de reserva."}},
   {"Carpeta del juego (la que contiene Grounded.exe):",new[]{"Game folder (the one containing Grounded.exe):","Pasta do jogo (a que contém Grounded.exe):"}},
   {"Buscar…",new[]{"Browse…","Procurar…"}},
   {"Elegí la carpeta Grounded",new[]{"Select the Grounded folder","Selecione a pasta do Grounded"}},
   {"DLSS se detecta automáticamente entre los componentes de NVIDIA. Si falta una biblioteca compatible, la instalación se detiene sin modificar el juego.",new[]{"DLSS is detected automatically in local NVIDIA components. If no compatible library is found, installation stops without changing the game.","O DLSS é detectado automaticamente nos componentes locais da NVIDIA. Se não houver uma biblioteca compatível, a instalação para sem alterar o jogo."}},
   {"Ver ayuda, términos y licencias",new[]{"View help, terms and licenses","Ver ajuda, termos e licenças"}},
   {"Leí y acepto los términos del mod y las licencias NVIDIA y AMD aplicables.",new[]{"I have read and accept the mod's terms and the applicable NVIDIA and AMD licenses.","Li e aceito os termos do mod e as licenças NVIDIA e AMD aplicáveis."}},
   {"Instalar / actualizar",new[]{"Install / update","Instalar / atualizar"}},
   {"Desinstalar",new[]{"Uninstall","Desinstalar"}},
   {"Recuperar operación",new[]{"Recover operation","Recuperar operação"}},
   {"Leé los términos y las licencias NVIDIA y AMD antes de aceptar e instalar.",new[]{"Read the terms and the NVIDIA and AMD licenses before accepting and installing.","Leia os termos e as licenças NVIDIA e AMD antes de aceitar e instalar."}},
   {"Esperá a que termine la operación antes de cerrar.",new[]{"Wait for the operation to finish before closing.","Aguarde a conclusão da operação antes de fechar."}},
   {"Ayuda, términos, licencias y atribuciones",new[]{"Help, terms, licenses and attribution","Ajuda, termos, licenças e atribuições"}},
   {"Ayuda",new[]{"Help","Ajuda"}},
   {"Términos del mod",new[]{"Mod terms","Termos do mod"}},
   {"Licencia MIT (original)",new[]{"MIT license (original)","Licença MIT (original)"}},
   {"NVIDIA RTX SDK (inglés original)",new[]{"NVIDIA RTX SDK (original English)","NVIDIA RTX SDK (inglês original)"}},
   {"Avisos de terceros (original)",new[]{"Third-party notices (original)","Avisos de terceiros (original)"}},
   {"Registro de recuperación inválido.",new[]{"Invalid recovery journal.","Registro de recuperação inválido."}},
   {"Registro de archivo temporal inválido.",new[]{"Invalid temporary-file journal.","Registro de arquivo temporário inválido."}},
   {"Respaldo inválido.",new[]{"Invalid backup.","Backup inválido."}},
   {"Usá una carpeta local sin rutas alternativas.",new[]{"Use a local folder without alternate paths.","Use uma pasta local sem caminhos alternativos."}},
   {"No se admiten enlaces o carpetas redirigidas: ",new[]{"Links or redirected folders are not supported: ","Links ou pastas redirecionadas não são aceitos: "}},
   {"Ruta de paquete inválida.",new[]{"Invalid package path.","Caminho do pacote inválido."}},
   {"Ruta fuera del destino.",new[]{"Path is outside the destination.","O caminho está fora do destino."}},
   {"Otra operación de Grounded DLSS sigue abierta.",new[]{"Another Grounded DLSS operation is still running.","Outra operação do Grounded DLSS ainda está em execução."}},
   {"Cerrá Grounded normalmente antes de continuar.",new[]{"Close Grounded normally before continuing.","Feche o Grounded normalmente antes de continuar."}},
   {"El selector de pruebas dejó el mod desactivado. Con el juego cerrado, ejecutá primero «1 - DLSS.bat» antes de actualizar o desinstalar.",new[]{"The test-mode selector disabled the mod. With the game closed, run '1 - DLSS.bat' before updating or uninstalling.","O seletor de testes desativou o mod. Com o jogo fechado, execute '1 - DLSS.bat' antes de atualizar ou desinstalar."}},
   {"Se requiere Grounded para Steam 1.4.7.4815 (ejecutable verificado). Esta versión del juego no es compatible.",new[]{"Grounded for Steam 1.4.7.4815 is required (verified executable). This game version is unsupported.","É necessário o Grounded da Steam 1.4.7.4815 (executável verificado). Esta versão do jogo não é compatível."}},
   {"Se encontró otro posible cargador: ",new[]{"Another possible loader was found: ","Outro possível carregador foi encontrado: "}},
   {". Revisá su compatibilidad antes de instalar.",new[]{". Check compatibility before installing.",". Verifique a compatibilidade antes de instalar."}},
   {"Hay una operación interrumpida. Usá Recuperar antes de continuar.",new[]{"An interrupted operation was found. Use Recover before continuing.","Foi encontrada uma operação interrompida. Use Recuperar antes de continuar."}},
   {"El registro de instalación no corresponde a esta carpeta o usuario.",new[]{"The installation record does not match this folder or user.","O registro da instalação não corresponde a esta pasta ou usuário."}},
   {"Registro de archivos inválido.",new[]{"Invalid file record.","Registro de arquivos inválido."}},
   {"Registro de ajustes inválido.",new[]{"Invalid settings record.","Registro de configurações inválido."}},
   {"El registro intenta acceder a un archivo ajeno. Se conserva para revisión.",new[]{"The journal attempts to access an unrelated file. It has been preserved for review.","O registro tenta acessar um arquivo de terceiros. Ele foi preservado para análise."}},
   {"Cerrá Grounded antes de recuperar.",new[]{"Close Grounded before recovery.","Feche o Grounded antes de recuperar."}},
   {"Operación interrumpida recuperada.",new[]{"Interrupted operation recovered.","Operação interrompida recuperada."}},
   {"El instalador no instala bibliotecas NVIDIA ni copias de respaldo.",new[]{"The installer does not install NVIDIA libraries or backup copies.","O instalador não instala bibliotecas da NVIDIA nem cópias de reserva."}},
   {"Archivo inesperado en el paquete: ",new[]{"Unexpected file in the package: ","Arquivo inesperado no pacote: "}},
   {"Ya existe un archivo ajeno al instalador: ",new[]{"A file not owned by this installer already exists: ","Já existe um arquivo que não pertence a este instalador: "}},
   {". Retirá la instalación anterior o el otro mod primero.",new[]{". Remove the previous installation or the other mod first.",". Remova primeiro a instalação anterior ou o outro mod."}},
   {"Un archivo instalado fue modificado. Se conserva: ",new[]{"An installed file was modified and has been preserved: ","Um arquivo instalado foi modificado e foi preservado: "}},
   {"Existe otro desinstalador o fue modificado. Se conserva.",new[]{"Another uninstaller exists or it was modified. It has been preserved.","Já existe outro desinstalador ou ele foi modificado. Ele foi preservado."}},
   {"Se conserva la biblioteca anterior porque fue modificada: ",new[]{"The previous library was modified and has been preserved: ","A biblioteca anterior foi modificada e foi preservada: "}},
   {"Se conserva tu configuración DLSS.",new[]{"Your DLSS configuration has been preserved.","Sua configuração do DLSS foi preservada."}},
   {"Instalación completada. F8: activar/desactivar DLSS/DLAA. F9: activar/desactivar FSR Frame Generation. El escalado FSR del menú quedó apagado. Se conservó tu selección anterior para desinstalar. Iniciá con DirectX 12.",new[]{"Installation complete. F8: toggle DLSS/DLAA. F9: toggle FSR Frame Generation. The in-game FSR upscaler is off. Your previous choice was saved for uninstall. Start with DirectX 12.","Instalação concluída. F8: ativar/desativar DLSS/DLAA. F9: ativar/desativar FSR Frame Generation. O escalonamento FSR do menu está desativado. Sua escolha anterior foi salva para a desinstalação. Inicie com DirectX 12."}},
   {"No hay una instalación registrada en esta carpeta.",new[]{"No installation is registered in this folder.","Não há uma instalação registrada nesta pasta."}},
   {"El respaldo de configuración fue modificado o está dañado. Se conserva la instalación para revisarlo.",new[]{"The configuration backup was modified or is damaged. The installation has been preserved for review.","O backup das configurações foi modificado ou está danificado. A instalação foi preservada para análise."}},
   {"El respaldo de opciones del juego falta o está dañado. Se conserva la instalación para revisarlo.",new[]{"The game-options backup is missing or damaged. The installation has been preserved for review.","O backup das opções do jogo está ausente ou danificado. A instalação foi preservada para análise."}},
   {"Se conserva la biblioteca automática porque cambió o perdió su firma NVIDIA.",new[]{"The automatic library was changed or lost its NVIDIA signature and has been preserved.","A biblioteca automática foi alterada ou perdeu a assinatura da NVIDIA e foi preservada."}},
   {"Conservado porque cambió: ",new[]{"Preserved because it was changed: ","Preservado porque foi alterado: "}},
   {"Se conserva el ajuste que cambiaste: ",new[]{"Your changed setting has been preserved: ","A configuração que você alterou foi preservada: "}},
   {"Se conserva la selección FSR que cambiaste después de instalar.",new[]{"Your FSR choice made after installation has been preserved.","Sua escolha de FSR feita após a instalação foi preservada."}},
   {"Mod desinstalado. Los archivos modificados, respaldos y este desinstalador se conservan en GroundedDLSS-install.",new[]{"Mod uninstalled. Modified files, backups and this uninstaller are preserved in GroundedDLSS-install.","Mod desinstalado. Arquivos modificados, backups e este desinstalador foram preservados em GroundedDLSS-install."}},
   {"No se encontró DLSS de producción compatible en los componentes locales de NVIDIA. No se hizo ningún cambio. Este instalador no descarga bibliotecas; revisá el controlador y los componentes de NVIDIA.",new[]{"No compatible production DLSS library was found in local NVIDIA components. No changes were made. This installer does not download libraries; check the NVIDIA driver and components.","Nenhuma biblioteca DLSS de produção compatível foi encontrada nos componentes locais da NVIDIA. Nenhuma alteração foi feita. Este instalador não baixa bibliotecas; verifique o driver e os componentes da NVIDIA."}},
   {"Falta el paquete integrado.",new[]{"The embedded package is missing.","O pacote integrado está ausente."}},
   {"DLSS local NVIDIA verificado: ",new[]{"Verified local NVIDIA DLSS: ","DLSS local da NVIDIA verificado: "}},
   {". Se detectará nuevamente al iniciar el juego.",new[]{". It will be detected again when the game starts.",". Ele será detectado novamente ao iniciar o jogo."}},
   {"No se completó: ",new[]{"Not completed: ","Não concluído: "}},
   {"El archivo no es una biblioteca DLSS de producción válida.",new[]{"The file is not a valid production DLSS library.","O arquivo não é uma biblioteca DLSS de produção válida."}},
   {"DLSS no es una biblioteca Windows válida.",new[]{"DLSS is not a valid Windows library.","O DLSS não é uma biblioteca válida do Windows."}},
   {"Cabecera DLSS inválida.",new[]{"Invalid DLSS header.","Cabeçalho do DLSS inválido."}},
   {"Se requiere DLSS x64.",new[]{"DLSS x64 is required.","É necessário o DLSS x64."}},
   {"Se requiere una DLL DLSS x64.",new[]{"An x64 DLSS DLL is required.","É necessária uma DLL DLSS x64."}},
   {"Se requiere NVIDIA DLSS Super Resolution de producción; no se admiten otras bibliotecas ni versiones de desarrollo.",new[]{"Production NVIDIA DLSS Super Resolution is required; other libraries and development builds are not supported.","É necessário o NVIDIA DLSS Super Resolution de produção; outras bibliotecas e versões de desenvolvimento não são aceitas."}},
   {"La firma de DLSS no es válida o no pertenece a NVIDIA. Se conserva el archivo.",new[]{"The DLSS signature is invalid or does not belong to NVIDIA. The file has been preserved.","A assinatura do DLSS é inválida ou não pertence à NVIDIA. O arquivo foi preservado."}}
  };
 }
}
