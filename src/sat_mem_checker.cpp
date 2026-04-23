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
Uint8 *vdp2ram = (Uint8 *)VDP2_VRAM_B0+1284;
Uint8 *vdp1ram = (Uint8 *)SpriteVRAM+0x20;
//Uint8 *cs2ram = (uint8_t *)0x22600000;
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

    emu_printf("hwram %d ptr %p lwram %d cs1 %p hw %p aft %p sz %d p %p\n",
            ((int)hwram_work) - 0x6000000, hwram_work,
            ((int)current_lwram) - 0x200000, cs1ram, hwram, ptr, size, sbrk(0));
			memset(hwram_work,0x00,10000);
    return dst;
}

uint8_t* allocate_memory(const uint8_t type, uint32_t alignedSize) 
{
	if(alignedSize==0)
		return (uint8_t*)0;
//if(alignedSize>8000)
emu_printf("type %d size %d\n", type, alignedSize);
    switch (type) {	
    case TYPE_LDIMG:
    case TYPE_FONT:
        return bump(&vdp2ram, alignedSize);
  // plante à la fin des videos
    case TYPE_PAF:
//		return bump(&cs1ram, alignedSize);
//        return bump(&hwram_work_paf, alignedSize);
    case TYPE_PAFBUF:
//      return bump(&hwram_work_paf, alignedSize); // à remettre des que possible
        return bump(&hwram_work_paf, alignedSize); // à remettre des que possible
//      return bump(&current_lwram, alignedSize);

    case TYPE_MENU:
        return current_lwram; // no increment

    case TYPE_HWRAM:
        hwram_src = (Uint8 *)malloc(alignedSize);
        hwram     = hwram_src + alignedSize;
        return hwram_src;

    case TYPE_BGLVL:
//        return bump(&current_lwram, alignedSize);
//       return bump(&cs1ram, alignedSize);
		return (uint8_t*)0x280000;

    case TYPE_ANDY:
	{

emu_printf("andy %d %p\n", alignedSize, hwram_work);
		uint8_t *dst = hwram_work;
//		hwram_work += 2048*22; //alignedSize; 
//		hwram_work += alignedSize; 
return bump(&cs1ram, alignedSize);
//		return dst;

//		return bump(&current_lwram, alignedSize);		
	}
	
    case TYPE_LAYER:
	{
		uint8_t *dst = hwram_work;
		hwram_work += alignedSize; // vbt : ne pas utiliser bump !!!

		if ((int)hwram_work > (int)hwram) {
			emu_printf("ERROR: hwram_work overflow! Requested: %d bytes\n", alignedSize);
			return nullptr;
		}
    emu_printf("--hwram %d ptr %p lwram %d cs1 %p hw %p aft %p sz %d p %p\n",
            ((int)hwram_work) - 0x6000000, hwram_work,
            ((int)current_lwram) - 0x200000, cs1ram, hwram, hwram_work, alignedSize, sbrk(0));
			memset(hwram_work,0x00,110000);
		return dst; 
	}	
//    case TYPE_ANDY:	
    case TYPE_SHADWLUT:
    case TYPE_SPRITE1:
    case TYPE_MOVBOUND:
/*
emu_printf("alignedSize %d hwr %p end %p\n",alignedSize, (int)hwram_work+alignedSize,(int)hwram);
		if((alignedSize>160000&&  alignedSize<170000) && ((int)hwram_work+alignedSize)<(int)hwram)
			return bump(&hwram_work, alignedSize);
emu_printf("test failed d\n", ((int)hwram_work+alignedSize)<(int)hwram); 	
*/
    	if(((int)current_lwram)+SAT_ALIGN(alignedSize)<0x300000)
		{
//    emu_printf("hwram %d ptr %p lwram %d cs1 %p hw %p aft %p sz %d p %p\n",
//            ((int)hwram_work) - 0x6000000, hwram_work,
//            ((int)current_lwram) - 0x200000, cs1ram, hwram, current_lwram, alignedSize, sbrk(0));
			
			return bump(&current_lwram, alignedSize);
		}
        return bump(&cs1ram, alignedSize);
/*
    case TYPE_RES:
    case TYPE_PAFHEAD:
        if (((int)current_lwram) + SAT_ALIGN(alignedSize) < 0x300000)
		{
//    emu_printf("hwram %d ptr %p lwram %d cs1 %p hw %p aft %p sz %d p %p\n",
            ((int)hwram_work) - 0x6000000, hwram_work,
            ((int)current_lwram) - 0x200000, cs1ram, hwram, ptr, size, sbrk(0));

            return bump(&current_lwram, alignedSize);
		}
        return bump(&cs1ram, alignedSize);
*/
    case TYPE_MAP:
	//if(type==TYPE_MAP)
	//emu_printf("TYPE_MAP\n");
    case TYPE_MONSTER1:
	//if(type==TYPE_MONSTER1)
	//emu_printf("TYPE_MONSTER1\n");
    case TYPE_SCRMASKBUF:
	//if(type==TYPE_SCRMASKBUF)
	//emu_printf("TYPE_SCRMASKBUF\n");
    case TYPE_RES:
	//if(type==TYPE_RES)
	//emu_printf("TYPE_RES\n");
    case TYPE_PAFHEAD:
	//if(type==TYPE_PAFHEAD)
	//emu_printf("TYPE_PAFHEAD\n");
    case TYPE_MONSTER2:
	//if(type==TYPE_MONSTER2)
	//emu_printf("TYPE_MONSTER2\n");
    case TYPE_MSTAREA:
	//if(type==TYPE_MSTAREA)
	//emu_printf("TYPE_MSTAREA\n");
    case TYPE_SHOOT:
	//if(type==TYPE_SHOOT)
	//emu_printf("TYPE_SHOOT\n");
    case TYPE_MSTCODE:
	//if(type==TYPE_MSTCODE)
	//emu_printf("TYPE_MSTCODE\n");
//    case TYPE_GFSFILE:
    case TYPE_SCRMASK:
	//if(type==TYPE_SCRMASK)
	//emu_printf("TYPE_SCRMASK\n");
    case TYPE_BGLVLOBJ:
	//if(type==TYPE_BGLVLOBJ)
	//emu_printf("TYPE_BGLVLOBJ\n");
//    case TYPE_TASK:
//	case TYPE_SHADWBUF:
        if (((int)current_lwram) + SAT_ALIGN(alignedSize) < 0x300000)
		{
//    emu_printf("hwram %d ptr %p lwram %d cs1 %p cs2 %p hw %p aft %p sz %d p %p\n",
//            ((int)hwram_work) - 0x6000000, hwram_work,
//            ((int)current_lwram) - 0x200000, cs1ram, cs2ram, hwram, current_lwram, alignedSize, sbrk(0));

            return bump(&current_lwram, alignedSize);
		}
//        emu_printf("lwram %d %p lwram %d cs1 %d\n",
//            ((int)hwram_work) - 0x6000000, hwram_src,
//            ((int)current_lwram) - 0x200000, ((int)cs1ram) - 0x22400000);
        return bump(&cs1ram, alignedSize);

    default:
    	emu_printf("missing case!!!\n");
        return nullptr;
    }
}
