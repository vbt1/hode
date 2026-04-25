#ifdef assert
  #undef assert
#endif
#define assert(x) do { if (!(x)) __builtin_trap(); } while(0)
	
