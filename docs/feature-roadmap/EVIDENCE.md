# Evidências externas e referências

## Documentação versus artefatos

Planos, decisões, pesquisas, progresso e relatórios narrativos estão nesta pasta.
Os documentos são legíveis sem o diretório temporário do OpenCode. A reprodução
ou auditoria dos resultados brutos, porém, depende dos artefatos externos abaixo;
não se afirma que esses artefatos estejam disponíveis em um clone do projeto.

Raiz histórica de evidências na máquina de origem:

```text
C:/Users/Leonardo Mello/AppData/Local/Temp/opencode/encore-feature-ports-20261030/
```

Os nomes de recibos, scripts, capturas e diretórios de validação escritos entre
crases nos documentos migrados continuam relativos a essa raiz ou à subpasta
`implementation-20261031/`, conforme o contexto original. Não são caminhos para
arquivos de build dentro de `docs/`.

## Artifacts

Sob `implementation-20261031/` na raiz externa:

| Diretório/arquivo | Alcance registrado |
| --- | --- |
| `focused-checks-post-cap-stream-repair/` | Dez regressões host e sintaxe Python de 751 arquivos; inclui oito falhas de alocação injetadas na enumeração. |
| `preview-post-cap-stream-repair-normal/` | 104 capturas do preview C++ host com dados sintéticos. |
| `preview-post-cap-stream-repair-ptbr-accessible/` | 104 capturas pt-BR acessíveis e verificações dos hashes da fonte. |
| `native-sdk-audit-01/receipt.json` | Primeiro ensaio: 5/13 objetos; oito unidades interrompidas por include GL ausente no ensaio. |
| `native-sdk-audit-02/receipt.json` | Segundo ensaio: 16/21 objetos; cinco unidades interrompidas por include JSON ausente no ensaio. |
| `native-sdk-audit-03/receipt.json` | Terceiro ensaio: 21/21 unidades do launcher/auxiliares compiladas para `x86_64-sie-ps5`; não é link completo. |
| `native-filesystem-abi-01/` | Inspeção estática dos wrappers de filesystem do SDK v0.42, sem execução no console. |
| `native-discovery-receipt-01.json` | Preservação de 1.533 arquivos antes desta migração e hashes das fontes/ferramentas inspecionadas. |
| `primary-baseline/`, `preservation-audit.json` | Baseline e comparação históricos do incremento, anteriores à adição desta documentação. |
| `reviews/` | Pacotes imutáveis, manifestos, alvos e snapshots exatos das revisões. Os relatórios narrativos foram copiados para esta pasta; os pacotes completos permaneceram externos. |

O recibo de audit03 tem SHA-256
`3833d71a46eb97a6a79c56a6ebbfa7cde1cc85bb43567802d2c12035be28efe8`.
Seu PASS não cobre o core selecionado, `headless/main.cpp`, o provider real
`eden_services.cpp`, HarfBuzz/fontes do sistema ou link/package completo.

Nenhum resultado host ou compilação de objeto estabelece gameplay, ABI de
runtime, comportamento de filesystem, funcionamento de serviços ou suporte a
firmware do PS5. As falhas anteriores não foram apagadas ou renomeadas como PASS.

## Provenance

Recibos e referências históricas externos, relativos à raiz acima:

- `scope-extension-01/input-provenance.json`: pins e hashes da documentação de
  ps5-homebrew-ui e Eglantine consultada no intake.
- `scope-extension-01/eglantine-reference/`: bytes originais dos documentos
  upstream salvos; links navegáveis agora apontam para o mesmo commit público.
- `source-comparison-baseline.json`, `tracked-owned-hashes-before.json` e demais
  `*-verification.json`: inventários e verificações da etapa correspondente.
- `egm-runtime/`: cópia das personas e padrões utilizados, não parte do produto.
- `documentation-migration-20261102/`: recibos de cópia e validação desta migração.

As referências locais ao projeto irmão `ps5-homebrew-ui` foram tornadas
relativas ao workspace `PS5/`. Esse projeto não foi copiado para Encore; sua
revisão estudada continua sendo `bbe19054ee2e7ec069254775273aeb8846c4bd3e`.
Paths absolutos em citações históricas e comandos preservam a máquina de origem,
não uma exigência de instalação para ler estes documentos.

## Novas evidências

### Campanha de50 agentes — 2026-11-07

Raiz externa independente:
`C:/Users/Leonardo Mello/AppData/Local/Temp/opencode/encore-campaign-20261107/`.
Os50 relatórios e decisões de integração estão permanentemente em
[campaign-20261107](campaign-20261107/README.md).

| Caminho nessa raiz | Alcance |
| --- | --- |
| `baseline.json`, `dispatch.json` |1.571 arquivos próprios antes do dispatch e50 sessões distintas; não implica checkout limpo. |
| `A49/`, `coordinator-A49-verification-01.json` | Font/SystemFonts/HarfBuzz objetos, link relocável com60 símbolos indefinidos e matriz A11 extraída; TB-01 ainda não qualificado. Falha inicial de depfile preservada. |
| `A50/evidence-index.json`, `A50/preservation-final-v2.json` | Índice e preservação da primeira etapa host; fontes/dados anteriores não descartados. |
| `A50/a02-01/out/{result,interpreted-result}.json` | Runner bruto exit1 e interpretação separada de quatro expectativas diagnósticas erradas;40/40 casos candidatos, A14-05 aberto. |
| `A50/a10-02/` | Compilação/link host, baseline render parcial e interrupção por prioridade exit137; não é PASS visual nem OOM demonstrado. |
| `integration-01/{preflight,source-applied,promoted-files}.json` | Readback de nove recibos, três deltas aplicados/hash-idênticos aos overlays e identidade/relocação das fixtures permanentes. Validação integrada posterior é separada. |
| `A50/integrated-04/`, `integration-01/coordinator-post-validation-01.json` | Checkout/fixtures permanentes executados:10/10 regressões,755 sintaxe Python,40/40 writers+OPEN separado,4/4 modos extraídos e29/29 pesquisa.177 artefatos do índice e12 pins vivos conferidos; preservação/cleanup PASS. |
| `A50/integrated-01/`, `integrated-02/`, `integrated-03/` | Recusas de preflight e censo estrito false preservados; não são o PASS final da entrega permanente. |
| `A12/inputs/G-NOTES.md` | Input documental bruto completo/pinado para o teste SHA/blob/quote; proveniência e instruções independentes da máquina em tools/research/nx2. Não é código incorporado. |
| `A46/ownership-only-v1/` | Patch e fixtures revisados estaticamente, ainda NEXT_WAVE/NOT_RUN. |

Resultados posteriores da mesma campanha, sem reclassificar os recibos acima:

| Caminho nessa raiz | Alcance atual limitado |
| --- | --- |
| `A50/integrated-03-live-01/` | Core live03 PASS:27 pins,10 regressões,757 sintaxe,40 writers,API68+5/11 negativos e harness promovido. A10/global continua FAILED_OR_BLOCKED pelo oracle UTF-8 inválido. |
| `A50/ui-evt-gl-r2-01/`, `A14/review-ui-evt-gl-r2-receipts-01/` | Preflight real+30 eventos+5 mutantes exatos;337 artefatos conferidos. A14-05 fechado somente fonte/API/evento host com TEST adapter/persistência real/sete pins7f, não P02/native. |
| `A17/independent-de05-closure-01/` | DE-05-01 fechado independentemente fonte+host no mecanismo exato Entries; cap5/não convergente e qualificação nativa inalterados. |
| `A50/readfile-harness-r3-01/`, `A17/readfile-r3-postreview-composed-01/` |14 fases/6deps/69+5/gates/nm,81 artefatos; aceite isolado c94 fonte+host. Não altera produção7f nem recibos r1/default FAIL. |
| `integration-04-readfile-candidate-01/`, `A50/readfile-composed-r4-01/` | Novo composto externo7f→8db59 passou gates HOST limitados,938 artefatos conferidos; A17 autorizou os3hunks, aplicados depois em integração04. Mesmos bytes antigos explicitamente, sem reclassificar BLOCKED histórico. |
| `A50/a10-utf8-oracle-r1-01/` | Oracle-only sobre160 evidências: verify0/controls0/MATCH/20tuples140obs por idioma/sete negativos exatos;46 artefatos. A22 aceitou suplemento e resolveu pendência textual só histórico7f/e083. Sem novo C++/render/PASS retroativo/global. |
| `A46/case-page-ownership-r1/`, `A49/wave02-case-page-native-01/` | Candidato Textures27c7c2… externo;9files/17pins e revisão A17/A29. Um objeto nativo real compilou,869deps/ELF REL/79 imports indefinidos/14 artefatos conferidos; não runtime/lifetime/GPU. |
| `A50/case-page-ownership-r1-{01,02}/` | Ambos wrappers falharam antes do runner:01censo/17artefatos/sem container;02TypeErrorstr/str/62artefatos/cleanup.11 comandos NOT_RUN em cada, nenhuma prova GL. A29 prepara fix isolado, sem terceira tentativa admitida. |
| `A02/ui-evt-composed-pin-r1/`, `A50/ui-evt-composed-pin-r1-01/` | Contrato TEST22c3dab1…/derived8db0 executado30+5exatos/361artefatos; A14 aceitou fonte/API/HOSTevent no contexto exact8db+seis atuais/Library5a/Texturese083, sem herdar7f ou qualificar P02/global/native. |
| `A46/presence-cancel-r1/`, `A50/presence-cancel-r1-01/` | Function-real HOST PASS/61artefatos/23pins; A40 autorizou só2hunks5a→034, aplicados depois em integração05. A46-05 parcial, sem UI034, prazo/cancel de syscall ou native lifetime qualificados. |
| `A49/wave02-presence-cancel-native-01/`, `integration-03/presence-cancel-native-closed-readback-01.json` | Uma TU Library candidata inteira compilada SDK0.42,1003deps/ELF REL/155 imports indefinidos;14 artefatos/header ELF/23pins/drain conferidos. Não é lifetime/host/link/runtime/ABI/PS5. |
| `A50/a35-guard-selection-supplement-01/`, `A23/A50-a35-guard-selection-supplement-01-postQA/` |83artefatos/16IDs/11errosprimários exatos, QA-P11-01 satisfeita apenas guard projection. Não full-main/argv/runtime/lifecycle/P11global. |
| `integration-04-readfile-live-01/` | Aplicação serial exata dos3hunksfe802,pre7f/post8db,1266outros paths intactos; receipts/aceites limitados. Tentativas live01/02 falharam na preparação metadata, CORE/UI NOT_RUN, sem PASS herdado. |
| `A29/case-page-enclosure-fix-r1/`, `integration-04-readfile-live-01/texture-fix-conditional-and-presence-ui-review-01.json` | Fix25853e1c…/11artefatos,só join `/proc`;supervisor/censo/17inputs A46 intactos. A17 aprovou, nova execução condicional A50 apenas após live02terminal/cleanup/preservação, snapshot8db/inspectnovos; resultado não antecipado. |
| `A02/ui-evt-presence-pin-r1/` | Contrato6f7d1364…/10artefatos8db+Library034/verifierf130 aprovado A14/A26, execução NOT_RUN/não admitida. Presença aplicada depois em integração05; contrato ainda exigeTexturese083, não aceita27c7. Novo contrato conjunto em preparação. |
| `A50/integrated-readfile-live-02/` |26artefatos conferidos,bootstrap comparouhashqueue-handoff aHANDOFF.md,CORE/UI NOT_RUN/0containers. Falha de binding A50, não falha de produto; histórico imutável. |
| `A50/case-page-enclosure-fix-r1-01/` | Primeiro REAL-GL HOST diferencial PASS limitado,88artefatos/11comandos/7deps/16faults por variante:baseline10/0/16/16/0,candidato0/0/0/16/16. A17 autorizou3hunks, aplicados depois em integração06; não PASS combinado/native/renderglyph. |
| `integration-05-presence-live-01/` | Aplicação serial exata2hunks2ffc,Library5a→034,1266outros paths intactos/store8db/Texturase083. Preservação23+17pins e0jobshandoff; nova validação conjunta ainda pendente. |
| `integration-06-case-page-live-01/` | Aplicação exata3hunksb29,Textures e083→27c7,1266outros paths/16outros inputs intactos;store8db/Library034 preservados. Context2772b5a9… fecha três integrações e1267 fontes com roles/path/hash/bytes; combinado live ainda pendente. |
| `integration-06-case-page-live-01/source-pin-projection-receipt.json` | Dois mapas27 preservam chaves e alteram só store/Library; Texture27c7 está no mapa completo e UI51, não é pin inventado nos27. README5af124…/8535bytes fora do censo1267 é referência auxiliar exata; qualquer outro ausente bloqueia. Dados, não execução. |
| `A29/live-combined-core-bindings-r1/` | Contrato estático CORE/checklist/bindings, três hashes/32 referências conferidos; separa roles, snapshots04→05→06 e história ancestral. Adendo estreito/preparo+execução e admissão completa ainda necessários; nenhum run autorizado. |
| `A02/ui-evt-presence-texture-pin-r1/` | Joint UI e149fc29… aprovado A14/A26 para teste,sevenpins/source51 estritos,verifier2edb/runner550388 explícitos. Execução final NOT_RUN/sem admissão, nenhum PASS transferido. |
| `A29/live-combined-core-bindings-r1-amendment-01/` | AMENDMENT8ad360…/ancillaryf1695… conferidos: só README5af124…/8535bytes fora1267; fases PREPARE_ONLY/EXECUTION e quatro mappings06 explícitos. Originais preservados. |
| `integration-06-case-page-live-01/final-combined-admission-01/` |124bindings e destinos exclusivos;25índices+ancestral formam união6083 sem conflito,6080regulares rehashados/3aliases reservados ao adaptador existente. PREPARE_ONLYd798c557…/inputs3a923540… autorizam somente metadata staging/scripts; manifesto derivado será saída, não teste/PASS ou execução já autorizada. |
| `A50/final-combined-live-01/prepared/derivation-manifest.json` | Preparo fechado19da61fe…/259015bytes/248artefatos;196fontes/124cópias/41anchors e3aliases conferidos.0produto/Docker na preparação. Deltas funcionais preservados, G1 do launcher registrado por A29; preparados imutáveis. |
| `A29/final-combined-postprepare-review-01/` | Review de deltas/plumbing: G1 exige gate externo para novo terminal/auth/manifest e closure CORE antesUI; não autoriza teste nem reclassifica preparo. |
| `integration-06-case-page-live-01/final-combined-admission-01/execution-core-01/` | Gate metadata255329… executado/receipt2186b56e… validou132bindings/44roles+ancillary/novo terminal299a/auth16b96/manifest19da. CORE900+10 admitido a A50, resultado pendente; UI não autorizada sem CORE real/closure/hashseal e nova permissão. |
| `A50/final-combined-live-01/execution/core/`, `A50/final-combined-core-dispatch-01/` | CORE interno rc0; dois coletores metadataFAIL/core_passfalse. Container removido/cleanup0/drainvazio/0jobs; índices1274aec8ef62…/1472b70ca13… fechados. UI NOT_RUN, falhas originais preservadas. |
| `integration-06-case-page-live-01/final-combined-admission-01/core-closed-readback-and-auth-block-01.json` |46209a5e… vincula índices e verificação parcial1271+147regulares/produção1267/preparados248, sem PASS dos reparses pendentes. A29/A17 bloqueados por token expirado; nenhuma entrega suplementar, substituto ou retry. Retomar mesmas sessões após renovar autenticação. |

As fixtures úteis e o CLI de pesquisa estão na árvore fonte, não apenas nos
artefatos externos; logs/objetos/binários/capturas/SDK e resultados brutos continuam
externos. Ver [integração](campaign-20261107/INTEGRATION.md) para o run Docker
combinado final, que não sobrescreveu nem reinterpretou como PASS uma falha antiga.

Novos ensaios devem usar identificadores e saídas exclusivos e guardar seus
recibos fora da árvore fonte. Atualize o [progresso](implementation-20261031/progress.md)
e o estudo pertinente aqui no repositório, identificando a versão, o ambiente,
as limitações e o endereço do novo recibo. Não reabra o ciclo EGM encerrado nem
reclassifique resultados históricos apenas por migrar a documentação.
