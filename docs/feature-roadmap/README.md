# Roadmap e estudos do Encore

Documentação permanente migrada para este repositório em **2 de novembro de 2026**.
Os estudos e resultados técnicos abaixo mantêm suas datas originais de outubro;
a migração não representa um novo teste, release ou aprovação.

## Comece aqui

- [Campanha de 50 agentes — 2026-11-07](campaign-20261107/README.md).
- [Integrações aceitas e gates pendentes da campanha](campaign-20261107/INTEGRATION.md).
- [Progresso dos doze grupos e limites atuais](implementation-20261031/progress.md).
- [Decisões e requisitos do proprietário](decisions.md).
- [Projeto geral — rascunho, não aceitação do roadmap](design_draft.md).
- [Contrato do incremento implementado e sua validação](implementation-20261031/increment.md).

**Switch 2 continua em pesquisa, não implementado.** A compilação de 21 unidades
do launcher e auxiliares não comprova um backend Switch 2 nem um build completo
do core. Amiibo permanece restrito a arquivos `.bin`, sem leitores físicos.
O ciclo EGM terminou no limite de cinco rodadas, sem convergência. Posteriormente,
[A17](campaign-20261107/agents/A17.md) fechou DE-05-01 de modo independente e
limitado a fonte+host; isso não reinicia o ciclo nem qualifica execução nativa.

## Estudos técnicos

| Tema | Documento |
| --- | --- |
| Switch 2: quatro upstreams, memória, CPU, GPU, proveniência e viabilidade PS5 | [Comparação NX2](implementation-20261031/research/nx2-comparison.md) |
| Gate aritmético executável e fixtures, sem suporte Switch 2 | [Ferramenta de pesquisa](../../tools/research/nx2/README.md) |
| SDK v0.42, fontes locais, compilação de objetos e limites de ABI | [Entradas nativas](implementation-20261031/research/native-inputs.md) |
| Firmware do PS5 versus firmware do sistema convidado | [Compatibilidade](implementation-20261031/research/firmware-compatibility.md) |
| Interfaces distintas e referência ps5-homebrew-ui | [Adaptação de UI](implementation-20261031/research/ui-adaptation.md) |

## Comparações de origem

- [Amiibo, cheats, Nextendo e teclado](sources/game-integration.md).
- [Armazenamento, SMB/NFS e backup/restauração](sources/storage-network.md).
- [Controles, apresentação e forwarders](sources/input-presentation.md).
- [Intake histórico de interfaces e Switch 2](sources/themes-switch2.md).

## Planos e registros históricos

Estes documentos preservam o escopo e as decisões das etapas em que foram
escritos. Para o estado posterior, use o progresso e as decisões acima; uma
aprovação antiga de plano não significa que a implementação foi concluída.

- [Visão geral aprovada na etapa inicial](approved-overview-01.md).
- [Plano técnico aprovado na etapa inicial](approved-technical-plan-01.md).
- [Matriz inicial de evidências por feature](feature-evidence.md).
- [Plano do incremento de armazenamento](implementation-20261031/storage-increment-plan.md).
- [Inspeção das capturas reais do preview C++](implementation-20261031/visual-inspection.md).
- [Histórico do fluxo de trabalho](workflow.md).

## Histórico das revisões

### Projeto geral

- [Relatório anotado](goldfish_review_report.md) e [configuração original](review-loop-20261031/configuration.md).
- [Consolidado](review-loop-20261031/round-01/consolidated.md),
  [design](review-loop-20261031/round-01/design.md),
  [estrutura](review-loop-20261031/round-01/structural.md),
  [estilo](review-loop-20261031/round-01/style.md) e
  [proposta de resolução](review-loop-20261031/round-01/resolution-proposal.md).

### Incremento de implementação

- [Rodada 01](implementation-20261031/reviews/increment-01/returned-findings.md).
- [Rodada 02](implementation-20261031/reviews/increment-02-final/goldfish_review_report.md).
- [Rodada 03](implementation-20261031/reviews/increment-03-final/goldfish_review_report.md).
- [Rodada 04](implementation-20261031/reviews/increment-04-final/goldfish_review_report.md).
- [Rodada 05 e limite do ciclo](implementation-20261031/reviews/increment-05-final/goldfish_review_report.md),
  [readiness](implementation-20261031/reviews/increment-05-final/readiness-review.md) e
  [fidelidade](implementation-20261031/reviews/increment-05-final/fidelity-review.md).
- [Validação pós-limite — não é sexta rodada](implementation-20261031/reviews/increment-post-cap-stream-repair-validation/VALIDATION_ONLY.md).

## Evidências e manutenção

- [Localização e alcance dos artefatos externos](EVIDENCE.md).
- [Registro da migração](MIGRATION.md).

Esta pasta é a referência para futuras atualizações da documentação do roadmap.
Os originais externos foram preservados como histórico. Logs, capturas, objetos,
SDK extraído, snapshots de código e recibos brutos não foram incorporados ao Git.
