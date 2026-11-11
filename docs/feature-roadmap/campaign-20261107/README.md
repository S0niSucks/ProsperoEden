# Campanha de engenharia — 2026-11-07

Solicitação: 50 agentes, todas as doze frentes do roadmap e especialistas.
**50 dispatches e50 primeiros retornos recebidos**, com sessionIDs próprios no
[registro de dispatch](DISPATCH.md). As continuações reutilizam essas sessões.
Os três deltas qualificados A05/A02/A11 foram integrados serialmente; a validação
conjunta passou no host:10/10 regressões,40/40 casos dos writers,4/4 modos de
sintaxe extraída e29/29 métodos de pesquisa. Isso não significa roadmap completo
ou suporte nativo. A14-05 segue aberto, A10/A46 pendentes, Switch2 unsupported.
Consulte o [brief comum](BRIEF.md).

Os [retornos efetivamente recebidos](STATUS.md) são registrados separadamente
dos dispatches. Nenhuma frente é marcada completa pela simples entrega de análise.
Os candidatos concretos e suas condições estão na [fila de integração](INTEGRATION.md).
Após o PASS da integração01, o proprietário autorizou [continuidade em ciclos
e onda02](WAVE-02.md), reutilizando o mesmo time e mantendo os bloqueios explícitos.

## Distribuição — 50 agentes distintos

| Frente | Desenvolvimento | Code review | QA |
| --- | --- | --- | --- |
| P01 Amiibo `.bin` | A01 | A13 | A25 |
| P02 cheats | A02 | A14 | A26 |
| P03 SMB | A03 | A15 | A27 |
| P04 NFS | A04 | A16 | A28 |
| P05 pastas/conteúdo | A05 | A17 | A29 |
| P06 Nextendo | A06 | A18 | A30 |
| P07 teclado | A07 | A19 | A31 |
| P08 Pro Controller USB | A08 | A20 | A32 |
| P09 backup/restore USB | A09 | A21 | A33 |
| P10 interfaces/aparência | A10 | A22 | A34 |
| P11 forwarder | A11 | A23 | A35 |
| P12 Switch 2 | A12 | A24 | A36 |

| ID | Especialidade e responsabilidade |
| --- | --- |
| A37 | Arquitetura: dependências, propriedade de arquivos e ordem de integração |
| A38 | Arquitetura: filesystem, persistência e recuperação |
| A39 | Arquitetura: rede, trust boundaries e isolamento por sessão |
| A40 | Arquitetura: APIs de core/frontend e ciclo de vida |
| A41 | Computação: ISA/semântica CPU do Switch 2 |
| A42 | Computação: memória, backing e orçamento viável de Switch 2 |
| A43 | Computação: tradução de GPU/shaders e limites de Switch 2 |
| A44 | Alta performance: CPU/JIT, profiling e hipóteses mensuráveis |
| A45 | Alta performance: GPU, residência e pressão de memória |
| A46 | Alta performance: UI, I/O e agendamento assíncrono |
| A47 | PO: priorização, dependências e entregas úteis ao jogador |
| A48 | PO: critérios de aceitação e rótulos honestos de capacidade |
| A49 | Integração: build nativo isolado e lacunas dos ensaios anteriores |
| A50 | Integração/QA: baseline, preservação e execução central dos testes host |

## Etapas

1. Cada agente trabalha sem editar código compartilhado, entregando seu relatório
   em `agents/Axx.md`; patches e recibos ficam externos.
2. O coordenador consolida bloqueios e escolhe incrementos verificáveis.
3. Revisores e QA conferem os candidatos reais antes de integração. Uma avaliação
   inicial da baseline não é aprovação antecipada desses candidatos.
4. A49/A50 executam validação centralizada conforme o brief. Só o coordenador
   altera a implementação principal e atualiza o progresso consolidado.

Não há promessa de que 50 agentes eliminem limites de hardware, contratos de
core ainda não verificados ou falta de evidência. Esta campanha não qualifica
Switch 2, novas versões de firmware ou online apenas pela quantidade de agentes.
