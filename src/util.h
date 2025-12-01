/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef UTIL_H__
#define UTIL_H__

#include "intern.h"

enum {
	kDebug_GAME     = 1 << 0,
	kDebug_RESOURCE = 1 << 1,
	kDebug_ANDY     = 1 << 2,
	kDebug_SOUND    = 1 << 3,
	kDebug_PAF      = 1 << 4,
	kDebug_MONSTER  = 1 << 5,
	kDebug_SWITCHES = 1 << 6, // 'lar1' and 'lar2' levels
	kDebug_MENU     = 1 << 7
};

extern int g_debugMask;

void debug(int mask, const char *msg, ...);
void error(const char *msg, ...);
void warning(const char *msg, ...);

#ifdef NDEBUG
#define debug(x, ...)
#endif

#define SAT_ALIGN(a) ((a+3)&~3)
#define SAT_ALIGN8(a) ((a+15)&~15)

#define	    SpriteVRAM		0x25c00000
#define	cgaddress	0x1000 //SpriteBufSize
#define pal1 COL_256
#undef TEXDEF
#define TEXDEF(h,v,presize)		{h,v,(cgaddress+(((presize)*4)>>(pal1)))/8,(((h)&0x1f8)<<5 | (v))}
#define	    toFIXED2(a)		((FIXED)(65536.0 * (a)))

struct __attribute__((__packed__)) SAT_sprite {
    uint32_t cgaddr : 26;  // 32 bits
    uint16_t x_flip : 6; 
    uint16_t size : 14;  // 14 bits
//    int8_t color : 8;  // 8 bits
    int16_t x : 10;  // 10 bits (values between -512 and +511)
    int16_t y : 10;  // 10 bits (values between -512 and +511)

};

extern "C" {
void emu_printf(const char *format, ...);
void SCU_DMAWait(void);
void *memset4_fast(void *, long, size_t);
void memcpyl(void *, void *, int);
extern Uint8 *current_lwram;
extern Uint8 *save_current_lwram;
extern Uint8 *cs1ram;
extern Uint8 *save_cs1ram;
#include "sat_mem_checker.h"
}

#endif // UTIL_H__
