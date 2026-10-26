# Grounded DLSS 11.24.1 Beta · Installer B

[![English](https://raw.githubusercontent.com/aquiyahora1598/Grounded-DLSS/main/assets/languages/en.svg)](https://github.com/aquiyahora1598/Grounded-DLSS/releases/tag/v11.24.1-beta) [![Español](https://raw.githubusercontent.com/aquiyahora1598/Grounded-DLSS/main/assets/languages/es.svg)](https://github.com/aquiyahora1598/Grounded-DLSS/blob/main/publication/RELEASE_NOTES-11.24.1.es.md) [![Português (Brasil)](https://raw.githubusercontent.com/aquiyahora1598/Grounded-DLSS/main/assets/languages/pt-BR-selected.svg)](https://github.com/aquiyahora1598/Grounded-DLSS/blob/main/publication/RELEASE_NOTES-11.24.1.pt-BR.md)

Um instalador menor com conteúdo separado e verificado. A renderização de 11.24.0 foi preservada.

**[Baixar 11.24.1 Beta](https://github.com/aquiyahora1598/Grounded-DLSS/releases/download/v11.24.1-beta/GroundedDLSS-11.24.1-beta.zip)**

## Controles

| | |
| --- | --- |
| **F8 · DLSS / DLAA** | Ativar ou desativar DLSS / DLAA |
| **F9 · FSR Frame Generation** | Ativar ou desativar FSR Frame Generation |

Ambos iniciam ativados. Use F9 para comparar somente Frame Generation. Desativar DLSS com F8 suspende Frame Generation; ele retorna ao reativar DLSS se F9 continuava ativado.

**Mantenha DESATIVADO o escalonamento AMD FSR do menu.** O instalador o desativa automaticamente. Frame Generation é controlado separadamente com F9.

> [!WARNING]
> **Ainda há detecções de antivírus.** Em 24 de outubro de 2026, o EXE do instalador tinha **5/70 detecções** e o ZIP completo **2/65**, com cobertura diferente. São resultados daquela data, não uma certificação. Se for bloqueado, mantenha o arquivo bloqueado; não desative a proteção nem adicione exclusões.
>
> [Detalhes de segurança e relatórios do VirusTotal](https://github.com/aquiyahora1598/Grounded-DLSS/blob/main/SECURITY.pt-BR.md).

## Instalar

1. Feche o Grounded normalmente.
2. Baixe **GroundedDLSS-11.24.1-beta.zip** e extraia **todos** os arquivos em uma pasta.
3. Execute `GroundedDLSS-11.24.0-beta-B-verified-sidecar-Setup.exe`. Mantenha **GroundedDLSS-payload.zip ao lado do EXE**, sem extrair o ZIP interno.
4. Escolha o idioma e a pasta Grounded que contém `Grounded.exe`. Leia os termos e as licenças; se concordar, escolha **Instalar / atualizar**.
5. Inicie o Grounded com DirectX 12. Comece com escala **67%** para Qualidade.

## Desinstalar

Feche o Grounded e escolha **Desinstalar** no mesmo instalador, ou execute `Desinstalar Grounded DLSS.exe` em `Maine/Binaries/Win64/GroundedDLSS-install`. O ZIP externo não é necessário para desinstalar, recuperar ou consultar a ajuda. Guarde o pacote completo para futuras instalações e atualizações.

## O que mudou

- Instalador pequeno com ZIP externo, verificado por tamanho e SHA-256 antes do uso.
- Ajuda e licenças integradas; desinstalação e recuperação sem o ZIP original.
- O pacote B exato testado no jogo e no VirusTotal foi preservado. Apenas o nome do download externo mudou.

**11.24.1 atualiza o instalador e a distribuição; a renderização continua em 11.24.0.** O download preserva os arquivos exatos de B que foram testados. O EXE interno mantém seu nome e sua versão para identificar essa amostra.

Passaram oito suítes de regressão de B, a integração isolada com controles de produção e os testes breves de F8/F9 no jogo. Não são testes exaustivos de compatibilidade.

[Guia completo de instalação e atualização](https://github.com/aquiyahora1598/Grounded-DLSS/blob/main/INSTALLER_B.pt-BR.md) · [Detalhes de segurança e relatórios do VirusTotal](https://github.com/aquiyahora1598/Grounded-DLSS/blob/main/SECURITY.pt-BR.md).

## Arquivos

[Código-fonte](https://github.com/aquiyahora1598/Grounded-DLSS/releases/download/v11.24.1-beta/GroundedDLSS-11.24.1-beta-Source.zip) · [Somas de verificação](https://github.com/aquiyahora1598/Grounded-DLSS/releases/download/v11.24.1-beta/SHA256SUMS.txt).

O código próprio usa MIT. Os componentes NVIDIA e AMD mantêm seus termos separados. Esta beta não oficial é publicada a pedido do autor; as pendências em LEGAL.md não são declaradas resolvidas. Revise os logs antes de compartilhá-los, pois podem conter caminhos pessoais.
