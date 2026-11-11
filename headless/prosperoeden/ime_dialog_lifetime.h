// SPDX-License-Identifier: GPL-3.0-or-later
// Lifetime de um diálogo IME do aplicativo: aquisição única, liberação em todo
// caminho (incluindo throw), liberação idempotente e cancelamento que nunca
// publica aceite. Camada de LIFETIME apenas: nenhum símbolo, estrutura,
// protótipo ou constante Sony é declarado ou assumido aqui. A ABI real não
// está comprovada em código permitido; ver
// docs/feature-roadmap/campaign-20261107/agents/A07.md (P07, bloqueios 2–3).
//
// Semânticas espelham a referência lida (S headless/system_keyboard.cpp):
//   acquire/recusa sem aquisição      : S:129–143 (open falha => nada a liberar)
//   retry de liberação com pausa      : S:194–199 (100 tentativas, 50 ms)
//   aceite suprimido sem liberação    : S:200–205 (answer zerada)
//   retenção bloqueia nova abertura   : S:118–127 (storage.active)
//   cancelamento nunca publica aceite : S:173–177,207
// Padrões RAII do projeto: PendingPage (textures.cpp:69–73) e Descriptor
// (user_content.h:41–47).
#pragma once

namespace Eden::Ime {

// Estado do diálogo observável pelo dono (poll; o provedor não chama de volta).
enum class DialogState { none, running, finished };

// Contrato documentável de um provedor de diálogo. O integrador adapta cada
// campo ao contrato IME VERIFICADO da plataforma alvo antes de qualquer uso em
// produção; este arquivo não valida essa adaptação.
struct DialogOps {
    void* context = nullptr;
    // Pede a abertura. false: nada foi adquirido e nenhuma liberação é devida.
    // Não lança.
    bool (*open)(void* context);
    // Estado atual do diálogo aberto. Não lança.
    DialogState (*state)(void* context);
    // Encerramento cooperativo: o diálogo só some de fato após release()
    // confirmar. Opcional (null): sem pedido explícito, release() decide.
    // Não lança.
    void (*request_cancel)(void* context);
    // Entrega o resultado após finished. false: sem resultado válido.
    // accepted=false significa fechado sem texto. Não lança.
    bool (*take_result)(void* context, bool& accepted);
    // Pede a liberação definitiva. true somente quando o provedor confirma
    // que não usa mais memória do aplicativo (buffers de texto incluídos).
    // Deve aceitar chamadas repetidas. Não lança.
    bool (*release)(void* context);
    // Pausa entre tentativas de liberação (produção: dormir). Opcional (null):
    // as tentativas seguem imediatas. Não lança.
    void (*pause)(void* context);
};

// Uma posse de diálogo por instância. Não copiável; sem move (o dono vive no
// escopo que abre o diálogo, como PendingPage).
class DialogGuard {
public:
    DialogGuard() = default;
    ~DialogGuard() {
        if (holding_ && !release()) {
            // Liberação não confirmada: o provedor retém o diálogo e os
            // buffers que ele ainda usa. Não lança do destruidor; a decisão de
            // reter (não limpar) os buffers pertence ao storage do aplicativo,
            // que consulta holding().
        }
    }
    DialogGuard(const DialogGuard&) = delete;
    DialogGuard& operator=(const DialogGuard&) = delete;

    // Abre o diálogo; sem throw. false quando o provedor recusa a abertura
    // (nenhuma liberação é devida) ou quando esta instância ainda retém um
    // diálogo não liberado: liberação pendente bloqueia nova abertura.
    bool acquire(const DialogOps& ops) {
        if (holding_) return false;
        if (!ops.open || !ops.state || !ops.take_result || !ops.release || !ops.open(ops.context))
            return false;
        ops_ = ops;
        holding_ = true;
        cancel_asked_ = false;
        return true;
    }

    // Cancelamento cooperativo; request_cancel é chamado no máximo uma vez.
    void cancel() {
        if (!holding_ || cancel_asked_) return;
        cancel_asked_ = true;
        if (ops_.request_cancel) ops_.request_cancel(ops_.context);
    }

    // Lê o resultado. Um pedido cancelado nunca é reportado como aceito
    // (nem consulta o resultado). false: sem resultado publicável.
    bool take_result(bool& accepted) {
        accepted = false;
        if (!holding_ || cancel_asked_) return false;
        if (ops_.state(ops_.context) != DialogState::finished) return false;
        bool ok = false;
        if (!ops_.take_result(ops_.context, ok)) return false;
        accepted = ok;
        return true;
    }

    // Tenta a liberação até o provedor confirmar ou esgotar kReleaseAttempts.
    // true somente com confirmação. Idempotente: sem posse, nada faz e retorna
    // true; após confirmação a instância pode adquirir de novo.
    bool release() {
        if (!holding_) return true;
        for (int attempt = 0; attempt < kReleaseAttempts; ++attempt) {
            if (ops_.release(ops_.context)) {
                holding_ = false;
                return true;
            }
            if (attempt + 1 < kReleaseAttempts && ops_.pause) ops_.pause(ops_.context);
        }
        return false;
    }

    // Fecha o ciclo: lê o resultado, tenta liberar e só entrega aceite com a
    // liberação CONFIRMADA — um aceite que o provedor ainda retém não é
    // resposta (S:200–205). Retorno false: cancelado, sem resultado ou sem
    // liberação; accepted vem sempre false nesses casos.
    bool finish(bool& accepted) {
        accepted = false;
        bool ok = false;
        const bool has_result = take_result(ok);
        if (!release()) return false;
        if (has_result) accepted = ok;
        return has_result;
    }

    bool holding() const { return holding_; }
    bool cancel_requested() const { return cancel_asked_; }

    // Tentativas de liberação por chamada de release(); a intenção da
    // referência é 100 com pausa de 50 ms (S:194–199). O custo total no
    // destruidor é controlado pelo pause fornecido pelo integrador.
    static constexpr int kReleaseAttempts = 100;

private:
    DialogOps ops_{};
    bool holding_ = false;
    bool cancel_asked_ = false;
};

}  // namespace Eden::Ime
