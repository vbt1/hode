/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */

#undef sleep 

#ifndef __SYSTEMSTUB_H__
#define __SYSTEMSTUB_H__

#include "intern.h"
#include "system.h"
#undef sleep 

extern "C" {
void SCU_DMAWait(void);
void emu_printf(const char *format, ...);
}
#define	cgaddress	0x1000 //SpriteBufSize
//#define PCM_ADDR ((void*)0x25a20000)
//#define PCM_SIZE (4096L*8)

int  cdUnlock(void); // CD Drive unlocker, when loading game through PAR
/*
struct PlayerInput {
	enum {
		DIR_UP    = 1 << 0,
		DIR_DOWN  = 1 << 1,
		DIR_LEFT  = 1 << 2,
		DIR_RIGHT = 1 << 3
	};
	enum {
		DF_FASTMODE = 1 << 0,
		DF_DBLOCKS  = 1 << 1,
		DF_SETLIFE  = 1 << 2,
		DF_AUTOZOOM = 1 << 3
	};
	volatile uint8 dirMask;
	volatile bool enter;
	volatile bool space;
	volatile bool shift;
	volatile bool backspace;
	volatile bool escape;
	volatile bool ltrig;
	volatile bool rtrig;

	char lastChar;

	bool save;
	bool load;
	int stateSlot;

	bool inpRecord;
	bool inpReplay;

	bool mirrorMode;

	uint8 dbgMask;
	bool quit;
};
*/
struct System {
	PlayerInput inp, pad;
	virtual void init(const char *title, int w, int h) = 0;
	virtual void destroy() = 0;

	virtual void setScaler(const char *name, int multiplier) = 0;
	virtual void setGamma(float gamma) = 0;

	virtual void setPalette(const uint8_t *pal, int n, int depth) = 0;
	virtual void clearPalette() = 0;
	virtual void copyRect(int x, int y, int w, int h, const uint8_t *buf, int pitch) = 0;
	virtual void copyYuv(int w, int h, const uint8_t *y, int ypitch, const uint8_t *u, int upitch, const uint8_t *v, int vpitch) = 0;
	virtual void fillRect(int x, int y, int w, int h, uint8_t color) = 0;
	virtual void copyRectWidescreen(int w, int h, const uint8_t *buf, const uint8_t *pal) = 0;
	virtual void shakeScreen(int dx, int dy) = 0;
	virtual void updateScreen(bool drawWidescreen) = 0;

	virtual void processEvents() = 0;
	virtual void sleep(int duration) = 0;
	virtual uint32_t getTimeStamp() = 0;

	virtual void startAudio(AudioCallback callback) = 0;
	virtual void stopAudio() = 0;
	virtual void lockAudio() = 0;
	virtual void unlockAudio() = 0;
	virtual AudioCallback setAudioCallback(AudioCallback callback) = 0;
};

//extern SystemStub *SystemStub_SDL_create();

extern System *const g_system;
#endif // __SYSTEMSTUB_H__
