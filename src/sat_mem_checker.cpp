#pragma GCC optimize ("Os")
#include <string.h>
#include <stdio.h>
#include "stdlib.h"

extern "C" {
#include <sgl.h>
#include <sl_def.h>
#include "sat_mem_checker.h"
void emu_printf(const char *format, ...);
void *sbrk(intptr_t increment);

Uint8 *hwram;
Uint8 *hwram_src;
Uint8 *hwram_work;
Uint8 *vdp2ram = (Uint8 *)VDP2_VRAM_B0 + 256;
//Uint8 *vdp1ram = (Uint8 *)SpriteVRAM + 0x20;
Uint8 *lwram_end = (Uint8 *)0x300000;
extern Uint8 *current_lwram;
extern Uint8 *hwram_work_paf;
}

#ifdef DEBUG
  #define DPRINTF(...) emu_printf(__VA_ARGS__)
#else
  #define DPRINTF(...) ((void)0)
#endif

static inline uint8_t *bump(Uint8 **ptr, uint32_t size) {
    uint8_t *dst = (uint8_t *)SAT_ALIGN((int)*ptr);
    *ptr = dst + size;

    emu_printf("hwram %d ptr %p lwram %d hw %p aft %p sz %d p %p\n",
            ((int)hwram_work) - 0x6000000, hwram_work,
            ((int)current_lwram) - 0x200000, hwram, ptr, size, sbrk(0));
			memset(hwram_work,0x00,10000);
    return dst;
}

uint8_t* allocate_memory(const uint8_t type, uint32_t alignedSize) 
{
//	if(alignedSize==0)
//		return (uint8_t*)0;

    switch (type) {	
    case TYPE_HWRAM:
        hwram_src = (Uint8 *)malloc(alignedSize);
        hwram     = hwram_src + alignedSize;
        return hwram_src;

    case TYPE_LDIMG:
    case TYPE_FONT:
        return bump(&vdp2ram, alignedSize);
    case TYPE_PAF:
    case TYPE_PAFBUF:
        return bump(&hwram_work_paf, alignedSize);

    case TYPE_MENU:
        return current_lwram; // no increment

    case TYPE_BGLVL:
		return (uint8_t*)0x219400;

    case TYPE_ANDY1:
        lwram_end -= SAT_ALIGN(alignedSize);
        return lwram_end;

    case TYPE_SCRMASKBUF:
    case TYPE_ANDY:
    case TYPE_LAYER:
    case TYPE_MSTCODE:
	{
        if ((int)hwram_work + alignedSize > (int)hwram) {
            emu_printf("ERROR: %d overflow req:%d miss:%d\n", type, alignedSize,
                    (int)hwram_work + alignedSize - (int)hwram);
            return nullptr;
        }
        return bump(&hwram_work, alignedSize);
    }

//    case TYPE_SHADWLUT:// plus utilisé
//    case TYPE_SPRITE1: // plus utilisé
    case TYPE_MOVBOUND:
    case TYPE_RES:
    case TYPE_PAFHEAD:
    case TYPE_MONSTER1:
    case TYPE_MONSTER2:
    case TYPE_MSTAREA:
    case TYPE_MAP:
    case TYPE_SHOOT:
//    case TYPE_MSTCODE:
//    case TYPE_GFSFILE:
    case TYPE_SCRMASK:
    case TYPE_BGLVLOBJ:
//    case TYPE_TASK:// plus utilisé
//	case TYPE_SHADWBUF:// plus utilisé
        if (((int)current_lwram) + SAT_ALIGN(alignedSize) < 0x300000)
		{
//    DPRINTF("hwram %d ptr %p lwram %d cs1 %p cs2 %p hw %p aft %p sz %d p %p\n",
//            ((int)hwram_work) - 0x6000000, hwram_work,
//            ((int)current_lwram) - 0x200000, cs1ram, cs2ram, hwram, current_lwram, alignedSize, sbrk(0));

            return bump(&current_lwram, alignedSize);
		}
//        DPRINTF("lwram %d %p lwram %d cs1 %d\n",
//            ((int)hwram_work) - 0x6000000, hwram_src,
//            ((int)current_lwram) - 0x200000, ((int)cs1ram) - 0x22400000);
        return nullptr;

    default:
    	emu_printf("missing case!!!\n");
        return nullptr;
    }
}
