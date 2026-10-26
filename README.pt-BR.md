# Grounded DLSS + FSR Frame Generation

[![English](assets/languages/en.svg)](README.md) [![Español](assets/languages/es.svg)](README.es.md) [![Português (Brasil)](assets/languages/pt-BR-selected.svg)](README.pt-BR.md)

**11.24.1 Beta**

Integração não oficial de DLSS/DLAA e AMD FSR Frame Generation para Grounded. Compatível com **Steam 1.4.7.4815**, Windows 10/11 x64, DirectX 12, NVIDIA RTX e saída SDR.

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
> [Detalhes de segurança e relatórios do VirusTotal](SECURITY.pt-BR.md).

## Instalar

1. Feche o Grounded normalmente.
2. Baixe **GroundedDLSS-11.24.1-beta.zip** e extraia **todos** os arquivos em uma pasta.
3. Execute `GroundedDLSS-11.24.0-beta-B-verified-sidecar-Setup.exe`. Mantenha **GroundedDLSS-payload.zip ao lado do EXE**, sem extrair o ZIP interno.
4. Escolha o idioma e a pasta Grounded que contém `Grounded.exe`. Leia os termos e as licenças; se concordar, escolha **Instalar / atualizar**.
5. Inicie o Grounded com DirectX 12. Comece com escala **67%** para Qualidade.

[Guia completo de instalação e atualização](INSTALLER_B.pt-BR.md).

## Desinstalar

Feche o Grounded e escolha **Desinstalar** no mesmo instalador, ou execute `Desinstalar Grounded DLSS.exe` em `Maine/Binaries/Win64/GroundedDLSS-install`. O ZIP externo não é necessário para desinstalar, recuperar ou consultar a ajuda. Guarde o pacote completo para futuras instalações e atualizações.

## Escala de resolução

| % | Modo DLSS |
| --- | --- |
| 100 | DLAA |
| 67 | Qualidade |
| 58 | Equilibrado |
| 50 | Desempenho |
| 33 | Ultra Desempenho |

O modo compatível mais próximo é selecionado. A nitidez está desativada. Ultra Desempenho pode perder muitos detalhes.

## DLSS e componentes incluídos

Inclui as DLLs AMD originais assinadas de Frame Generation e sua licença. **Não inclui nem baixa DLLs NVIDIA.** Ao iniciar, o mod seleciona o DLSS de produção assinado mais recente e compatível disponível nos componentes locais da NVIDIA. Essa fonte local é necessária; uma instalação limpa do driver pode não contê-la. Não altera os perfis nem o cache original da NVIDIA. Não inclui OptiScaler nem Frame Generation da NVIDIA.

**11.24.1 atualiza o instalador e a distribuição; a renderização continua em 11.24.0.** O download preserva os arquivos exatos de B que foram testados. O EXE interno mantém seu nome e sua versão para identificar essa amostra.

## Testes e limitações

Testes breves no jogo com uma RTX 4070 Laptop incluíram 1080p e 1440p. B passou pela ativação de DLSS/FG e pelas alternâncias F8/F9. Isso não garante compatibilidade ou desempenho universal. HDR não foi validado; podem ocorrer artefatos breves ao pausar. Frame Generation não duplica a resposta dos controles.

[Escopo da validação](docs/INSTALLER_B_VALIDATION.md).

## Compilar o código-fonte

Requer Python 3.10+, MSVC C++ x64, Windows SDK e .NET Framework 4.x. Leia a licença NVIDIA incluída antes de aceitar seus termos.

```text
python tools/fetch_sdk.py --accept-nvidia-sdk-license
python tools/fetch_framegen_sdks.py amd
python tools/build.py loader
python tools/build.py dlss-fsr-framegen
python tools/build_release.py
python tools/build_source_release.py
```

## Licenças e avisos

O código próprio usa MIT. Os componentes NVIDIA e AMD mantêm seus termos separados. Esta beta não oficial é publicada a pedido do autor; as pendências em LEGAL.md não são declaradas resolvidas. Revise os logs antes de compartilhá-los, pois podem conter caminhos pessoais.

[MIT](LICENSE) · [AMD](licenses/AMD-FidelityFX-SDK-LICENSE.txt) · [NVIDIA](licenses/NVIDIA-RTX-SDK-LICENSE.txt) · [LEGAL.md](LEGAL.md).

<details>
<summary>Avisos legais originais em inglês</summary>

This software contains source code provided by NVIDIA Corporation.

Grounded DLSS isn't endorsed by Obsidian Entertainment, Inc. and doesn't reflect
the views or opinions of Obsidian Entertainment, Inc. or anyone officially involved
with making Grounded. Grounded and Obsidian Entertainment and all related logos
are trademarks or registered trademarks of Obsidian Entertainment, Inc.

</details>
