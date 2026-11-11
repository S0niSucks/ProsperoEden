# Migração da documentação — 2026-11-02

## Escopo autorizado

O proprietário solicitou trazer a documentação de trabalho para este projeto.
Foram incorporados **33 documentos Markdown existentes** e criados três guias:
o [índice](README.md), o [mapa de evidências](EVIDENCE.md) e este registro.

Destino: `docs/feature-roadmap/`. A estrutura relativa dos documentos foi mantida
para conservar a navegação entre planos, decisões, estudos e revisões.

Origem histórica:

```text
C:/Users/Leonardo Mello/AppData/Local/Temp/opencode/encore-feature-ports-20261030/
```

## Preservação

- Os originais externos não foram removidos nem editados; agora são cópias
  históricas. A documentação permanente passa a ser mantida neste repositório.
- Todos os arquivos do projeto existentes antes desta migração foram preservados.
  Não houve alteração de código, README preexistente, dados de usuário ou core.
- Os relatórios das revisões permanecem com seu conteúdo original. Datas, hashes,
  findings, não convergência e limites de leitura/execução não foram promovidos.
- Nos documentos de navegação, links absolutos para Encore foram convertidos em
  links relativos. Referências locais de Eglantine usam o mesmo commit público;
  os recibos brutos continuam identificados como externos.
- As decisões e o progresso receberam apenas o registro da nova localização;
  o fluxo antigo foi identificado como histórico e os limites dos artefatos
  externos foram explicitados no estudo de entradas nativas.

## O que não foi incorporado

Logs, PNGs, objetos, SDK extraído, scripts auxiliares dos ensaios, baselines,
snapshots de código/alvos imutáveis e recibos JSON brutos permanecem externos.
Também não foram duplicadas dependências ou a documentação original de terceiros.
Consulte o [mapa de evidências](EVIDENCE.md) para localizar esses materiais.

## Verificação

O recibo externo de cópia conserva os hashes SHA-256 e tamanhos dos 33 originais,
além dos hashes de todos os arquivos preexistentes do projeto. A verificação da
migração compara esses bytes, confere os destinos dos links Markdown e executa
`git diff --check`, sem novo build ou ensaio de hardware.

A migração não é uma revisão EGM, aceitação do roadmap, nova evidência de suporte
a Switch 2 ou qualificação do PS5. Nenhum commit, staging ou deploy foi realizado.
