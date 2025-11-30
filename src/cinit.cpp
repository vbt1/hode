//#define LINEAR_BITMAP 1
extern "C" {
#include 	<sl_def.h>
#include	<sega_sys.h>
#include	"gfs_wrap.h"
#include <stdarg.h>
#include <string.h>
void *memset4_fast(void *, long, size_t);
}

//#include 	"saturn_print.h"
#include 	"systemstub.h"
#include "sat_mem_checker.h"

extern Uint32 _bstart, _bend;
#define		SystemWork		0x060ffc00		/* System Variable Address */
#define		SystemSize		(0x06100000-0x060ffc00)		/* System Variable Size */

extern void ss_main( void );

#ifdef LINEAR_BITMAP
typedef Sint32           Fixed32;

#define	    toFIXED(a)		((FIXED)(65536.0 * (a)))
#define		IntToFixed(x)	(((Fixed32)(x)) << 16)

void BitmapCellScrTbl(Uint32 *tbl, Uint16 hRes)
{
	Sint32	i;

	for (i = 0; i < hRes/8; ++i)
		tbl[i] = i * 0x400;
}

void BitmapLineScrTbl(Uint32 *tbl, Uint16 hRes, Uint16 vRes, Sint16 hOff, Sint16 vOff)
{
	Sint32	i, hscroll, vscroll;

	for (i = 0; i < vOff*2; ++i)
		tbl[i] = 0;

	for (i = vOff*2, hscroll = -hOff, vscroll = 0; i < (vOff+vRes)*2; i += 2)
	{
		/*
		 *  Shift and mask the horizontal scroll value and stick it in the
		 *  line scroll table.  Give the vertical scroll value a fractional
		 *  component which will combine with the values in the vertical
		 *  cell scroll table so that the screen will scroll by one line
		 *  just as the VDP2 hits a 512-pixel boundary.
		 */
		tbl[i] = (hscroll << 16) & 0x7ffff00;
		tbl[i+1] = vscroll + hscroll/8 * 0x400;

		hscroll += hRes;

		if (hscroll >= 512)
		{
			hscroll -= 512;
			vscroll += IntToFixed(1);
		}
	}

	/*
	 *  Set up a transparency window to mask off the portion of the screen
	 *  that's outside the bitmap.
	 */
//	VDP2Window(VDP2_WINDOW_1, hOff*2, vOff, (hOff+hRes)*2 - 1, vOff+vRes-1);
//	VDP2WindowMode(VDP2_WINDOW_1, NBG0, VDP2_WINDOW_TRANS, VDP2_WINDOW_OUTSIDE);
}
#endif

int	main( void )
{
	Uint8	*dst;
	Uint32	i;

	/* 1.Zero Set .bss Section */
	for( dst = (Uint8 *)&_bstart; dst < (Uint8 *)&_bend; dst++ ) {
		*dst = 0;
	}
	/* 2.ROM has data at end of text; copy it. */

	/* 3.SGL System Variable Clear */
	for( dst = (Uint8 *)SystemWork, i = 0;i < SystemSize; i++) {
		*dst = 0;
	}

	init_GFS(); // Initialize GFS system

	slInitSystem(TV_640x224, (TEXTURE*)NULL, 1); // Init SGL
//	memset4_fast((void *)LOW_WORK_RAM_START,0x00,LOW_WORK_RAM_SIZE);

//	slBitMapNbg0(COL_TYPE_256, BM_512x512, (void*)VDP2_VRAM_A1);
	slBitMapNbg1(COL_TYPE_256, BM_512x512, (void*)VDP2_VRAM_A0); 
//	slScrTransparent(NBG0ON); // Do NOT elaborate transparency on NBG0 scroll
	slScrTransparent(NBG1ON); // Do NOT elaborate transparency on NBG1 scroll
	slZoomNbg1(26350, toFIXED(1.0));
//	slZoomNbg0(26350, toFIXED(1.0));
//	slScrPosNbg0(0, toFIXED(-16));
	slScrPosNbg1(0, toFIXED(-16));
//	slPriorityNbg0(5);
//	slPriorityNbg1(6);
//	slPrioritySpr0(7);
	slZdspLevel(7); // vbt : ne pas d?placer !!!

#ifdef LINEAR_BITMAP

Uint32			lineScrTbl[640*3];
Uint32			cellScrTbl[224/8];

//VDP2LineScroll(NBG1, LINE_SCROLL_TBL_ADDR, VDP2_LINE_SCROLL_HV_SCROLL);
BitmapLineScrTbl(lineScrTbl, 640, 224, 0, 0);
BitmapCellScrTbl(cellScrTbl, 224);
slLineScrollModeNbg1(lineHScroll|lineVScroll|VCellScroll);
slLineScrollTable1(lineScrTbl);
slVCellTable(cellScrTbl);
#endif
	slSynch();
//	DMA_ScuInit(); // Init for SCU DMA
	ss_main();
	return 0;
}

#define CS1(x)                  (0x24000000UL + (x))
extern void emu_printf(const char *format, ...);

#if 1
void emu_printf(const char *format, ...)
{
#if 1
   static char emu_printf_buffer[128];
   char *s = emu_printf_buffer;
   volatile uint8_t *addr = (volatile uint8_t *)CS1(0x1000);
   va_list args;

   va_start(args, format);
   (void)vsnprintf(emu_printf_buffer, 256, format, args);
   va_end(args);

   while (*s)
      *addr = (uint8_t)*s++;
#endif  
}
#endif