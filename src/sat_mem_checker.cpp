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
Uint8 *vdp1ram = (Uint8 *)SpriteVRAM+0x20;
}
Uint8 *hwram;
int vbt=0;

uint8_t* allocate_memory(const uint8_t type, uint32_t alignedSize) 
{
//	//emu_printf("allocate_memory type %d size %d - ", type, alignedSize);
    uint8_t* dst;
	
	if( type == TYPE_LDIMG || type == TYPE_FONT)
	{
////emu_printf("TYPE_LDIMG or font %p\n", dst);
		dst = vdp2ram;
		vdp2ram += SAT_ALIGN(alignedSize);
	}
	
// vbt pas besoin d'allouer de la ram
	if( type == TYPE_MENU)
	{
		dst = current_lwram;
	}

	if(type == TYPE_LAYER || type == TYPE_BGLVLOBJ 
	|| type == TYPE_SHADWBUF || type == TYPE_SHADWLUT
	|| type == TYPE_SCRMASKBUF || type == TYPE_SCRMASK) // jamais libéré 6762
	{
////emu_printf("malloc %d type %d\n", alignedSize, type);
		dst = (Uint8 *)malloc(alignedSize);
		hwram = dst+alignedSize;
	}
	
	if(type == TYPE_BGLVL) // toujours moins de 500ko?
	{
//		if(((int)current_lwram)+SAT_ALIGN(alignedSize)<0x300000)
//		{
//		dst = current_lwram; // vbt : on dirait qu'il ne faut pas incrémenter
		dst = cs1ram; // vbt : on dirait qu'il ne faut pas incrémenter
//		dst = vdp1ram; // vbt : on dirait qu'il ne faut pas incrémenter
//		memset(dst,0x00, SAT_ALIGN(alignedSize));
// vbt à recommenter lorsque  pb au moment de mourir est résolu		
//		current_lwram += SAT_ALIGN(alignedSize);
		cs1ram += SAT_ALIGN(alignedSize);
//		vdp1ram += SAT_ALIGN(alignedSize);
//		}
//		else
//		{
//		dst = vdp1ram;
//		vdp1ram += SAT_ALIGN(alignedSize);
//		}
vbt++;
//emu_printf("TYPE_BGLVL %p nb calls %d\n", dst, vbt);
emu_printf("hwram used %d lwram used %d cs1 used %d\n", ((int)hwram)-0x6000000, ((int)current_lwram)-0x200000, ((int)cs1ram)-0x22400000);
	}

	if(type == TYPE_SPRITE || type == TYPE_MONSTER || type == TYPE_MSTAREA || type == TYPE_MAP 
	|| type == TYPE_MOVBOUND || type == TYPE_SHOOT || type == TYPE_MSTCODE 	|| type == TYPE_PAF
	|| type == TYPE_PAFHEAD || type == TYPE_GFSFILE)
	{
		if(((int)current_lwram)+SAT_ALIGN(alignedSize)<0x300000)
		{
			dst = current_lwram;
			current_lwram += SAT_ALIGN(alignedSize);
		}
		else
		{
// //emu_printf("no more ram\n");
//			dst = cs1ram;
//			cs1ram += SAT_ALIGN(alignedSize);
		}		
	}

//	//emu_printf("addr %p next %p\n", dst, dst+SAT_ALIGN(alignedSize));
	
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
