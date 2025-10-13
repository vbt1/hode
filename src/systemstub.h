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
struct SystemStub : System {
	PlayerInput inp, pad;
    void init(const char *title, uint16 w, uint16 h);
    void destroy();
    void setPaletteEntry(uint16 i, const Color *c);
    void getPaletteEntry(uint16 i, Color *c);
    void setOverscanColor(uint8 i);
    void copyRect(int16 x, int16 y, uint16 w, uint16 h, const uint8 *buf, uint32 pitch);
    void updateScreen(int shakeOffset) ;
    void processEvents() ;
    void sleep(uint32 duration) ;
    uint32_t getTimeStamp() ;       // must be defined
    void initTimeStamp() ;
    uint32 getOutputSampleRate() ;
    void setup_input(void) ;        // must be defined
};

extern SystemStub *SystemStub_SDL_create();

#endif // __SYSTEMSTUB_H__
