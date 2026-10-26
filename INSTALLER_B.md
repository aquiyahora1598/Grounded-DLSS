# Installer B — 11.24.1 Beta

## Español

1. Cerrá Grounded normalmente.
2. Descargá **GroundedDLSS-11.24.1-beta.zip** y extraé **todo** su contenido.
3. Abrí `GroundedDLSS-11.24.0-beta-B-verified-sidecar-Setup.exe`.
   Dejá `GroundedDLSS-payload.zip` al lado del EXE, sin extraer el ZIP interno.
4. Elegí tu idioma y la carpeta Grounded que contiene `Grounded.exe`.
5. Leé los términos y licencias. Si aceptás, usá **Instalar / actualizar**.
6. Iniciá Grounded con DirectX 12. Para empezar, usá escala 67 % (Calidad).

**F8: DLSS / DLAA. F9: FSR Frame Generation.** Ambos inician activados.
Apagar DLSS suspende FG; vuelve al reactivar DLSS si F9 seguía encendido.
El escalado FSR del menú debe permanecer apagado; el instalador lo apaga.
Escalas: 100 DLAA, 67 Calidad, 58 Equilibrado, 50 Rendimiento, 33 Ultra.

Para desinstalar, cerrá el juego y abrí el mismo EXE o
`Maine/Binaries/Win64/GroundedDLSS-install/Desinstalar Grounded DLSS.exe`.
La desinstalación, recuperación y ayuda funcionan sin el ZIP acompañante.
Para instalar o actualizar sí necesitás el paquete completo. Conservá una copia.
No ejecutes antiguos selectores BAT de pruebas sobre una instalación nueva.

## English

Close Grounded. Download **GroundedDLSS-11.24.1-beta.zip**, extract **all** files,
and run the included Setup. Keep **GroundedDLSS-payload.zip next to the EXE**;
do not extract the inner ZIP. Select your language and the Grounded folder,
read the terms and licenses, then choose **Install / update** if you agree.
Start Grounded with DirectX 12. Try 67% resolution scale for Quality.

**F8 toggles DLSS/DLAA; F9 toggles FSR Frame Generation.** Both start enabled.
Turning DLSS off suspends FG. Keep the in-game AMD FSR upscaler off.
To uninstall, close the game and use the same Setup or the installed uninstaller
in `Maine/Binaries/Win64/GroundedDLSS-install`. Removal, recovery and help work
without the sidecar ZIP; installing/updating requires the complete extracted package.

## Português (Brasil)

Feche o Grounded. Baixe **GroundedDLSS-11.24.1-beta.zip**, extraia **todos** os
arquivos e execute o instalador incluído. Mantenha **GroundedDLSS-payload.zip
ao lado do EXE**, sem extrair o ZIP interno. Escolha o idioma e a pasta Grounded,
leia os termos e as licenças e, se concordar, clique em **Instalar / atualizar**.
Inicie com DirectX 12. Comece com escala 67% para Qualidade.

**F8 alterna DLSS/DLAA; F9 alterna FSR Frame Generation.** Ambos iniciam ativados.
Desativar DLSS suspende FG. Mantenha o escalonamento AMD FSR do menu desativado.
Para desinstalar, feche o jogo e use o mesmo instalador ou o desinstalador em
`Maine/Binaries/Win64/GroundedDLSS-install`. Desinstalação, recuperação e ajuda
funcionam sem o ZIP adicional; instalar/atualizar exige o pacote completo extraído.

## Identity / Identidad / Identidade

11.24.1 is a packaging/installer update. Rendering remains 11.24.0.
The release ZIP is byte-identical to the B ZIP tested in VirusTotal; only its
download filename changed. The inner EXE retains its tested filename/version.
The installer verifies the complete sidecar's size and SHA-256 before reading it,
on the same stream locked against writes. It embeds documentation for offline help.
No NVIDIA runtime is bundled or downloaded. The signed AMD FG DLLs remain included.
The small installer is unsigned. Dated antivirus detections remain in [SECURITY.md](SECURITY.md).
The historical PRIVATE SECURITY EXPERIMENT label inside the unchanged ZIP identifies
the tested sample; this beta is now distributed at the author's request.

## Build

Build the runtime/dependencies as described in README, then run
`python tools/build_release.py`. It creates the external payload and a matching
installer. Alternatively, `tools/build_installer_b.py --payload <payload.zip>
--output <Setup.exe>` builds from an existing allowed 15-file payload.
`installer/PackageIdentity.cs` records the reviewed payload's original identity;
the builder generates a fresh identity for the payload it actually packages.
Rebuilt executables need their own hashes/reports; bit-for-bit reproducibility
is not claimed for this classic compiler. Publication clearance gates remain intact.
