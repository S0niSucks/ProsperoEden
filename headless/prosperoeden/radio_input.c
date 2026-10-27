#include "radio_input.h"
#include "../pro_controller/pro_controller.h"
#include "../pro_controller/monotonic_clock.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define INPUT_QUEUE_SIZE 64U
#define PAD_SAMPLE_SIZE 120U
#define PAD_SAMPLE_CAPACITY 64
#define PAD_BUTTON_INTERCEPTED UINT32_C(0x80000000)
#define STICK_LOW 64U
#define STICK_HIGH 192U
#define STICK_REPEAT_DELAY_MS UINT64_C(350)
#define STICK_REPEAT_MS UINT64_C(110)
#define MAX_PLAYERS 4U
#define USER_SCAN_MS UINT64_C(500)
#define PAD_RETRY_INITIAL_MS UINT64_C(1000)
#define PAD_RETRY_MAX_MS UINT64_C(30000)
#define PAD_READ_ERRORS_TO_REOPEN 3U

typedef struct {
    uint32_t button;
    radio_input_key_t key;
} button_map_t;

/* Signed-in users; unused entries are -1. */
typedef struct {
    int32_t user_id[4];
} login_user_list_t;

extern int scePadInit(void);
extern int scePadOpen(int32_t user_id, int32_t port_type, int32_t index,
                      const void * param);
extern int scePadClose(int32_t handle);
typedef struct { uint8_t large_motor; uint8_t small_motor; } pad_vibration_t;
extern int scePadSetVibration(int32_t handle, const pad_vibration_t * vibration);
extern int scePadSetVibrationMode(int32_t handle, int32_t mode);
extern int scePadRead(int32_t handle, void * data, int32_t num);
extern int scePadReadState(int32_t handle, void * data);
extern int scePadGetHandle(int32_t user_id, int32_t port_type, int32_t index);
extern int sceUserServiceGetLoginUserIdList(login_user_list_t * list);
extern int sceUserServiceInitialize(void * init_params);
extern int sceUserServiceGetInitialUser(int32_t * user_id);
extern int sceUserServiceTerminate(void);

static const button_map_t buttons[] = {
    {UINT32_C(0x00004000), RADIO_INPUT_CROSS},
    {UINT32_C(0x00002000), RADIO_INPUT_CIRCLE},
    {UINT32_C(0x00008000), RADIO_INPUT_SQUARE},
    {UINT32_C(0x00001000), RADIO_INPUT_TRIANGLE},
    {UINT32_C(0x00000008), RADIO_INPUT_OPTIONS},
    {UINT32_C(0x00000400), RADIO_INPUT_L1},
    {UINT32_C(0x00000800), RADIO_INPUT_R1},
    {UINT32_C(0x00000010), RADIO_INPUT_UP},
    {UINT32_C(0x00000040), RADIO_INPUT_DOWN},
    {UINT32_C(0x00000080), RADIO_INPUT_LEFT},
    {UINT32_C(0x00000020), RADIO_INPUT_RIGHT},
};

static radio_input_event_t queue[INPUT_QUEUE_SIZE];
static unsigned char samples[PAD_SAMPLE_CAPACITY][PAD_SAMPLE_SIZE];
static unsigned queue_read;
static unsigned queue_write;
static uint32_t button_state;
static int analog_key = -1;
static uint64_t analog_repeat_at;
/* A held L1 or R1 repeats like the stick (the Game files browser pages with them). */
static int shoulder_key = -1;
static uint64_t shoulder_repeat_at;
/* So does a held D-pad direction: the last one pressed. */
static int dpad_key = -1;
static uint64_t dpad_repeat_at;
/* A vibration started by radio_input_pulse: when it ends (0: none runs). */
static uint64_t pulse_ends_at;
static bool pulse_mode_set;
#define PULSE_MILLISECONDS 350u
static bool owns_user_service;
/* Every signed-in user's controller, for the home screen's controller display. Player 1 (slot 0)
   drives the menu; the other slots only tell whether their controller is connected. */
static int32_t player_user[MAX_PLAYERS] = {-1, -1, -1, -1};
static int32_t player_handle[MAX_PLAYERS] = {-1, -1, -1, -1};
/* A user's reservation survives failed opens and invalid handles. Opening alone
   does not reset backoff: only a successful read can prove a recovered handle. */
static struct {
    uint64_t retry_at, retry_ms;
    unsigned errors;
} recovery[MAX_PLAYERS];
static unsigned connected_players;
static uint64_t user_scan_at;
static bool pro_usb_enabled;
static bool pro_usb_active;
static bool pro_usb_restart;
static uint64_t input_now_ms;
static void pulse_stop(void);

static bool update_clock(void)
{
    /* All timing within a poll/pulse uses one validated snapshot. */
    uint64_t now;
    if(!pe_pro_monotonic_ms(&now) || now < input_now_ms) return false;
    input_now_ms = now;
    return true;
}

static uint64_t deadline_after(uint64_t delay)
{
    return input_now_ms > UINT64_MAX - delay ? UINT64_MAX : input_now_ms + delay;
}

static bool is_direction(radio_input_key_t key)
{
    return key == RADIO_INPUT_UP || key == RADIO_INPUT_DOWN ||
        key == RADIO_INPUT_LEFT || key == RADIO_INPUT_RIGHT;
}

static int stick_direction(uint8_t x, uint8_t y)
{
    const int horizontal = (int)x - 128;
    const int vertical = (int)y - 128;
    const int horizontal_size = horizontal < 0 ? -horizontal : horizontal;
    const int vertical_size = vertical < 0 ? -vertical : vertical;
    if(horizontal_size < 64 && vertical_size < 64) return -1;
    if(horizontal_size > vertical_size)
        return x < STICK_LOW ? RADIO_INPUT_LEFT :
            x > STICK_HIGH ? RADIO_INPUT_RIGHT : -1;
    return y < STICK_LOW ? RADIO_INPUT_UP :
        y > STICK_HIGH ? RADIO_INPUT_DOWN : -1;
}

static void queue_push(radio_input_key_t key, bool pressed)
{
    const unsigned next = (queue_write + 1U) % INPUT_QUEUE_SIZE;
    if(next == queue_read) {
        /* ponytail: bounded input; discard the oldest event instead of allocating. */
        queue_read = (queue_read + 1U) % INPUT_QUEUE_SIZE;
    }
    queue[queue_write] = (radio_input_event_t){key, pressed};
    queue_write = next;
}

static void process_sample(const unsigned char * sample)
{
    uint32_t current;
    memcpy(&current, sample, sizeof(current));
    const bool neutral = sample[76] == 0 ||
        (current & PAD_BUTTON_INTERCEPTED) != 0;
    if(neutral) current = 0;

    const uint32_t changed = button_state ^ current;
    for(unsigned i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
        if((changed & buttons[i].button) != 0) {
            const bool down = (current & buttons[i].button) != 0;
            queue_push(buttons[i].key, down);
            if(buttons[i].key == RADIO_INPUT_L1 || buttons[i].key == RADIO_INPUT_R1) {
                if(down) {
                    shoulder_key = buttons[i].key;
                    shoulder_repeat_at = deadline_after(STICK_REPEAT_DELAY_MS);
                } else if(shoulder_key == (int)buttons[i].key) {
                    shoulder_key = -1;
                }
            }
            if(is_direction(buttons[i].key)) {
                if(down) {
                    dpad_key = buttons[i].key;
                    dpad_repeat_at = deadline_after(STICK_REPEAT_DELAY_MS);
                } else if(dpad_key == (int)buttons[i].key) {
                    dpad_key = -1;
                }
            }
        }
    }
    button_state = current;

    int current_analog = neutral ? -1 : stick_direction(sample[4], sample[5]);
    if(current_analog != analog_key) {
        if(analog_key >= 0) queue_push((radio_input_key_t)analog_key, false);
        analog_key = current_analog;
        if(analog_key >= 0) {
            queue_push((radio_input_key_t)analog_key, true);
            analog_repeat_at = deadline_after(STICK_REPEAT_DELAY_MS);
        }
    }
}

static void close_handle(unsigned player)
{
    if(player == 0) {
        if(!pro_usb_active) pulse_stop();
        pulse_mode_set = false;
    }
    if(player_handle[player] >= 0) scePadClose(player_handle[player]);
    player_handle[player] = -1;
    connected_players &= ~(1U << player);
}

static void close_player(unsigned player)
{
    close_handle(player);
    player_user[player] = -1;
    memset(&recovery[player], 0, sizeof(recovery[player]));
}

static void retry_later(unsigned player)
{
    uint64_t delay = recovery[player].retry_ms;
    if(delay == 0) delay = PAD_RETRY_INITIAL_MS;
    recovery[player].retry_at = deadline_after(delay);
    recovery[player].retry_ms = delay >= PAD_RETRY_MAX_MS / 2 ? PAD_RETRY_MAX_MS : delay * 2;
    recovery[player].errors = 0;
}

static void open_player(unsigned player)
{
    if(player_user[player] < 0 || player_handle[player] >= 0 ||
       input_now_ms < recovery[player].retry_at) return;
    int32_t handle = scePadOpen(player_user[player], 0, 0, NULL);
    /* Reuse a handle this process already holds for the reserved user. */
    if(handle < 0) handle = scePadGetHandle(player_user[player], 0, 0);
    if(handle < 0) { retry_later(player); return; }
    player_handle[player] = handle;
    recovery[player].errors = 0;
    if(player == 0) pulse_mode_set = false;
}

/* Players 2-4 follow the signed-in users: one leaves when their user signs out, and a new user
   takes the first free slot. */
static void scan_users(void)
{
    login_user_list_t list = {{-1, -1, -1, -1}};
    if(sceUserServiceGetLoginUserIdList(&list) < 0) return;
    for(unsigned player = 1; player < MAX_PLAYERS; ++player) {
        if(player_user[player] < 0) continue;
        bool signed_in = false;
        for(unsigned i = 0; i < 4; ++i) signed_in |= list.user_id[i] == player_user[player];
        if(!signed_in) close_player(player);
    }
    for(unsigned i = 0; i < 4; ++i) {
        const int32_t user = list.user_id[i];
        if(user < 0) continue;
        bool known = false;
        for(unsigned player = 0; player < MAX_PLAYERS; ++player)
            known |= player_user[player] == user;
        if(known) continue;
        for(unsigned player = 1; player < MAX_PLAYERS; ++player) {
            if(player_user[player] >= 0) continue;
            player_user[player] = user;
            memset(&recovery[player], 0, sizeof(recovery[player]));
            break;
        }
    }
}

static void open_players(void)
{
    if(input_now_ms >= user_scan_at) {
        user_scan_at = deadline_after(USER_SCAN_MS);
        scan_users();
    }
    for(unsigned player = 0; player < MAX_PLAYERS; ++player) open_player(player);
}

static bool poll_players(int primary_count)
{
    bool primary_connected = false;
    for(unsigned player = 0; player < MAX_PLAYERS; ++player) {
        unsigned char state[PAD_SAMPLE_SIZE];
        connected_players &= ~(1U << player);
        if(player_handle[player] < 0) continue;
        memset(state, 0, sizeof(state));
        const int result = scePadReadState(player_handle[player], state);
        if(result != 0 || (player == 0 && (primary_count < 0 || primary_count > PAD_SAMPLE_CAPACITY))) {
            if(++recovery[player].errors >= PAD_READ_ERRORS_TO_REOPEN) {
                close_handle(player);
                retry_later(player);
            }
            continue;
        }
        recovery[player].errors = 0;
        recovery[player].retry_ms = PAD_RETRY_INITIAL_MS;
        if(state[76] != 0) {
            connected_players |= 1U << player;
            if(player == 0) primary_connected = true;
        }
    }
    return primary_connected;
}

#ifdef EDEN_DEV_ROM_ID
void radio_input_development_sample(const void * sample)
{
    if(sample) process_sample(sample);
}
#endif

bool radio_input_init(void)
{
    input_now_ms = 0;
    if(!update_clock()) return false;
    pro_usb_enabled = pe_pro_usb_start();
    pro_usb_active = false;
    pro_usb_restart = false;
    const int user_init = sceUserServiceInitialize(NULL);
    owns_user_service = user_init == 0;

    int32_t user_id = -1;
    if(sceUserServiceGetInitialUser(&user_id) < 0 || user_id < 0 || scePadInit() < 0) {
        radio_input_shutdown();
        return false;
    }
    /* Reserve player 1 before opening, even if USB is the only current source.
       Login scans must never assign this user to a secondary slot. */
    player_user[0] = user_id;
    memset(&recovery[0], 0, sizeof(recovery[0]));
    open_player(0);
    if(player_handle[0] < 0 && !pro_usb_enabled) {
        radio_input_shutdown();
        return false;
    }
    queue_read = queue_write = 0;
    button_state = 0;
    analog_key = -1;
    analog_repeat_at = 0;
    shoulder_key = -1;
    dpad_key = -1;
    connected_players = 0;
    user_scan_at = 0;
    return true;
}

static void pulse_stop(void)
{
    const pad_vibration_t still = {0, 0};
    if(pulse_ends_at != 0) {
        if(pro_usb_active) pe_pro_usb_rumble(0, 0);
        else if(player_handle[0] >= 0) scePadSetVibration(player_handle[0], &still);
    }
    pulse_ends_at = 0;
}

static void clock_failed(void)
{
    /* No fabricated epoch or unsigned deadline subtraction. Stop feedback,
       release held keys, and retry USB ownership after the clock recovers. */
    pulse_stop();
    pe_pro_usb_stop();
    pro_usb_restart = pro_usb_enabled;
    pro_usb_active = false;
    unsigned char neutral[PAD_SAMPLE_SIZE];
    pe_pro_pad_sample(NULL, neutral);
    process_sample(neutral);
    connected_players = 0;
}

void radio_input_pulse(unsigned percent)
{
    if(!update_clock()) { clock_failed(); return; }
    if(pro_usb_active) {
        if(percent > 100) percent = 100;
        pe_pro_usb_rumble((uint8_t)(255u * percent / 100u), 0);
        pulse_ends_at = deadline_after(PULSE_MILLISECONDS);
        return;
    }
    if(player_handle[0] < 0 || percent == 0) return;
    if(percent > 100) percent = 100;
    /* The two-motor mode the games use (headless/pad.cpp): 2 is the compatible mode. */
    if(!pulse_mode_set) pulse_mode_set = scePadSetVibrationMode(player_handle[0], 2) == 0;
    /* Both motors, as a game's strong rumble drives them, scaled as the setting scales a game's. */
    const pad_vibration_t pulse = {(uint8_t)(255u * percent / 100u), (uint8_t)(160u * percent / 100u)};
    if(scePadSetVibration(player_handle[0], &pulse) == 0) pulse_ends_at = deadline_after(PULSE_MILLISECONDS);
}

void radio_input_poll(void)
{
    if(player_user[0] < 0) return;
    if(!update_clock()) { clock_failed(); return; }
    if(pro_usb_restart) {
        pro_usb_enabled = pe_pro_usb_start();
        pro_usb_restart = false;
    }
    open_players();
    pe_pro_state pro_state;
    const bool active = pe_pro_usb_poll(&pro_state);
    if(active != pro_usb_active) {
        unsigned char neutral[PAD_SAMPLE_SIZE];
        pe_pro_pad_sample(NULL, neutral);
        process_sample(neutral);
        pulse_stop();
        pro_usb_active = active;
    }
    if(pulse_ends_at != 0 && input_now_ms >= pulse_ends_at) {
        pulse_stop();
    }
    /* Drain the fallback queue even while USB owns input, never replay it later. */
    const int count = player_handle[0] >= 0 ? scePadRead(player_handle[0], samples, PAD_SAMPLE_CAPACITY) : 0;
    const bool primary_connected = poll_players(count);
    if(active) {
        unsigned char sample[PAD_SAMPLE_SIZE];
        pe_pro_pad_sample(&pro_state, sample);
        process_sample(sample);
    } else if(primary_connected) {
        for(int i = 0; i < count; ++i) process_sample(samples[i]);
    } else {
        pulse_stop();
        unsigned char neutral[PAD_SAMPLE_SIZE];
        pe_pro_pad_sample(NULL, neutral);
        process_sample(neutral);
    }
    if(active) connected_players |= 1U;
    if(analog_key >= 0 && input_now_ms >= analog_repeat_at) {
        queue_push((radio_input_key_t)analog_key, true);
        analog_repeat_at = deadline_after(STICK_REPEAT_MS);
    }
    if(shoulder_key >= 0 && input_now_ms >= shoulder_repeat_at) {
        queue_push((radio_input_key_t)shoulder_key, true);
        shoulder_repeat_at = deadline_after(STICK_REPEAT_MS);
    }
    if(dpad_key >= 0 && input_now_ms >= dpad_repeat_at) {
        queue_push((radio_input_key_t)dpad_key, true);
        dpad_repeat_at = deadline_after(STICK_REPEAT_MS);
    }
}

bool radio_input_next(radio_input_event_t * event)
{
    if(event == NULL || queue_read == queue_write) return false;
    *event = queue[queue_read];
    queue_read = (queue_read + 1U) % INPUT_QUEUE_SIZE;
    return true;
}

unsigned radio_input_controllers(void)
{
    return connected_players;
}

bool radio_input_pressed(radio_input_key_t key)
{
    if(key < 0 || key >= RADIO_INPUT_COUNT) return false;
    for(unsigned i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
        if(buttons[i].key == key) return (button_state & buttons[i].button) != 0;
    }
    return false;
}

void radio_input_shutdown(void)
{
    pulse_stop();
    pe_pro_usb_stop();
    pro_usb_enabled = pro_usb_active = pro_usb_restart = false;
    for(unsigned player = 0; player < MAX_PLAYERS; ++player) close_player(player);
    if(owns_user_service) {
        sceUserServiceTerminate();
        owns_user_service = false;
    }
    queue_read = queue_write = 0;
    button_state = 0;
    analog_key = -1;
    analog_repeat_at = 0;
    shoulder_key = -1;
    dpad_key = -1;
}
