# Guia completo de instalação e atualização

[![English](assets/languages/en.svg)](INSTALLER_B.md) [![Español](assets/languages/es.svg)](INSTALLER_B.es.md) [![Português (Brasil)](assets/languages/pt-BR-selected.svg)](INSTALLER_B.pt-BR.md)

**11.24.1 Beta · Installer B**

> [!WARNING]
> **Ainda há detecções de antivírus.** Em 24 de outubro de 2026, o EXE do instalador tinha **5/70 detecções** e o ZIP completo **2/65**, com cobertura diferente. São resultados daquela data, não uma certificação. Se for bloqueado, mantenha o arquivo bloqueado; não desative a proteção nem adicione exclusões.
>
> [Detalhes de segurança e relatórios do VirusTotal](SECURITY.pt-BR.md).

## Instalar

1. Feche o Grounded normalmente.
2. Baixe **GroundedDLSS-11.24.1-beta.zip** e extraia **todos** os arquivos em uma pasta.
3. Execute `GroundedDLSS-11.24.0-beta-B-verified-sidecar-Setup.exe`. Mantenha **GroundedDLSS-payload.zip ao lado do EXE**, sem extrair o ZIP interno.
4. Escolha o idioma e a pasta Grounded que contém `Grounded.exe`. Leia os termos e as licenças; se concordar, escolha **Instalar / atualizar**.
5. Inicie o Grounded com DirectX 12. Comece com escala **67%** para Qualidade.

## Controles

| | |
| --- | --- |
| **F8 · DLSS / DLAA** | Ativar ou desativar DLSS / DLAA |
| **F9 · FSR Frame Generation** | Ativar ou desativar FSR Frame Generation |

Ambos iniciam ativados. Use F9 para comparar somente Frame Generation. Desativar DLSS com F8 suspende Frame Generation; ele retorna ao reativar DLSS se F9 continuava ativado.

**Mantenha DESATIVADO o escalonamento AMD FSR do menu.** O instalador o desativa automaticamente. Frame Generation é controlado separadamente com F9.

## Atualizar ou reinstalar

Com o Grounded fechado, extraia o pacote completo e use **Instalar / atualizar**. Mantenha o EXE e o ZIP que o acompanha. Não use os antigos seletores BAT de pesquisa em uma instalação nova. A desinstalação preserva configurações não relacionadas e arquivos alterados pelo usuário.

## Desinstalar

Feche o Grounded e escolha **Desinstalar** no mesmo instalador, ou execute `Desinstalar Grounded DLSS.exe` em `Maine/Binaries/Win64/GroundedDLSS-install`. O ZIP externo não é necessário para desinstalar, recuperar ou consultar a ajuda. Guarde o pacote completo para futuras instalações e atualizações.

## Identidade do pacote

**11.24.1 atualiza o instalador e a distribuição; a renderização continua em 11.24.0.** O download preserva os arquivos exatos de B que foram testados. O EXE interno mantém seu nome e sua versão para identificar essa amostra.

Um ZIP externo ausente, truncado ou alterado é recusado. Seu tamanho e SHA-256 são verificados com os valores incorporados no instalador, usando o mesmo arquivo bloqueado contra gravação durante a leitura. A ajuda e as licenças estão integradas para consulta sem o ZIP.

[Compilar o código-fonte](README.pt-BR.md) · [Detalhes de segurança e relatórios do VirusTotal](SECURITY.pt-BR.md).
