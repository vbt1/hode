#pragma GCC optimize ("Os")
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "stdlib.h"
//#include <cstdlib>
extern "C" {
#include "stdlib.h"
#include <sgl.h>
#include <sl_def.h>
//#include <sega_mem.h>
#include "sat_mem_checker.h"
void emu_printf(const char *format, ...);
Uint8 *hwram;
extern Uint8 *current_lwram;
extern Uint8 *cs1ram;
extern Uint8 *hwram_src;
Uint8 *vdp2ram = (Uint8 *)VDP2_VRAM_B0;
Uint8 *vdp1ram = (Uint8 *)SpriteVRAM+0x20;
Uint8 *cs2ram = (uint8_t *)0x22600000;
Uint8 *hwram_src;
Uint8 *hwram_work;
extern Uint8 *hwram_work_paf;
void *sbrk(intptr_t increment);
}

// Bump-allocate from a pointer, returning the old value
static inline uint8_t *bump(Uint8 **ptr, uint32_t size) {
    uint8_t *dst = *ptr;
//	emu_printf("bump %p\n", ptr);
    *ptr += SAT_ALIGN(size);

    emu_printf("hwram %d ptr %p lwram %d cs1 %p cs2 %p hw %p aft %p sz %d p %p\n",
            ((int)hwram_work) - 0x6000000, hwram_work,
            ((int)current_lwram) - 0x200000, cs1ram, cs2ram, hwram, ptr, size, sbrk(0));
    return dst;
}

uint8_t* allocate_memory(const uint8_t type, uint32_t alignedSize) 
{
//if(alignedSize>8000)
emu_printf("type %d size %d\n", type, alignedSize);
    switch (type) {	
    case TYPE_LDIMG:
    case TYPE_FONT:
        return bump(&vdp2ram, alignedSize);
  // plante à la fin des videos
    case TYPE_PAF:
//    case TYPE_PAFBUF:
//        return bump(&hwram_work_paf, alignedSize); // à remettre des que possible
        return bump(&hwram_work_paf, alignedSize); // à remettre des que possible
//        return bump(&current_lwram, alignedSize);

    case TYPE_MENU:
        return current_lwram; // no increment

    case TYPE_HWRAM:
        hwram_src = (Uint8 *)malloc(alignedSize);
        hwram     = hwram_src + alignedSize;
        return hwram_src;

    case TYPE_ANDY:
//		return bump (&hwram_work,2048*22);
		return bump(&cs1ram, alignedSize);
//       return bump(&hwram_work, alignedSize);

    case TYPE_BGLVL:
//        return bump(&current_lwram, alignedSize);
//        return current_lwram+2000;
       return bump(&cs1ram, alignedSize);
//       return (uint8_t*)0x270000;
//    case TYPE_MAP:
//    case TYPE_RES:
//    case TYPE_SHOOT:
    case TYPE_LAYER:
//    case TYPE_MSTCODE:
//    case TYPE_SHADWLUT:
		hwram_work += alignedSize; // vbt : ne pas utiliser bump !!!

		if ((int)hwram_work > (int)hwram) {
			emu_printf("ERROR: hwram_work overflow! Requested: %d bytes\n", alignedSize);
			return nullptr;
		}
		return hwram_work; 
  // retour des bugs sur sprites
    case TYPE_SPRITE1:
    case TYPE_MOVBOUND:
    	if(((int)current_lwram)+SAT_ALIGN(alignedSize)<0x300000)
		{
			return bump(&current_lwram, alignedSize);
		}
        return bump(&cs2ram, alignedSize);

    case TYPE_RES:
    case TYPE_PAFHEAD:
        if (((int)current_lwram) + SAT_ALIGN(alignedSize) < 0x300000)
            return bump(&current_lwram, alignedSize);
        return bump(&cs1ram, alignedSize);
    case TYPE_MONSTER1:
    case TYPE_MONSTER2:
    case TYPE_MSTAREA:
   case TYPE_MAP:
    case TYPE_SHOOT:
    case TYPE_MSTCODE:
//    case TYPE_GFSFILE:
    case TYPE_SCRMASK:
    case TYPE_SCRMASKBUF:
    case TYPE_BGLVLOBJ:
//    case TYPE_TASK:
//	case TYPE_SHADWBUF:
        if (((int)current_lwram) + SAT_ALIGN(alignedSize) < 0x300000)
		{
            return bump(&current_lwram, alignedSize);
		}
        return bump(&cs1ram, alignedSize);

    default:
    	emu_printf("missing case!!!\n");
        return nullptr;
    }
}
