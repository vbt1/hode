#ifndef _SAT_MEM_CHECK_
#define _SAT_MEM_CHECK_

//#include <sega_mem.h>
#include <stdlib.h>

uint8_t* allocate_memory(const uint8_t type, uint32_t alignedSize);

/*
void *sat_calloc(size_t nmemb, size_t size);
void *sat_malloc(size_t size);
void sat_free(void *ptr);
void *std_calloc(size_t nmemb, size_t size);
void *std_malloc(size_t size);

void *sat_realloc(void *ptr, size_t size);
char *sat_strdup(const char *s);
*/

void memcpyl(void *, void *, int);
void memcpyw(void *, void *, int);
void *memset4_fast(void *, long, size_t);
void *malloc(size_t);
#define SAT_ALIGN(a) ((a+3)&~3)
#define VBT_L_START    ((volatile void *)(0x200000))

	enum ResType {
		TYPE_HWRAM = 0,
		TYPE_GFSFILE,
		TYPE_LAYER,
		TYPE_LDIMG,
		TYPE_FONT,
		TYPE_MENU,   //5
		TYPE_ANDY,
		TYPE_SPRITE1,
		TYPE_SCRMASK,
		TYPE_SCRMASKBUF,
		TYPE_SHADSCRMASKBUF,		 //10
		TYPE_BGLVL,
		TYPE_BGLVLOBJ,  
		TYPE_SHADWBUF,
		TYPE_SHADWLUT,
		TYPE_MONSTER1,	 //15
		TYPE_MONSTER2,
		TYPE_TASK,
		TYPE_MSTAREA,
		TYPE_MAP,       
		TYPE_MOVBOUND,	//20
		TYPE_SHOOT,
		TYPE_MSTCODE,
		TYPE_PAFHEAD,
		TYPE_PAF,       
		TYPE_PAFBUF,	//25
		TYPE_RES,
		TYPE_ANDY1
	};


#endif
