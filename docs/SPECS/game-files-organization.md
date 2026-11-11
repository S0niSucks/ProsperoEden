# Especificação: Auto-organização da pasta de jogos

**Versão:** 1.0 · **Estado:** aprovada · **Dono:** coordenador · **Fronteira:** headless/ (scanner de biblioteca, serviços de conteúdo)

## 1. Objetivo

O sistema gerencia o layout da pasta de jogos sem exigir qualquer ação ou
conhecimento do usuário. Arquivos de jogo colocados **em qualquer pasta
dentro da estrutura** são descobertos, classificados e reorganizados
automaticamente para o layout canônico. A operação é segura (nenhum dado
é perdido), idempotente (execuções repetidas não movem nada) e
transparente (journal no log).

## 2. Princípios

1. **Zero configuração** — não existe opção de layout; há um único padrão.
2. **Auto-descoberta recursiva** — o usuário pode depositar arquivos em
   qualquer subpasta da estrutura; o scanner os encontra.
3. **Auto-organização** — o sistema migra os arquivos para o layout
   canônico por conta própria, no momento do scan.
4. **Segurança de dados absoluta** — somente movimentação atômica
   (`rename`); nenhuma exclusão; arquivos sem destino certo permanecem
   onde estão.
5. **Idempotência** — organizar um estado já organizado é no-op.
6. **Tolerância a falha isolada** — a falha de uma movimentação não aborta
   as demais; o resultado parcial é reportado.

## 3. Layout canônico (padrão único)

```
<game files folder>/
└── roms/
    └── <Nome do Jogo>/
        ├── <Nome do Jogo>.nsp|.xci|.nup     (base)
        ├── <arquivo de update>.nsp           (updates)
        ├── <arquivo de DLC>.nsp              (DLC)
        └── (opcional) exefs/ + romfs/        (dump extraído)
```

- **Base, updates e DLC vivem juntos** na pasta do jogo.
- Pasta de jogo = nome do arquivo base sem extensão e sem sufixos
  identificadores (`[Update]`, `[DLC]`, tags entre colchetes no final).
- A pasta `updates/` global permanece existindo como local de entrada
  aceito, mas não é o destino de nada após a organização.

## 4. Entradas aceitas (o que o scanner reconhece, em qualquer lugar)

| Origem | Descrição |
| --- | --- |
| `roms/<arquivo>` | arquivo de jogo solto na raiz de roms/ |
| `roms/<pasta>/<arquivo>` | arquivo dentro de subpasta imediata |
| `roms/<...>/<...>/<arquivo>` | **qualquer profundidade** dentro de roms/ (varredura recursiva limitada, ex. profundidade 8, com proteção contra ciclos/symlinks) |
| `updates/<arquivo>` | update/DLC na pasta global |
| `roms/<pasta>/exefs/main` | dump extraído (regra existente preservada) |
| `.nup` | pacote único (base+update+DLC) — não é desempacotado; migra como base |

Extensões reconhecidas: `.nsp`, `.xci`, `.nup` (insensível a caixa).

## 5. Classificação base × update × DLC

1. `.nup` é sempre **base**.
2. Arquivos cujo nome contenham sufixo identificativo — `[Update]`,
   `[DLC]`, `[UPD]`, padrões comuns de coleções — são **complementos**.
3. Caso contrário: arquivos distintos no mesmo título são
   **base = o de maior precedência** (ordem documentada: `.nup` >
   `.xci` > `.nsp`; entre iguais, o primeiro em ordem lexicográfica) e os
   demais complementos por exclusão.
4. Empates ambiguos não adivinham: classificam-se como complemento e o
   journal registra a decisão.

## 6. Casamento complemento → jogo

1. **Title ID** (16 hex no nome do arquivo) — casamento exato.
2. Fallback: **nome normalizado** (caixa, acentos, pontuação, espaços).
3. Complemento sem jogo correspondente = **órfão**: permanece onde está,
   com linha de journal; é reavaliado no próximo scan.

## 7. Algoritmo de organização (executado no scan da biblioteca)

```
1. DESCOBRIR   — varredura recursiva de roms/ (+ updates/) coletando
                 bases, complementos, dumps; nada é tocado ainda.
2. CASAR       — complementos → base por title ID, depois nome.
3. PLANEJAR    — para cada arquivo fora do canônico: destino
                 roms/<Jogo>/; resolução de colisão (ver §8).
4. EXECUTAR    — mover com rename (mesmo volume, atômico); verificar
                 tamanho pós-move; falha registrada e segue a próxima.
5. JOURNAL     — linha por movimentação: "organized: <origem> -> <destino>";
                 resumo final: "organized N files, M orphans kept, K failures".
```

Ordem de execução: bases antes de complementos (a pasta destino nasce
com a base).

## 8. Regras de colisão e deduplicação

- **Pasta destino já existe** → o arquivo entra nela (é o caso normal de
  complemento alcançando sua base).
- **Arquivo de mesmo nome já existe no destino** → destino renomeado com
  sufixo incremental (`Jogo (2).nsp`); nada é sobrescrito, nunca.
- **Mesmo jogo descoberto em múltiplos lugares** (ex.: base solta em
  roms/ e cópia dentro de pasta) → **uma** entrada na biblioteca; o
  layout canônico vence: a cópia fora do padrão migra para dentro da
  pasta e, no conflito de nome, a que já estava na pasta permanece como
  principal (a outra vira o sufixo incremental).
- **Base duplicada de extensões diferentes** (`.nsp` + `.xci`) → ambas
  coexistem na pasta; a biblioteca lista o jogo uma vez usando a
  precedência do §5.3.

## 9. Regra de aplicação de updates/DLC

- Jogo com pasta própria: **somente** os complementos da própria pasta
  aplicam; `updates/` global não é consultado para esse título (impede
  dupla aplicação).
- Jogo sem pasta (ainda não organizado, ex.: órfão deliberado):
  `updates/` global continua válido como hoje.
- `.nup`: update/DLC embutidos aplicam a partir do próprio pacote.

## 10. Garantias formais

| # | Garantia |
| --- | --- |
| G1 | Nenhum arquivo é excluído ou sobrescrito em nenhum caminho do algoritmo. |
| G2 | Toda movimentação é `rename` no mesmo volume (atômica no filesystem). |
| G3 | Pós-move, o tamanho do arquivo no destino confere com o de origem. |
| G4 | Idempotência: segunda execução consecutiva realiza zero movimentações. |
| G5 | Falha em uma movimentação não afeta as demais; falhas são listadas. |
| G6 | Órfãos nunca saem do lugar nem somem da biblioteca de entradas aceitas. |
| G7 | Dumps extraídos (exefs/romfs) migram como unidade (pasta inteira). |
| G8 | O journal registra toda decisão de classificação ambígua (§5.4). |
| G9 | A varredura ignora symlinks/ciclos e respeita o limite de profundidade. |
| G10 | Contagens de setup (chaves/firmware/jogos por pasta) permanecem coerentes pós-organização. |

## 11. Observabilidade

- Log por movimentação e resumo por scan (§7.5).
- Configuração: nenhuma (§2.1). Variável de ambiente de diagnóstico
  `EDEN_ORGANIZER_DRY_RUN=1` pode existir para relatar sem mover
  (ferramenta de suporte, não opção de usuário).

## 12. Não-objetivos

- Converter formatos (.nsp↔.xci↔.nup), extrair ou desempacotar NUP.
- Baixar, buscar metadados ou renomear por bases externas.
- Reorganizar saves, mods, chaves, firmware — apenas jogos e seus
  complementos.
- Mover arquivos entre volumes/dispositivos diferentes.

## 13. Critérios de aceitação (cenários de teste mínimos)

1. **Caos completo**: base solta + update em updates/ + DLC em subpasta
   profunda + jogo já organizado + órfão + dump → um scan organiza tudo
   no canônico; órfão intacto; segundo scan = zero movimentos.
2. **Dedup plano×pasta**: mesmo jogo nas duas formas → 1 entrada;
   plana migra; nada sobrescrito.
3. **Colisão de nome**: dois arquivos iguais chegando ao mesmo destino →
   sufixo incremental; conteúdo de ambos preservado.
4. **Órfão com update**: update sem base permanece; base aparece depois
   (novo scan) → casamento ocorre e organiza.
5. **Falha de move** (injetada): demais movimentos concluem; falha no
   relatório; re-scan conclui o pendente.
6. **Dump profundo**: exefs/ dentro de subpasta qualquer → pasta inteira
   migrada como unidade para roms/<Jogo>/.
7. **Contagens de setup**: números exibidos no navegador de pastas
   batem com o estado pós-organização.

## 14. Referências

- Doutrina UX: `docs/feature-roadmap/decisions.md` AUTH-07.
- Implementação de referência: A05/roms-dual-layout-r1 (a validar).
- Projeto irmão (comportamento análogo de entrada): EglantinePS5,
  `docs/feature-roadmap/sources/reference-projects.md`.
