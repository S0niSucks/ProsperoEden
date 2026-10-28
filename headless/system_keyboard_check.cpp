// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise native parameter construction against the system dialog's password/type rule.
#define PS5_NATIVE
#include "system_keyboard.cpp"
#include <cassert>

static int expected_type;
static bool expected_password;
static bool termination_blocked;
static int opens, terms;
static int result_outcome;
static ImeParam system_param;
static std::atomic<bool>* cancel_during_poll;
extern "C" {
int sceSysmoduleLoadModule(std::uint16_t) { return 0; }
int sceCommonDialogInitialize() { return 0; }
int sceUserServiceGetForegroundUser(std::int32_t* user) { *user = 1; return 0; }
int sceImeDialogInit(const ImeParam* param, const void*) {
    ++opens;
    system_param = *param; // the system copies the parameter, but retains its buffer pointers
    assert(param->type == expected_type);
    assert(bool(param->option & 4) == expected_password);
    if ((param->option & 4) && param->type != 1 && param->type != 4)
        return static_cast<int>(0x80bc0030u);
    assert(param->max_text_length == 128);
    assert(param->input_text_buffer[0] == u't');
    return 0;
}
int sceImeDialogGetStatus() {
    if (cancel_during_poll) *cancel_during_poll = true;
    return 2;
}
int sceImeDialogGetResult(ImeResult* result) { result->outcome = result_outcome; return 0; }
int sceImeDialogTerm() {
    ++terms;
    // The system keeps these pointers until Term succeeds. Access after a caller has timed out
    // is the regression: the original stack/heap storage had already been destroyed.
    assert(system_param.input_text_buffer[0] == u't');
    assert(system_param.title[0] == u'T' && system_param.placeholder[0] == u'H');
    if (!termination_blocked) system_param = {};
    return termination_blocked ? -1 : 0;
}
int sceImeDialogAbort() { return 0; }
}

int main() {
    std::atomic<bool> stop{};
    for (bool password : {false, true}) {
        for (bool numbers : {false, true}) {
            Eden::TextRequest request;
            request.initial = u"test";
            request.title = u"Title";
            request.placeholder = u"Hint";
            request.max_length = 128;
            request.password = expected_password = password;
            request.numbers = numbers;
            expected_type = numbers ? 4 : password ? 1 : 0;
            const auto answer = Eden::AskSystemKeyboard(request, stop);
            assert(answer.outcome == Eden::TextOutcome::accepted);
            assert(answer.text == request.initial);
        }
    }
    Eden::TextRequest request;
    request.initial = u"test";
    request.title = u"Title";
    request.placeholder = u"Hint";
    request.max_length = 128;
    request.password = expected_password = true;
    expected_type = 1;
    termination_blocked = true;
    const int before_terms = terms;
    const auto timed_out = Eden::AskSystemKeyboard(request, stop);
    assert(system_param.input_text_buffer[0] == u't');
    assert(timed_out.outcome == Eden::TextOutcome::unavailable);
    assert(terms == before_terms + 100);
    const int before_opens = opens;
    assert(Eden::AskSystemKeyboard(request, stop).outcome == Eden::TextOutcome::unavailable);
    assert(opens == before_opens); // pending storage cannot be reused, even for another private prompt
    stop = true;
    assert(Eden::AskSystemKeyboard(request, stop).outcome == Eden::TextOutcome::cancelled);
    assert(opens == before_opens);
    stop = false;
    termination_blocked = false;
    assert(Eden::AskSystemKeyboard(request, stop).outcome == Eden::TextOutcome::accepted);
    assert(opens == before_opens + 1);
    assert(system_param.input_text_buffer == nullptr);
    cancel_during_poll = &stop;
    const auto cancelled = Eden::AskSystemKeyboard(request, stop);
    assert(cancelled.outcome == Eden::TextOutcome::cancelled && cancelled.text.empty());
    assert(system_param.input_text_buffer == nullptr);
    cancel_during_poll = nullptr;
    stop = false;
    result_outcome = 1;
    termination_blocked = true;
    const auto user_cancelled = Eden::AskSystemKeyboard(request, stop);
    assert(user_cancelled.outcome == Eden::TextOutcome::cancelled && user_cancelled.text.empty());
    assert(system_param.input_text_buffer[0] == u't');
    termination_blocked = false;
    result_outcome = 0;
    assert(Eden::AskSystemKeyboard(request, stop).outcome == Eden::TextOutcome::accepted);
}
