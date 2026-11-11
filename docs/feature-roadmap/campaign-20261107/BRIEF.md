# Brief comum — campanha de 50 agentes

## Objetivo e estado inicial

Em 2026-11-07 o proprietário pediu 50 agentes cobrindo desenvolvimento, code
review, QA, PO, arquitetura, computação e alta performance nas doze frentes.
Esta é uma nova campanha de engenharia, não uma sexta rodada do ciclo EGM
anterior nem uma dispensa dos seus findings.

Leia antes de trabalhar: `D:/leonardommello/PS5/AGENTS.md`, instruções aplicáveis
ao caminho investigado, `docs/feature-roadmap/decisions.md` e
`docs/feature-roadmap/implementation-20261031/progress.md`. Consulte os estudos
relevantes em `docs/feature-roadmap/sources/` e `implementation-20261031/research/`.
Histórico documental não concede permissões adicionais. Use as skills aplicáveis.
Para código, leia as instruções iniciais de Serena; se a ferramenta não enxergar
o projeto Windows, registre o fallback, sem criar configuração no checkout.

Switch 2 não está implementado. Amiibo usa apenas `.bin`, sem leitor físico.
Há trabalho local não commitado que deve ser preservado. O SDK v0.42 existe:
21 unidades do launcher/auxiliares compilaram, não o core/provider/link completo.

## Isolamento e autoridade

- O checkout `D:/leonardommello/PS5/ProsperoEden-Encore` é SOMENTE LEITURA para
  agentes, exceto o único relatório `docs/feature-roadmap/campaign-20261107/agents/Axx.md`
  atribuído individualmente. Não edite outro arquivo compartilhado, nem outro
  relatório. O coordenador integra código depois de revisão/QA.
- Candidatos de código, patches, fixtures, scripts, logs e recibos ficam apenas em
  `C:/Users/Leonardo Mello/AppData/Local/Temp/opencode/encore-campaign-20261107/Axx/`.
  Crie seu diretório de forma exclusiva. Copie somente arquivos permitidos e
  necessários para seu overlay; preserve os bytes de entrada e registre hashes.
- Não inspecione semanticamente nem modifique `vendor/sources/eden/**`, nem
  contorne essa restrição por cópias do mesmo original. Fontes mantidas em
  `vendor/encore-active/` podem ser lidas quando pertinentes, nunca editadas nesta
  etapa. Respeite outros AGENTS locais. Não copie árvores inteiras de dependências.
- Nenhum reset, checkout destrutivo, staging, commit, push, publicação, deploy,
  instalação de ferramentas, operação de console, montagem/restauração ou leitura
  de dados pessoais, chaves, contas e credenciais. Sem contato com serviços de
  jogos/contas. Apenas leitura de fontes públicas documentais quando necessária.
- Não crie subagentes: os 50 IDs são controlados pelo coordenador. Nenhuma rodada
  EGM é iniciada automaticamente. Não declare fechamento independente de DE-05-01
  sem uma revisão específica dos bytes correspondentes.

## Builds e integração

Todos os builds continuam Docker-only. **Somente A49 executa compilação nativa
e somente A50 executa a suíte host nesta etapa**, evitando 50 toolchains em
concorrência. Os demais escrevem fixtures/comandos reproduzíveis para execução
centralizada e registram NOT_RUN. Não execute compiladores no Windows host.
A49/A50 usam imagem existente, rede desabilitada, repo read-only, saídas externas
exclusivas, timeout/cleanup de processos e limites razoáveis de CPU/memória.
Não polua `.local`, `.deps`, `build/`, `dist/` ou `vendor` do checkout.

Não habilite `EDEN_NATIVE_USER_CONTENT_IO_QUALIFIED` por simples compilação.
Preserve recusa de configuração corrompida, criação exclusiva/no-follow de
staging, cancelamento final antes do rename e drenagem dos donos assíncronos.
Nextendo continua default-off, por usuário/sessão e destinos verificados; DNS
sozinho não cria online. SMB/NFS exigem share estável e envenenamento de leitores
após mutação. Restore é explícito/confirmado, validado e aplicado no próximo
startup, nunca em dados reais como fixture. Preserve Switch 1, saves e loaders.

## Entrega por papel

- **Desenvolvedor:** produza um incremento pequeno e útil contra interfaces
  realmente verificadas, com patch/overlay e fixture. Se faltar contrato
  indispensável, não invente parser, ABI, offsets, método de core ou sucesso:
  entregue o bloqueio exato e uma alternativa segura justificável. Não faça um
  framework especulativo ou uma UI que simule uma feature funcional.
- **Code reviewer:** examine a baseline atual da frente, registre findings
  reproduzíveis com severidade, caminho/linhas e correção. Defina o que conferir
  no candidato do desenvolvedor em uma segunda etapa; não espere/polle outro
  agente e não revise um patch inexistente como se estivesse aprovado.
- **QA:** derive critérios de aceitação e entregue fixtures/controles negativos
  executáveis para a frente, cobrindo falhas, cancelamento, limites e regressões.
  Não invente integração em um mock. Testes não executados são NOT_RUN.
- **Especialista/PO:** entregue uma decisão ou artefato técnico utilizável,
  dependências, evidência, condições e limites. Evite repetir todo o roadmap.

Escreva seu relatório permanente no arquivo Axx.md, com: papel/frente; escopo e
hashes lidos; resultado; arquivos candidatos; testes realmente executados ou
NOT_RUN; bloqueios; dependências para integração. Diferencie fonte, compilação,
host e console. Mantenha o relatório enxuto (idealmente até 150 linhas).
Na resposta final ao coordenador, use no máximo 180 palavras: caminho do relatório,
patch/fixture se houver, estado PATCH_READY/ANALYSIS_COMPLETE/BLOCKED/TESTED e
o próximo passo. Não resuma outras frentes e não prometa execução futura autônoma.
