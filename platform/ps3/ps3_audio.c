#ifdef PS3

#include <audio/audio.h>
#include <sys/thread.h>
#include <sys/systime.h>
#include <sys/event_queue.h>
#include <sys/mutex.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL.h>
#include "ps3_platform.h"

#define PS3_AUDIO_BLOCK_SAMPLES 256

static u32 g_audio_port = 0xFFFFFFFFu;
static audioPortConfig g_audio_cfg;
static sys_ppu_thread_t g_audio_tid = 0;
static sys_event_queue_t g_audio_eq = 0;
static sys_ipc_key_t g_audio_key = 0;
static volatile int g_audio_run = 0;
static volatile int g_audio_paused = 1;
static int g_audio_ready = 0;
static int g_audio_blocks = 0;
static int g_audio_write_block = 4;
static int g_audio_blocks_per_copy = 4;
static int16_t* g_audio_stage = NULL;
static int g_audio_stage_frames = 0;
static void (*g_audio_callback)(void*, Uint8*, int) = NULL;
static void* g_audio_userdata = NULL;
static sys_mutex_t g_audio_mutex = 0;
static int g_audio_mutex_ready = 0;

static void audio_thread_entry(void* arg) {
	(void)arg;
	if (!g_audio_cfg.audioDataStart) {
		sysThreadExit(0);
	}
	float* ring = (float*)(uintptr_t)g_audio_cfg.audioDataStart;

	while (g_audio_run) {
		sys_event_t ev;
		if (sysEventQueueReceive(g_audio_eq, &ev, 200000) != 0) continue;
		if (++g_audio_blocks >= g_audio_blocks_per_copy) {
			g_audio_blocks = 0;
			float* dst = ring + (size_t)g_audio_write_block * PS3_AUDIO_BLOCK_SAMPLES * 2;
			if (g_audio_paused || !g_audio_stage || !g_audio_callback) {
				memset(dst, 0, (size_t)g_audio_blocks_per_copy * PS3_AUDIO_BLOCK_SAMPLES * 2 * sizeof(float));
			} else {
				if (g_audio_mutex_ready) sysMutexLock(g_audio_mutex, 0);
				g_audio_callback(g_audio_userdata, (Uint8*)g_audio_stage, g_audio_stage_frames * 4);
				if (g_audio_mutex_ready) sysMutexUnlock(g_audio_mutex);
				const float scale = 1.0f / 32768.0f;
				int n = g_audio_stage_frames * 2;
				for (int i = 0; i < n; i++) dst[i] = (float)g_audio_stage[i] * scale;
			}
			g_audio_write_block = (g_audio_write_block + g_audio_blocks_per_copy) % AUDIO_BLOCK_8;
		}
	}
	sysThreadExit(0);
}

int ps3_audio_open(const SDL_AudioSpec* desired) {
	if (g_audio_ready) return 0;
	if (desired == NULL || desired->callback == NULL) return -1;

	if (audioInit() != 0) {
		ps3_log("audioInit failed\n");
		return -1;
	}

	audioPortParam param;
	memset(&param, 0, sizeof(param));
	param.numChannels = AUDIO_PORT_2CH;
	param.numBlocks = AUDIO_BLOCK_8;
	param.attrib = AUDIO_PORT_INITLEVEL;
	param.level = 1.0f;

	if (audioPortOpen(&param, &g_audio_port) != 0) {
		ps3_log("audioPortOpen failed\n");
		audioQuit();
		return -1;
	}

	if (audioGetPortConfig(g_audio_port, &g_audio_cfg) != 0) {
		ps3_log("audioGetPortConfig failed\n");
		audioPortClose(g_audio_port);
		g_audio_port = 0xFFFFFFFFu;
		audioQuit();
		return -1;
	}

	if (audioCreateNotifyEventQueue(&g_audio_eq, &g_audio_key) != 0) {
		ps3_log("audioCreateNotifyEventQueue failed\n");
		audioPortClose(g_audio_port);
		g_audio_port = 0xFFFFFFFFu;
		audioQuit();
		return -1;
	}

	if (audioSetNotifyEventQueue(g_audio_key) != 0) {
		ps3_log("audioSetNotifyEventQueue failed\n");
		audioRemoveNotifyEventQueue(g_audio_key);
		g_audio_key = 0;
		sysEventQueueDestroy(g_audio_eq, 0);
		g_audio_eq = 0;
		audioPortClose(g_audio_port);
		g_audio_port = 0xFFFFFFFFu;
		audioQuit();
		return -1;
	}

	u32 stage_size = (u32)(desired->samples * desired->channels * sizeof(int16_t));
	g_audio_blocks_per_copy = (int)(stage_size / (PS3_AUDIO_BLOCK_SAMPLES * 4));
	if (g_audio_blocks_per_copy < 1) g_audio_blocks_per_copy = 1;
	if (g_audio_blocks_per_copy > 8) g_audio_blocks_per_copy = 8;

	g_audio_stage_frames = (int)(stage_size / 4);
	g_audio_stage = (int16_t*)malloc((size_t)stage_size);
	if (!g_audio_stage) {
		audioRemoveNotifyEventQueue(g_audio_key);
		g_audio_key = 0;
		sysEventQueueDestroy(g_audio_eq, 0);
		g_audio_eq = 0;
		audioPortClose(g_audio_port);
		g_audio_port = 0xFFFFFFFFu;
		audioQuit();
		return -1;
	}
	memset(g_audio_stage, 0, stage_size);

	float* ring = (float*)(uintptr_t)g_audio_cfg.audioDataStart;
	if (ring) memset(ring, 0, AUDIO_BLOCK_8 * PS3_AUDIO_BLOCK_SAMPLES * 2 * sizeof(float));
	sysEventQueueDrain(g_audio_eq);

	if (!g_audio_mutex_ready) {
		sys_mutex_attr_t attr;
		sysMutexAttrInitialize(attr);
		attr.attr_protocol = SYS_MUTEX_PROTOCOL_FIFO;
		attr.attr_recursive = SYS_MUTEX_ATTR_RECURSIVE;
		if (sysMutexCreate(&g_audio_mutex, &attr) == 0) {
			g_audio_mutex_ready = 1;
		}
	}

	g_audio_callback = desired->callback;
	g_audio_userdata = desired->userdata;
	g_audio_blocks = 0;
	g_audio_write_block = 4 % AUDIO_BLOCK_8;
	g_audio_paused = 1;
	g_audio_run = 1;

	audioPortStart(g_audio_port);
	if (sysThreadCreate(&g_audio_tid, audio_thread_entry, NULL, 1500, 0x10000, THREAD_JOINABLE, "sdlpop_snd") != 0) {
		g_audio_run = 0;
		audioPortStop(g_audio_port);
		audioRemoveNotifyEventQueue(g_audio_key);
		g_audio_key = 0;
		sysEventQueueDestroy(g_audio_eq, 0);
		g_audio_eq = 0;
		audioPortClose(g_audio_port);
		g_audio_port = 0xFFFFFFFFu;
		audioQuit();
		free(g_audio_stage);
		g_audio_stage = NULL;
		return -1;
	}
	g_audio_ready = 1;
	ps3_log("ps3_audio_open: ready port=%u size=%u\n", (unsigned)g_audio_port, (unsigned)stage_size);
	return 0;
}

void ps3_audio_pause(int pause_on) {
	g_audio_paused = pause_on ? 1 : 0;
}

void ps3_audio_close(void) {
	if (!g_audio_ready) return;
	g_audio_run = 0;
	if (g_audio_port != 0xFFFFFFFFu) audioPortStop(g_audio_port);
	if (g_audio_tid) {
		u64 retval = 0;
		sysThreadJoin(g_audio_tid, &retval);
		g_audio_tid = 0;
	}
	if (g_audio_port != 0xFFFFFFFFu) {
		if (g_audio_key) {
			audioRemoveNotifyEventQueue(g_audio_key);
			g_audio_key = 0;
		}
		audioPortClose(g_audio_port);
		g_audio_port = 0xFFFFFFFFu;
	}
	if (g_audio_eq) {
		sysEventQueueDestroy(g_audio_eq, 0);
		g_audio_eq = 0;
	}
	audioQuit();
	if (g_audio_stage) {
		free(g_audio_stage);
		g_audio_stage = NULL;
	}
	if (g_audio_mutex_ready) {
		sysMutexDestroy(g_audio_mutex);
		g_audio_mutex = 0;
		g_audio_mutex_ready = 0;
	}
	g_audio_ready = 0;
}

void ps3_audio_lock(void) {
	if (g_audio_mutex_ready) sysMutexLock(g_audio_mutex, 0);
}

void ps3_audio_unlock(void) {
	if (g_audio_mutex_ready) sysMutexUnlock(g_audio_mutex);
}

#endif
