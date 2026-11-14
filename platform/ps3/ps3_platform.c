#ifdef PS3

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <dirent.h>

#include <SDL2/SDL.h>
#include <io/pad.h>
#include <sysutil/video.h>
#include <sysutil/sysutil.h>

#include "ps3_platform.h"

#define PS3_DEFAULT_APP_DIR "/dev_hdd0/game/SDLP30001/USRDIR"

static char app_dir[512] = PS3_DEFAULT_APP_DIR;
static FILE* log_file = NULL;
static int pad_inited = 0;
static int pad_has_data = 0;
static padData last_pad_data;
static volatile int ps3_exit_requested = 0;
static int ps3_exit_sent = 0;
static volatile int ps3_sound_ended_pending = 0;

static void boot_trace_write(const char* path, s32 flags, const char* msg) {
	s32 fd = -1;
	if (sysLv2FsOpen(path, flags, &fd, 0666, NULL, 0) == 0 && fd >= 0) {
		if (flags & SYS_O_CREAT) {
			sysLv2FsChmod(path, 0666);
		}
		u64 written = 0;
		sysLv2FsWrite(fd, msg, strlen(msg), &written);
		sysLv2FsClose(fd);
	}
}

/* Constructors removed: running file I/O before crt1's __syscalls_init/sbrk_init/init_metalock crashes/deadlocks. */

void ps3_boot_mark(const char* msg) {
	if (msg == NULL) return;
	char line[256];
	snprintf(line, sizeof(line), "%s\n", msg);
	boot_trace_write("/dev_hdd0/tmp/sdlpop_boot.log", SYS_O_WRONLY | SYS_O_CREAT | SYS_O_APPEND, line);
	boot_trace_write("/dev_usb000/sdlpop_boot.log", SYS_O_WRONLY | SYS_O_CREAT | SYS_O_APPEND, line);
	ps3_log("mark: %s", line);
}

static void ps3_sysutil_cb(u64 status, u64 param, void* usrdata) {
	(void)param;
	(void)usrdata;
	if (status == SYSUTIL_EXIT_GAME) {
		ps3_exit_requested = 1;
	}
}

void ps3_sysutil_register(void) {
	sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
	sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT1);
	int rc = sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, ps3_sysutil_cb, NULL);
	ps3_log("ps3_sysutil_register: rc=%d\n", rc);
}

void ps3_notify_sound_ended(void) {
	__sync_lock_test_and_set(&ps3_sound_ended_pending, 1);
}

int ps3_consume_sound_ended(void) {
	return __sync_lock_test_and_set(&ps3_sound_ended_pending, 0);
}

enum {
	VK_LEFT = 0,
	VK_RIGHT,
	VK_UP,
	VK_DOWN,
	VK_ACTION,
	VK_CONFIRM,
	VK_BACK,
	VK_CONFIRM_ALT,
	VK_QUICKSAVE,
	VK_QUICKLOAD,
	VK_SCREENSHOT,
	VK_VOLUP,
	VK_VOLDOWN,
	VK_COUNT
};

/* Diagnostic marker: written with raw stdio once main() is reached. */
void ps3_marker(const char* text) {
	static const char* paths[] = {
		"/dev_hdd0/tmp/sdlpop_marker.txt",
		"/dev_hdd0/game/SDLP30001/USRDIR/marker.txt",
		"/dev_usb000/sdlpop_marker.txt",
		"/dev_usb001/sdlpop_marker.txt",
		NULL
	};
	for (int i = 0; paths[i] != NULL; i++) {
		FILE* f = fopen(paths[i], "a");
		if (f != NULL) {
			fputs(text, f);
			fputc('\n', f);
			fclose(f);
		}
	}
}

static const SDL_Scancode vk_scancodes[VK_COUNT] = {
	SDL_SCANCODE_LEFT,
	SDL_SCANCODE_RIGHT,
	SDL_SCANCODE_UP,
	SDL_SCANCODE_DOWN,
	SDL_SCANCODE_LSHIFT,
	SDL_SCANCODE_RETURN,
	SDL_SCANCODE_ESCAPE,
	SDL_SCANCODE_SPACE,
	SDL_SCANCODE_F6,
	SDL_SCANCODE_F9,
	SDL_SCANCODE_F12,
	SDL_SCANCODE_KP_PLUS,
	SDL_SCANCODE_KP_MINUS
};

static int vk_state[VK_COUNT];

static int dir_has_data(const char* dir) {
	char path[512];
	struct stat st;
	DIR* d;
	snprintf(path, sizeof(path), "%s/data/TITLE", dir);
	d = opendir(path);
	if (d != NULL) {
		closedir(d);
		return 1;
	}
	if (stat(path, &st) == 0) return 1;
	snprintf(path, sizeof(path), "%s/data", dir);
	d = opendir(path);
	if (d != NULL) {
		closedir(d);
		return 1;
	}
	if (stat(path, &st) == 0) return 1;
	return 0;
}

const char* ps3_get_app_dir(void) {
	return app_dir;
}

void ps3_log(const char* fmt, ...) {
	va_list args;
	FILE* file = log_file != NULL ? log_file : stderr;
	va_start(args, fmt);
	vfprintf(file, fmt, args);
	va_end(args);
	fflush(file);
}

void ps3_platform_init(int argc, char* argv[]) {
	char base[512] = "";
	(void)argc;
	{
		static const char* log_paths[] = {
			"/dev_usb000/sdlpop_debug.log",
			"/dev_usb001/sdlpop_debug.log",
			"/dev_hdd0/game/SDLP30001/USRDIR/debug.log",
			"/dev_hdd0/tmp/sdlpop_debug.log",
			"debug.log",
			NULL
		};
		for (int i = 0; log_paths[i] != NULL; i++) {
			log_file = fopen(log_paths[i], "w");
			if (log_file != NULL) break;
		}
		if (log_file != NULL) {
			setvbuf(log_file, NULL, _IONBF, 0);
		}
	}
	ps3_log("boot: argc=%d\n", argc);
	ps3_log("boot: argv0=%s\n", (argv != NULL && argv[0] != NULL) ? argv[0] : "(null)");

	if (argv != NULL && argv[0] != NULL && argv[0][0] == '/') {
		snprintf(base, sizeof(base), "%s", argv[0]);
		char* slash = strrchr(base, '/');
		if (slash != NULL) {
			*slash = '\0';
			if (!dir_has_data(base)) base[0] = '\0';
		}
	}

	if (base[0] == '\0') {
		static const char* candidates[] = {
			"/dev_hdd0/game/SDLP30001/USRDIR",
			"/dev_hdd0/game/SDLP30002/USRDIR",
			"/dev_hdd0/game/SDLP30003/USRDIR",
			"/dev_usb000/SDLPoP",
			"/dev_usb001/SDLPoP",
			"/dev_usb000/prince",
			"/dev_usb001/prince",
			NULL
		};
		for (int i = 0; candidates[i] != NULL; i++) {
			if (dir_has_data(candidates[i])) {
				snprintf(base, sizeof(base), "%s", candidates[i]);
				break;
			}
		}
	}

	if (base[0] == '\0') snprintf(base, sizeof(base), "%s", PS3_DEFAULT_APP_DIR);

	int chdir_result = chdir(base);
	if (log_file == NULL) {
		log_file = fopen("debug.log", "w");
		if (log_file != NULL) {
			setvbuf(log_file, NULL, _IONBF, 0);
		}
	}
	ps3_log("ps3_platform_init: base=%s chdir=%d\n", base, chdir_result);
	if (chdir_result != 0) {
		printf("PS3: cannot enter %s, keeping current directory\n", base);
		ps3_log("ps3_platform_init: chdir failed\n");
	}
	snprintf(app_dir, sizeof(app_dir), "%s", base);
	ps3_log("ps3_platform_init: data=%d\n", dir_has_data("."));

	SDL_setenv("HOME", app_dir, 1);
	SDL_setenv("SDLPOP_SAVE_PATH", app_dir, 1);

	char sub_dir[512];
	snprintf(sub_dir, sizeof(sub_dir), "%s/screenshots", app_dir);
	mkdir(sub_dir, 0777);
	snprintf(sub_dir, sizeof(sub_dir), "%s/replays", app_dir);
	mkdir(sub_dir, 0777);

	ps3_boot_mark("platform_init_ok");
	ps3_sysutil_register();
}

void ps3_platform_shutdown(void) {
	ps3_audio_close();
	if (pad_inited) {
		ioPadEnd();
		pad_inited = 0;
	}
	sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
	if (log_file != NULL) {
		fflush(log_file);
		fclose(log_file);
		log_file = NULL;
	}
}

static void emit_key(int vk, int down) {
	if (vk < 0 || vk >= VK_COUNT) return;
	if (vk_state[vk] == down) return;
	vk_state[vk] = down;

	SDL_Event event;
	SDL_memset(&event, 0, sizeof(event));
	event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
	event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
	event.key.keysym.scancode = vk_scancodes[vk];
	event.key.keysym.sym = SDL_GetKeyFromScancode(vk_scancodes[vk]);
	event.key.keysym.mod = KMOD_NONE;
	event.key.repeat = 0;
	SDL_PushEvent(&event);
}

void ps3_poll_input(void) {
	sysUtilCheckCallback();
	if (ps3_exit_requested && !ps3_exit_sent) {
		ps3_exit_sent = 1;
		SDL_Event ev;
		SDL_memset(&ev, 0, sizeof(ev));
		ev.type = SDL_QUIT;
		SDL_PushEvent(&ev);
		return;
	}
	if (!SDL_WasInit(SDL_INIT_VIDEO)) return;

	if (!pad_inited) {
		if (ioPadInit(7) != 0) return;
		pad_inited = 1;
		pad_has_data = 0;
		memset(&last_pad_data, 0, sizeof(last_pad_data));
		last_pad_data.ANA_L_H = 128;
		last_pad_data.ANA_L_V = 128;
	}

	padInfo pinfo;
	if (ioPadGetInfo(&pinfo) != 0 || !pinfo.status[0]) {
		pad_has_data = 0;
		memset(&last_pad_data, 0, sizeof(last_pad_data));
		last_pad_data.ANA_L_H = 128;
		last_pad_data.ANA_L_V = 128;
		for (int i = 0; i < VK_COUNT; i++) emit_key(i, 0);
		return;
	}

	padData pdata;
	memset(&pdata, 0, sizeof(pdata));
	if (ioPadGetData(0, &pdata) == 0 && pdata.len > 0) {
		last_pad_data = pdata;
		pad_has_data = 1;
	}
	if (!pad_has_data) return;

	int left = last_pad_data.BTN_LEFT ? 1 : 0;
	int right = last_pad_data.BTN_RIGHT ? 1 : 0;
	int up = last_pad_data.BTN_UP ? 1 : 0;
	int down = last_pad_data.BTN_DOWN ? 1 : 0;

	int ax = (int)last_pad_data.ANA_L_H - 128;
	int ay = (int)last_pad_data.ANA_L_V - 128;
	const int deadzone = 45;

	if (abs(ax) > deadzone || abs(ay) > deadzone) {
		if (abs(ax) >= abs(ay)) {
			if (ax < -deadzone) left = 1;
			else if (ax > deadzone) right = 1;
		} else {
			if (ay < -deadzone) up = 1;
			else if (ay > deadzone) down = 1;
		}
	}

	emit_key(VK_LEFT, left);
	emit_key(VK_RIGHT, right);
	emit_key(VK_UP, up);
	emit_key(VK_DOWN, down);
	emit_key(VK_ACTION, last_pad_data.BTN_CROSS ? 1 : 0);
	emit_key(VK_CONFIRM, last_pad_data.BTN_SQUARE ? 1 : 0);
	emit_key(VK_CONFIRM_ALT, last_pad_data.BTN_TRIANGLE ? 1 : 0);
	emit_key(VK_BACK, (last_pad_data.BTN_SELECT || last_pad_data.BTN_START) ? 1 : 0);
	emit_key(VK_QUICKSAVE, last_pad_data.BTN_L1 ? 1 : 0);
	emit_key(VK_QUICKLOAD, last_pad_data.BTN_R1 ? 1 : 0);
	emit_key(VK_SCREENSHOT, last_pad_data.BTN_R3 ? 1 : 0);
	emit_key(VK_VOLUP, last_pad_data.BTN_R2 ? 1 : 0);
	emit_key(VK_VOLDOWN, last_pad_data.BTN_L2 ? 1 : 0);
}

float SDL_roundf(float x) {
	return roundf(x);
}

intmax_t strtoimax(const char* nptr, char** endptr, int base) {
	return (intmax_t) strtoll(nptr, endptr, base);
}

void ps3_sdl_render_cp(const char* what) {
	static int cp_count = 0;
	if (cp_count < 8) {
		++cp_count;
		char buf[128];
		snprintf(buf, sizeof(buf), "render_cp:%s", what != NULL ? what : "null");
		ps3_boot_mark(buf);
	}
}

void ps3_sdl_flip_timeout(unsigned int ms) {
	static int timeout_logged = 0;
	if (!timeout_logged) {
		timeout_logged = 1;
		char buf[64];
		snprintf(buf, sizeof(buf), "flip_timeout:%u", ms);
		ps3_boot_mark(buf);
	}
}

#endif
