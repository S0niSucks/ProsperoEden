# Backlog da doutrina AUTH-07 (design para o usuário leigo)

Fonte: auditoria A48/dumb-user-audit-r1 (31 passos, 24 atritos, 22 reescritas).
Estado: LEVA-1 em execução; itens M/L aguardam slot.

## Leva 1 — esforço S (em execução)

| # | Item | Dono | Estado |
| --- | --- | --- | --- |
| 6 | Mensagens centrais em duas camadas (resumo→ação→detalhe recolhido) | A05 | 🔄 |
| 7 | pt-BR: GARFO/Switch/sandbox + 5 msgstr em inglês + varredura de glossário | A10 | 🔄 |
| 8 | Crash notice "Eden Encore" + 1 ação por kLaunchErrors | A05 | 🔄 (junto do 6) |
| 10 | Diagnóstico: 3 frases humanas + detalhes técnicos recolhidos | A05 | 🔄 (junto do 6) |

## Leva 2 — esforço M (próximas)

| # | Item | Nota |
| --- | --- | --- |
| 2 | Auto-organizador roms + subpastas | 🔄 já em dev (A05/spec SPECS) |
| 3 | Fim do ritual "reabra" (restart orcestrado in-process) | reusa reconstrução de idioma |
| 4 | Importar save escolhendo pasta na tela (mata title ID manual) | ryujinx_saves já faz cópia segura |
| 5 | Instalar mod escolhendo pasta na tela | make_mods_folder já existe |
| 1 | Wizard primeira execução (3 passos com estado ao vivo) | maior item M; por último da leva |

## Leva 3 — esforço L (futuro)

| # | Item |
| --- | --- |
| 9 | Verificador de integridade pós-download (página About S primeiro) |

## Proteções invariantes (nunca mudam)

Validação real de chaves/firmware; backup de save; no-symlink; sandbox
honesto; texto técnico sempre acessível; nunca logar chaves; nunca
"não verificado"→"ok".
