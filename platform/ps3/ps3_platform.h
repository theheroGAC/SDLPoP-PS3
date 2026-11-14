#ifndef PS3_PLATFORM_H
#define PS3_PLATFORM_H

#ifdef PS3

#include <SDL2/SDL.h>

void ps3_platform_init(int argc, char* argv[]);
void ps3_platform_shutdown(void);
void ps3_sysutil_register(void);
void ps3_notify_sound_ended(void);
int ps3_consume_sound_ended(void);
void ps3_boot_mark(const char* msg);
void ps3_poll_input(void);
void ps3_log(const char* fmt, ...);
void ps3_marker(const char* text);
const char* ps3_get_app_dir(void);

int ps3_audio_open(const SDL_AudioSpec* desired);
void ps3_audio_pause(int pause_on);
void ps3_audio_close(void);
void ps3_audio_lock(void);
void ps3_audio_unlock(void);

#endif

#endif
