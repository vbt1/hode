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
extern Uint8 *hwram;
extern Uint8 *current_lwram;
extern Uint8 *cs1ram;
Uint8 *vdp2ram = (Uint8 *)VDP2_VRAM_B1;
}

uint8_t* allocate_memory(const uint8_t type, uint32_t alignedSize) 
{
	emu_printf("allocate_memory type %d size %d\n", type, alignedSize);
    uint8_t* dst;
	
	if( type == TYPE_LDIMG || type == TYPE_FONT)
	{
		dst = vdp2ram;
		vdp2ram += SAT_ALIGN(alignedSize);
	}
	
// vbt pas besoin d'allouer de la ram
	if( type == TYPE_MENU1 || type == TYPE_MENU0)
	{
		dst = current_lwram;
	}

	if(type == TYPE_LAYER || type == TYPE_BGLVLOBJ 
	|| type == TYPE_SHADWBUF || type == TYPE_SHADWLUT
	|| type == TYPE_SCRMASKBUF || type == TYPE_GFSFILE)
	{
		dst = (Uint8 *)malloc(alignedSize);
	}
	
	if( type == TYPE_SCRMASK)
	{
		dst = current_lwram;
		current_lwram += SAT_ALIGN(alignedSize);
	}
	
	if(type == TYPE_BGLVL) // toujours moins de 500ko?
	{
		dst = cs1ram; // vbt : on dirait qu'il ne faut pas incrémenter
// vbt à recommenter lorsque  pb au moment de mourir est résolu		
		cs1ram += SAT_ALIGN(alignedSize);
	}

	if(type == TYPE_SPRITE || type == TYPE_MONSTER || type == TYPE_MSTAREA || type == TYPE_MAP 
	|| type == TYPE_MOVBOUND || type == TYPE_SHOOT || type == TYPE_MSTCODE)
	{
		if(((int)current_lwram)+SAT_ALIGN(alignedSize)<0x300000)
		{
			dst = current_lwram;
			current_lwram += SAT_ALIGN(alignedSize);
		}
		else
		{
			dst = cs1ram;
			cs1ram += SAT_ALIGN(alignedSize);
		}		
	}

	emu_printf("addr %p next %p\n", dst, dst+SAT_ALIGN(alignedSize));
	
	return dst;
}
//#include "saturn_print.h"
/*
void sat_free(void *ptr) {

	if(ptr == NULL || ptr == hwram)
		return;

//	emu_printf("FREE to NULL: addr: %p\n", ptr);
	ptr = NULL;

	return;
}
*/
