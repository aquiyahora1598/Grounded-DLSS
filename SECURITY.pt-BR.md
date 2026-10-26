# Segurança e estado do antivírus

[![English](assets/languages/en.svg)](SECURITY.md) [![Español](assets/languages/es.svg)](SECURITY.es.md) [![Português (Brasil)](assets/languages/pt-BR-selected.svg)](SECURITY.pt-BR.md)

**11.24.1 Beta · Installer B**

Estas observações foram registradas em **24 de outubro de 2026** e não foram atualizadas com esta alteração da documentação. Os resultados podem mudar. A release preserva os arquivos exatos de B que foram testados.

**Ainda há detecções de antivírus.** Em 24 de outubro de 2026, o EXE do instalador tinha **5/70 detecções** e o ZIP completo **2/65**, com cobertura diferente. São resultados daquela data, não uma certificação. Se for bloqueado, mantenha o arquivo bloqueado; não desative a proteção nem adicione exclusões.

| Amostra | SHA-256 | Resultado observado | Microsoft naquele relatório |
| --- | --- | --- | --- |
| ZIP completo para download | `86ccb82a12ecdf8f5ae5fdd57aebd40e0ac5a9d950f705673cb88d039edca6b4` | 2/65 | Undetected |
| EXE do instalador dentro do ZIP | `c7945b4333058cb4eab0817eb210296bbb294d1c2d37cec001a0e5034d66a4ca` | 5/70 | Undetected |
| ZIP interno do conteúdo | `d965884af71330028edfd17fdd4883e4fe678aa2621601aa81194ae674ba5e04` | Sem análise separada declarada | — |

[VirusTotal · ZIP](https://www.virustotal.com/gui/file/86ccb82a12ecdf8f5ae5fdd57aebd40e0ac5a9d950f705673cb88d039edca6b4/detection) · [VirusTotal · EXE](https://www.virustotal.com/gui/file/c7945b4333058cb4eab0817eb210296bbb294d1c2d37cec001a0e5034d66a4ca/detection).

**O resultado do ZIP não certifica seu conteúdo.** Os dois contadores têm cobertura diferente. Bkav e Elastic sinalizaram o ZIP completo; o EXE também tinha detecções de CrowdStrike, McAfee e SecureAge. A ausência de detecção da Microsoft no VirusTotal não garante que o Defender não bloqueie em outro PC.

O Defender local não encontrou ameaças com as definições 1.461.188.0. Esse resultado, as verificações de integridade e os testes no jogo não demonstram que as demais detecções são falsos positivos. O instalador, carregador e ASI não são assinados digitalmente; as duas DLLs AMD incluídas mantêm suas assinaturas originais.

O último estado registrado da solicitação original à Microsoft era **In progress** em 24 de outubro. Não se declara um parecer posterior nem uma aprovação geral dos fornecedores.

[Escopo da validação](docs/INSTALLER_B_VALIDATION.md) · [Relatório de segurança anterior](https://github.com/aquiyahora1598/Grounded-DLSS/blob/8fb24196042063e09521bb41744ba45938a1acf7/SECURITY.md).
