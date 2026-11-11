// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <condition_variable>
#include <chrono>
#include <deque>
#include <map>
#include <span>
#include <mutex>
#include <vector>
#include "ps5_pad.hpp"
namespace Eden::Mock {
struct State {
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<ps5::pad::Data> pad_samples;
    int user_init_result = 0, user_result = 0, pad_open_result = 7, read_result = 0;
    int foreground_user = 84, last_pad_user = -1;
    int user_terminations = 0, pad_closes = 0, pad_opens = 0;
    // Empty overrides preserve the single-foreground-user fixture defaults.
    std::vector<int> login_users;
    std::map<int, int> pad_open_results, pad_read_results, pad_reads;
    int pad_get_handle_result = -1;
    std::vector<int> pad_open_users, pad_get_handle_users, pad_closed_handles;
    std::vector<int> vibration_handles, vibration_mode_handles, motion_handles;
    std::vector<ps5::pad::Vibration> vibrations;
    bool fail_audio_open = false, fail_volume = false, fail_output = false;
    int audio_opens = 0, audio_closes = 0, audio_drains = 0;
    std::map<int, std::vector<std::array<std::int16_t, 512>>> audio;
    bool guest_input = false;
    std::chrono::steady_clock::time_point guest_start;
};
extern State state;
void BeginGuestCycle();
void CheckGuestCycle();
std::array<std::int16_t, 960> HighRendererReference();
bool RendererPcmMatches(std::span<const std::int16_t> pcm, bool high = false);
}
