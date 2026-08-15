#pragma GCC optimize ("Os")
#include <string.h>
#include <stdio.h>
#include "stdlib.h"
#define DEBUG 1

extern "C" {
#include <sgl.h>
#include <sl_def.h>
#include "sat_mem_checker.h"
void emu_printf(const char *format, ...);
//void *sbrk(intptr_t increment);

Uint8 *hwram;
Uint8 *hwram_src;
Uint8 *hwram_work;
Uint8 *vdp2ram = (Uint8 *)VDP2_VRAM_B0 + 256;
//Uint8 *vdp1ram = (Uint8 *)SpriteVRAM + 0x20;
Uint8 *lwram_end = (Uint8 *)0x300000;
extern Uint8 *current_lwram;
extern Uint8 *hwram_work_paf;
extern Uint8 *_scrapBuffer;
Uint8 *cs1ram = (uint8_t *)0x22402000;
}

#ifdef DEBUG
  #define DPRINTF(...) emu_printf(__VA_ARGS__)
#else
  #define DPRINTF(...) ((void)0)
#endif

// noinline explicite : cette fonction est appelée depuis ~10 sites différents.
// Sans ça, -Os peut quand même choisir de l'inliner (elle est petite une fois
// le printf passé sous DPRINTF), ce qui dupliquerait le code d'alignement/bump
// à chaque site d'appel pour un gain de perf marginal.
__attribute__((noinline))
static uint8_t *bump(Uint8 **ptr, uint32_t size) {
    uint8_t *dst = (uint8_t *)SAT_ALIGN((int)*ptr);
    *ptr = dst + size;

    DPRINTF("hwram %d ptr %p lwram %d hw %p aft %p sz %d endhw %p\n",
            ((int)hwram_work) - 0x6000000, hwram_work,
            ((int)current_lwram) - 0x200000, hwram, ptr, size, lwram_end);
//			memset(hwram_work,0x00,10000);

    return dst;
}

uint8_t* allocate_memory(const uint8_t level, const uint8_t type, uint32_t alignedSize) 
{
	DPRINTF("level %d type %d size %d ", level, type, alignedSize);
//	if(alignedSize==0)
//		return (uint8_t*)0;
	if(level==255)
	{
		switch (type) {	
			case TYPE_HWRAM:
				hwram_src = (Uint8 *)malloc(alignedSize);
				hwram     = hwram_src + alignedSize;
				return hwram_src;
			case TYPE_LDIMG:
			case TYPE_FONT:
				return bump(&vdp2ram, alignedSize);
			case TYPE_RES:
			case TYPE_PAF:
			case TYPE_PAFBUF:
				return bump(&hwram_work_paf, alignedSize);
			case TYPE_PAFEND:
			if (alignedSize !=2096)
				return bump(&current_lwram, alignedSize);
				else
				return lwram_end - SAT_ALIGN(alignedSize);
//				return bump(&current_lwram, alignedSize);
//					return cs1ram;
//				return bump(&cs1ram, alignedSize);
//					return _scrapBuffer;
//					return bump(&hwram_work, alignedSize);
//				return bump(&hwram_work_paf, alignedSize);
			case TYPE_MENU:
				return current_lwram; // no increment
			case TYPE_LAYER:
				return bump(&hwram_work, alignedSize);
			case TYPE_PAFHEAD:
			case TYPE_MONSTER1:
			case TYPE_MONSTER2:
//emu_printf("lwram level %d type %d size %d\n", level, type, alignedSize);
//				return (uint8_t *)0x22400000;
				return bump(&current_lwram, alignedSize);

			default:
				DPRINTF("missing case!!! -1 %d\n", type);
				return nullptr;
		}
	}
	else if(level==0)
	{
		uint8_t *dst = (uint8_t *)0x210000;
		switch (type) {	
		case TYPE_BGLVL:
// vbt : si lwram > 103424 on ecrase des données
			if (dst + alignedSize > lwram_end)
				DPRINTF("ERROR33: %d overflow req:%d miss:%d\n", type, alignedSize,
						(int)lwram_end - (int)dst - alignedSize);
						
			return dst;
		case TYPE_ANDY1:
			lwram_end -= SAT_ALIGN(alignedSize);
			return lwram_end;

		case TYPE_SCRMASKBUF:
		case TYPE_ANDY:
//		case TYPE_RES:
		case TYPE_BGLVLOBJ: // pas lui
///		case TYPE_MAP: // coupable
//		case TYPE_MSTCODE:
//		case TYPE_MOVBOUND:
		case TYPE_MSTAREA: // pas lui
		{
			if (__builtin_expect((int)hwram_work + alignedSize > (int)hwram, 0)) {
				DPRINTF("ERROR1: %d overflow req:%d miss:%d\n", type, alignedSize,
						(int)hwram_work + alignedSize - (int)hwram);
				return nullptr;
			}
			return bump(&hwram_work, alignedSize);
//			return bump(&cs1ram, alignedSize);
//		case TYPE_MAP:
//			return bump(&cs1ram, alignedSize);
		}

	//    case TYPE_SHADWLUT:// plus utilisé
	//    case TYPE_SPRITE1: // plus utilisé
//		case TYPE_RES:
		case TYPE_MONSTER1:
		case TYPE_MONSTER2:
//		case TYPE_MSTAREA:
		case TYPE_MSTCODE:
		case TYPE_MOVBOUND:
		case TYPE_SHOOT:
	//    case TYPE_GFSFILE:
		case TYPE_SCRMASK:
//		case TYPE_BGLVLOBJ:
	//    case TYPE_TASK:// plus utilisé
	//	case TYPE_SHADWBUF:// plus utilisé
			if (__builtin_expect(((int)current_lwram) + SAT_ALIGN(alignedSize) < (int)lwram_end, 1))
			{
	//    DPRINTF("hwram %d ptr %p lwram %d cs1 %p cs2 %p hw %p aft %p sz %d p %p\n",
	//            ((int)hwram_work) - 0x6000000, hwram_work,
	//            ((int)current_lwram) - 0x200000, cs1ram, cs2ram, hwram, current_lwram, alignedSize, sbrk(0));
//emu_printf("lwram level %d type %d size %d\n", level, type, alignedSize);
				return bump(&current_lwram, alignedSize);
			}
			else
				DPRINTF("ERROR2: %d overflow req:%d miss:%d\n", type, alignedSize,
						(int)hwram_work + alignedSize - (int)hwram);

	//        DPRINTF("lwram %d %p lwram %d cs1 %d\n",
	//            ((int)hwram_work) - 0x6000000, hwram_src,
	//            ((int)current_lwram) - 0x200000, ((int)cs1ram) - 0x22400000);
			return nullptr;

		default:
			DPRINTF("missing case!!!\n");
			return nullptr;
		}
	}
#if 0
	else
	{
//	emu_printf("level %d type %d size %d\n", level, type, alignedSize);
return bump(&cs1ram, alignedSize);

		switch (type) {	
		case TYPE_BGLVL:
//			return (uint8_t*)0x21B400;
			return bump(&cs1ram, alignedSize);
		case TYPE_ANDY1:
			return bump(&cs1ram, alignedSize);
//			lwram_end -= SAT_ALIGN(alignedSize);
//			return lwram_end;			
		case TYPE_SCRMASKBUF:
//		case TYPE_ANDY:
		case TYPE_BGLVLOBJ:
		{
			if ((int)hwram_work + alignedSize > (int)hwram) {
				emu_printf("ERROR1: %d overflow req:%d miss:%d\n", type, alignedSize,
						(int)hwram_work + alignedSize - (int)hwram);
				return nullptr;
			}
			return bump(&hwram_work, alignedSize);
		}
		case TYPE_MOVBOUND:
		case TYPE_RES:
		case TYPE_MONSTER1:
		case TYPE_MONSTER2:
		case TYPE_MSTAREA:
		case TYPE_MAP:
		case TYPE_SHOOT:
		case TYPE_MSTCODE:
		case TYPE_SCRMASK:
			if (((int)current_lwram) + SAT_ALIGN(alignedSize) < 0x300000)
				return bump(&current_lwram, alignedSize);
			else
				emu_printf("ERROR3: %d overflow req:%d miss:%d\n", type, alignedSize,
						(int)hwram_work + alignedSize - (int)hwram);
			return nullptr;		
		default:
			emu_printf("missing case!!! %d\n", type);
			return bump(&cs1ram, alignedSize);
		}
	}
#endif
}
