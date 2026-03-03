#pragma GCC optimize ("Os")
/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 * SuperH-2 optimizations
 */
#include "intern.h"
#include "util.h"

#if 0
extern "C" {
extern Uint8 *cs1ram;
}

enum {
	kCodeWidth = 9,
	kClearCode = 1 << (kCodeWidth - 1),
	kEndCode = kClearCode + 1,
	kNewCodes = kEndCode + 1,
	kStackSize = 8192,
	kMaxBits = 12
};

struct LzwDecoder {

//	uint16_t _prefix[1 << kMaxBits];
//	uint8_t _stack[kStackSize];
	uint16_t *_prefix;
	uint8_t *_stack;

	const uint8_t *_buf;
	uint32_t _currentBits;
	uint8_t _bitsLeft;
	
	inline uint32_t nextCode(int codeSize) __attribute__((always_inline));
	int decode(uint8_t *dst);
};

static struct LzwDecoder _lzw;

// Optimized for SH-2: reduced branches, better register usage
inline uint32_t LzwDecoder::nextCode(int codeSize) {
	uint32_t bits = _currentBits;
	uint8_t left = _bitsLeft;
	const uint8_t *buf = _buf;
	
	// First byte always needed
	bits |= (*buf++) << left;
	left += 8;
	
	// Second byte conditionally needed - branchless on SH-2
	uint32_t needMore = (left < codeSize);
	bits |= (needMore ? (*buf << left) : 0);
	buf += needMore;
	left += (needMore << 3);
	
	_buf = buf;
	
	const uint32_t mask = (1 << codeSize) - 1;
	const uint32_t code = bits & mask;
	
	_currentBits = bits >> codeSize;
	_bitsLeft = left - codeSize;
	
	return code;
}

int LzwDecoder::decode(uint8_t *dst) {
	uint8_t *p = dst;
//	_stack = (uint8_t *)0x22600000;//hwram_work;
	_stack = (uint8_t *)hwram_work;
//	_prefix = (uint16_t *)_stack + kStackSize;
	uint8_t * const stackBase = _stack;
	uint8_t * const stackTop = &_stack[kStackSize - 1];
	uint8_t * const stackTop2 = &_stack[kStackSize - 2];
	
	uint32_t previousCode = 0;
	uint32_t lastCode = 0;
	uint32_t currentSlot = kNewCodes;
	uint32_t topSlot = 1 << kCodeWidth;
	int codeSize = kCodeWidth;
	
	// Pre-load frequently used values
	uint16_t * const prefix = _prefix;
	uint8_t * const stack = _stack;
	
	uint32_t currentCode;
	while ((currentCode = nextCode(codeSize)) != kEndCode) {
		if (currentCode == kClearCode) {
			currentSlot = kNewCodes;
			topSlot = 1 << kCodeWidth;
			codeSize = kCodeWidth;
			
			// Skip multiple clear codes efficiently
			do {
				currentCode = nextCode(codeSize);
			} while (currentCode == kClearCode);
			
			if (currentCode == kEndCode) {
				break;
			}
			// Clamp invalid codes
			if (currentCode >= kNewCodes) {
				currentCode = 0;
			}
			previousCode = lastCode = currentCode;
			*p++ = (uint8_t)currentCode;
			continue;
		}
		
		// Main decode path
		uint8_t *currentStackPtr = stackTop;
		uint32_t slot = currentSlot;
		uint32_t code = currentCode;
		
		// Handle code >= slot case
		if (currentCode >= slot) {
			code = lastCode;
			currentStackPtr = stackTop2;
			*currentStackPtr = (uint8_t)previousCode;
		}
		
		// Unwind the code chain
		while (code >= kNewCodes) {
			--currentStackPtr;
			*currentStackPtr = stack[code];
			code = prefix[code];
		}
		
		// Write final byte
		--currentStackPtr;
		*currentStackPtr = (uint8_t)code;
		
		// Update tables if space available
		if (slot < topSlot) {
			stack[slot] = (uint8_t)code;
			previousCode = code;
			prefix[slot] = (uint16_t)lastCode;
			lastCode = currentCode;
			++slot;
			currentSlot = slot;
			
			// Check for code size increase
			if (slot >= topSlot && codeSize < kMaxBits) {
				topSlot <<= 1;
				++codeSize;
			}
		}
		
		// Copy decoded bytes - optimized for SH-2
		// Use word copies when possible
		uint8_t *src = currentStackPtr;
		ptrdiff_t count = stackTop - currentStackPtr;
		
		// Unrolled copy for small common cases
		if (count <= 4) {
			switch(count) {
				case 4: *p++ = *src++;
				case 3: *p++ = *src++;
				case 2: *p++ = *src++;
				case 1: *p++ = *src++;
				case 0: break;
			}
		} else {
			// memcpy for larger blocks
			memcpy(p, src, count);
			p += count;
		}
	}
	
	return p - dst;
}

int decodeLZW(const uint8_t *src, uint8_t *dst) {
	// Avoid memset overhead - zero only what's needed
	_lzw._buf = src;
	_lzw._currentBits = 0;
	_lzw._bitsLeft = 0;
	// _prefix and _stack will be initialized as needed during decode
	
	return _lzw.decode(dst);
}
#else
#pragma GCC optimize ("O2")
/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "intern.h"
#include "util.h"

enum {
	kCodeWidth = 9,
	kClearCode = 1 << (kCodeWidth - 1),
	kEndCode = kClearCode + 1,
	kNewCodes = kEndCode + 1,
	kStackSize = 8192,
	kMaxBits = 12
};

struct LzwDecoder {

	uint16_t _prefix[1 << kMaxBits];
	uint8_t _stack[kStackSize];
	const uint8_t *_buf;
	uint32_t _currentBits;
	uint8_t _bitsLeft;

	uint32_t nextCode(int codeSize);
	int decode(uint8_t *dst);
};

static struct LzwDecoder _lzw;

uint32_t LzwDecoder::nextCode(int codeSize) { // 9 to 12bits
	_currentBits |= (*_buf++) << _bitsLeft;
	_bitsLeft += 8;
	if (_bitsLeft < codeSize) {
		_currentBits |= (*_buf++) << _bitsLeft;
		_bitsLeft += 8;
	}
	const uint32_t code = _currentBits & ((1 << codeSize) - 1);
	_currentBits >>= codeSize;
	_bitsLeft -= codeSize;
	return code;
}

int LzwDecoder::decode(uint8_t *dst) {
////emu_printf("decode %p\n", dst);
	uint8_t *p = dst;
	uint8_t *stackPtr = &_stack[kStackSize - 1];
	uint32_t previousCode = 0;
	uint32_t lastCode = 0;
	uint32_t currentCode;
	uint32_t currentSlot = kNewCodes;
	uint32_t topSlot = 1 << kCodeWidth;
	int codeSize = kCodeWidth;
	while ((currentCode = nextCode(codeSize)) != kEndCode) {
		if (currentCode == kClearCode) {
			currentSlot = kNewCodes;
			topSlot = 1 << kCodeWidth;
			codeSize = kCodeWidth;
			while ((currentCode = nextCode(codeSize)) == kClearCode) {
			}
			if (currentCode == kEndCode) {
				break;
			} else if (currentCode >= kNewCodes) {
				currentCode = 0;
			}
			previousCode = lastCode = currentCode;
			*p++ = (uint8_t)currentCode;
		} else {
			uint8_t *currentStackPtr = stackPtr;
			uint32_t slot = currentSlot;
			uint32_t code = currentCode;
			if (currentCode >= slot) {
				code = lastCode;
				currentStackPtr = &_stack[kStackSize - 2];
				*currentStackPtr = (uint8_t)previousCode;
			}
			while (code >= kNewCodes) {
				--currentStackPtr;
				assert(currentStackPtr >= &_stack[0]);
				*currentStackPtr = _stack[code];
				code = _prefix[code];
			}
			--currentStackPtr;
			*currentStackPtr = (uint8_t)code;
			if (slot < topSlot) {
				_stack[slot] = (uint8_t)code;
				previousCode = code;
				_prefix[slot] = (uint16_t)lastCode;
				lastCode = currentCode;
				++slot;
				currentSlot = slot;
			}
			if (slot >= topSlot && codeSize < kMaxBits) {
				topSlot <<= 1;
				++codeSize;
			}
			while (currentStackPtr < stackPtr) {
				*p++ = *currentStackPtr++;
			}
			assert(currentStackPtr == stackPtr);
		}
	}
	return p - dst;
}

int decodeLZW(const uint8_t *src, uint8_t *dst) {
//emu_printf("decodeLZW %p %p\n", src, dst);
	memset(&_lzw, 0, sizeof(_lzw));
	_lzw._buf = src;
	return _lzw.decode(dst);
}

#endif