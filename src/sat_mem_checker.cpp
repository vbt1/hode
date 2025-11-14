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
extern Uint8 *hwram;
}
//#include "saturn_print.h"

void sat_free(void *ptr) {

	if(ptr == NULL || ptr == hwram)
		return;

//	emu_printf("FREE to NULL: addr: %p\n", ptr);
	ptr = NULL;

	return;
}
