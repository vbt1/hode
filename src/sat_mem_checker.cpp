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
}

//int vbt=0;

uint8_t* allocate_memory(const uint8_t type, uint32_t alignedSize) 
{
	emu_printf("alloctype %d size %d \n", type, alignedSize);
    uint8_t* dst;
	
	if( type == TYPE_LDIMG || type == TYPE_FONT)
	{
//emu_printf("TYPE_LDIMG or font %p\n", dst);
		dst = vdp2ram;
		vdp2ram += SAT_ALIGN(alignedSize);
		return dst;
	}
	
	if(type == TYPE_PAF || type == TYPE_PAFBUF)
	{
emu_printf("0hwram %d hwrampaf %d lwram %d cs1 %d\n", ((int)hwram_work)-0x6000000,  ((int)hwram_work_paf)-0x6000000, ((int)current_lwram)-0x200000, ((int)cs1ram)-0x22400000);
		dst = hwram_work_paf;
		hwram_work_paf += SAT_ALIGN(alignedSize);
		return dst;
	}
	
// vbt pas besoin d'allouer de la ram
	if( type == TYPE_MENU)
	{
emu_printf("1hwram %d hwrampaf %d lwram %d cs1 %d\n", ((int)hwram_work)-0x6000000,  ((int)hwram_work_paf)-0x6000000, ((int)current_lwram)-0x200000, ((int)cs1ram)-0x22400000);
		dst = current_lwram;
		return dst;
	}

	if(type == TYPE_HWRAM)
	{
//emu_printf("malloc1 %d type %d\n", alignedSize, type);
		dst = (Uint8 *)malloc(alignedSize);
		hwram_src = dst;
		hwram = dst+alignedSize;
		return dst;
	}
/*
	if(type == TYPE_LAYER 
	|| type == TYPE_SHADWBUF || type == TYPE_SHADWLUT
	|| type == TYPE_SHADSCRMASKBUF) // jamais libéré 6762
	{
		dst = (Uint8 *)malloc(alignedSize);
		hwram = dst+alignedSize;
		
emu_printf("1hwram %d lwram %d cs1 %d\n", ((int)hwram_work)-0x6000000, ((int)current_lwram)-0x200000, ((int)cs1ram)-0x22400000);
		return dst;
	}
*/	
	if(type == TYPE_PAFHEAD)
	{
emu_printf("2hwram %d lwram %d cs1 %d\n", ((int)hwram_work)-0x6000000, ((int)current_lwram)-0x200000, ((int)cs1ram)-0x22400000);

//		dst = current_lwram;
//		current_lwram += SAT_ALIGN(alignedSize);
		dst = cs1ram;
		cs1ram += SAT_ALIGN(alignedSize);

		return dst;
//		dst = (Uint8 *)malloc(alignedSize);
//		hwram = dst+alignedSize;
//		dst = hwram+1024;
	}
	
	if(type == TYPE_BGLVL) // toujours moins de 500ko?
	{
//emu_printf("3hwram %d lwram %d cs1 %d\n", ((int)hwram_work)-0x6000000, ((int)current_lwram)-0x200000, ((int)cs1ram)-0x22400000);
//emu_printf("hwram ptr %p\n", hwram_work);
//		dst = current_lwram; // vbt : on dirait qu'il ne faut pas incrémenter
//		dst = cs1ram; // vbt : on dirait qu'il ne faut pas incrémenter
//		cs1ram += SAT_ALIGN(alignedSize);
		dst = (uint8_t *)0x22600000-SAT_ALIGN(alignedSize);
//emu_printf("cs1ram %d type %d\n", alignedSize, type);
/*
		if(((int)current_lwram)+SAT_ALIGN(alignedSize)<0x300000)
		{
		dst = current_lwram; // vbt : on dirait qu'il ne faut pas incrémenter
//		dst = cs1ram; // vbt : on dirait qu'il ne faut pas incrémenter
//		dst = vdp1ram; // vbt : on dirait qu'il ne faut pas incrémenter
//		memset(dst,0x00, SAT_ALIGN(alignedSize));
// vbt à recommenter lorsque  pb au moment de mourir est résolu		
		current_lwram += SAT_ALIGN(alignedSize);
//		cs1ram += SAT_ALIGN(alignedSize);
//		vdp1ram += SAT_ALIGN(alignedSize);
		}
		else
		{
			dst = hwram_work;
			hwram_work += alignedSize;
		}
*/
//emu_printf("TYPE_BGLVL %p size %d\n", dst, alignedSize);
		return dst;
	}

	if(type == TYPE_RES)
	{
/*		dst = hwram_work;
		hwram_work += alignedSize;
*/
		dst = current_lwram;
		current_lwram += SAT_ALIGN(alignedSize);
		return dst;
	}

	if(type == TYPE_SPRITE || type == TYPE_MOVBOUND)
	{
emu_printf("4hwram %d %p lwram %d cs1 %d\n", ((int)hwram_work)-0x6000000, hwram_src, ((int)current_lwram)-0x200000, ((int)cs1ram)-0x22400000);

		if(alignedSize<170000 && ((int)hwram+alignedSize)<0x60f0000)
		{
			dst = hwram_work;
			hwram_work += SAT_ALIGN(alignedSize);
		}
		else
		{
/*			dst = current_lwram;
			current_lwram += SAT_ALIGN(alignedSize);
*/
emu_printf("cs2ram used!!!\n");
		dst = cs2ram;
		cs2ram += SAT_ALIGN(alignedSize);			
		}
		return dst;		
	}

	if(type == TYPE_SPRITE || type == TYPE_MONSTER1 || type == TYPE_MONSTER2 || type == TYPE_MSTAREA || type == TYPE_MAP 
	|| type == TYPE_SHOOT || type == TYPE_MSTCODE 
	|| type == TYPE_GFSFILE || type == TYPE_SCRMASK || type == TYPE_SCRMASKBUF
	|| type == TYPE_BGLVLOBJ || type == TYPE_TASK)
	{
		if(((int)current_lwram)+SAT_ALIGN(alignedSize)<0x300000)
		{
//emu_printf("lwram bglvl current_lwram %x\n", ((int)current_lwram)+SAT_ALIGN(alignedSize));
			dst = current_lwram;
			current_lwram += SAT_ALIGN(alignedSize);
emu_printf("4hwram %d %p lwram %d cs1 %d\n", ((int)hwram_work)-0x6000000, hwram_src, ((int)current_lwram)-0x200000, ((int)cs1ram)-0x22400000);

			return dst;
		}
		else
		{
//emu_printf("'bhwram %d %p lwram %d cs1 %d\n", ((int)hwram_work)-0x6000000, hwram_src, ((int)current_lwram)-0x200000, ((int)cs1ram)-0x22400000);

emu_printf("no more ram %d over %d\n", alignedSize, ((int)current_lwram)+SAT_ALIGN(alignedSize));
			dst = cs2ram;
			cs2ram += SAT_ALIGN(alignedSize);
			return dst;
//		dst = (Uint8 *)malloc(alignedSize);
//		hwram = dst+alignedSize;
		}
	}



//emu_printf("addr %p next %p\n", dst, dst+SAT_ALIGN(alignedSize));
//emu_printf("5hwram %d %p lwram %d cs1 %d\n", ((int)hwram_work)-0x6000000, hwram_src, ((int)current_lwram)-0x200000, ((int)cs1ram)-0x22400000);
	
	return dst;
}
//#include "saturn_print.h"
/*
void sat_free(void *ptr) {

	if(ptr == NULL || ptr == hwram)
		return;

//	//emu_printf("FREE to NULL: addr: %p\n", ptr);
	ptr = NULL;

	return;
}
*/
