#pragma GCC optimize ("Os")
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

extern "C" {
void 	free(void *ptr);
void	*malloc(size_t);
void emu_printf(const char *format, ...);
extern uint8_t *hwram_work;
extern uint8_t *current_lwram;
}

extern "C"
void __cxa_pure_virtual(void) {
    /* Pure C++ virtual call; abort! */
    assert(false);
}

void* operator new(size_t size) {
	emu_printf("--- allocate2 %d\n", size);
	if(size==4148 || size==40096 || size==8)
	{
		
//		void *ptr = (void *)hwram_work;
//		hwram_work +=size;
		void *ptr = (void *)current_lwram;
		current_lwram +=size;
		return ptr;
	}
    return malloc(size);
}

void* operator new[](size_t size) {
	emu_printf("--- allocate %d\n", size);
	if(size==4148 || size==40096 || size==20168 || size == 60948)
	{
		
		void *ptr = (void *)hwram_work;
		hwram_work +=size;
		return ptr;
	}
    return malloc(size);	
}

void operator delete(void* ptr) {
	emu_printf("--- delete1 %p\n", ptr);
    free(ptr);
}

void operator delete[](void* ptr) {
	emu_printf("--- delete2 %p\n", ptr);
    free(ptr);
}

void operator delete[](void*, unsigned int) {
    /* Not yet implemented */
}

/*-
 * <https://en.cppreference.com/w/cpp/memory/new/operator_delete>
 *
 * Called if a user-defined replacement is provided, except that it's
 * unspecified whether other overloads or this overload is called when deleting
 * objects of incomplete type and arrays of non-class and trivially-destructible
 * class types.
 *
 * A memory allocator can use the given size to be more efficient */
void operator delete(void* ptr, unsigned int) {
    free(ptr);
}

extern "C"
void 
__global_ctors(void)
{
    extern void (*__ctors[])(void);

    /* Constructors are called in reverse order of the list */
    for (int32_t i = (int32_t)__ctors[0]; i >= 1; i--) {
        /* Each function handles one or more destructor (within file
         * scope) */
        __ctors[i]();
    }
}

extern "C"
void 
__global_dtors(void)
{
    extern void (*__dtors[])(void);

    /* Destructors in forward order */
    for (int32_t i = 0; i < (int32_t)__dtors[0]; i++) {
        /* Each function handles one or more destructor (within file
         * scope) */
        __dtors[i + 1]();
    }
}
