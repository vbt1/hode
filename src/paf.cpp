#pragma GCC optimize ("O2")
//#define DEBUG 1
/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */
extern "C" {
#include <sl_def.h>
#include <sega_spr.h>
}
#include "fs.h"
#include "paf.h"
#include "systemstub.h"
#include "util.h"
#include "video.h"

extern "C" {
void 	free(void *ptr);
void	*malloc(size_t);
void *calloc(size_t nmemb, size_t size);
void CSH_AllClr(void);
extern unsigned char frame_x;
extern unsigned char frame_z;
extern Uint8 *cs2ram;
extern Sint32 iondata;
Sint32 GFCD_GetBufSiz(void);
Sint32  CDC_GetBufSiz(Sint32 *totalsiz, Sint32 *bufnum, Sint32 *freesiz);
}

// Configuration: choose which section runs on slave CPU
#define V_ON_SLAVE 0  // 1 = V on slave, H on master; 0 = H on slave, V on master

// Keep your exact original structures
typedef struct {
	const uint8_t *src;
	uint8_t *dst;
	uint8_t **pageBuffers;
} VerticalParams;

typedef struct {
	const uint8_t *base;
	const uint8_t *src;
	uint8_t **pageBuffers;
	uint8_t code;
} HorizontalParams;


/*
static const char *_filenames[] = {
	"HOD.PAF",
	"HOD_DEMO.PAF",
	"HOD_DEMO2.PAF",
	"HOD_OEM.PAF",
	0
};
*/

static bool openPaf(FileSystem *fs, File *f) {
////emu_printf("openPaf\n");	
/*	
	for (int i = 0; _filenames[i]; ++i) {
		GFS_FILE *fp = fs->openAssetFile(_filenames[i]);
		if (fp) {
			f->setFp(fp);
			return true;
		}
	}
	return false;
	*/
	GFS_FILE *fp = fs->openAssetFile("HOD.PAF");
	if (fp) {
		f->setFp(fp);
		return true;
	}
	return false;	
}

static void closePaf(FileSystem *fs, File *f) {
	if (f->_fp) {
		fs->closeFile(f->_fp);
		f->_fp = 0;
	}
}

PafPlayer::PafPlayer(FileSystem *fs)
	: _fs(fs) {
////emu_printf("PafPlayer\n");
	_skipCutscenes = !openPaf(_fs, &_file);
	_videoNum = -1;
	memset(&_pafHdr, 0, sizeof(_pafHdr));
	memset(_pageBuffers, 0, sizeof(_pageBuffers));
	_demuxAudioFrameBlocks = 0;
	_demuxVideoFrameBlocks = 0;
	_audioQueue = _audioQueueTail = 0;
	_playedMask = 0;
	memset(&_pafCb, 0, sizeof(_pafCb));
	_volume = 128;
	_frameMs = kFrameDuration;
#ifdef DEBUG	
	_video = new Video();
#endif
}

PafPlayer::~PafPlayer() {
	unload();
	closePaf(_fs, &_file);
}

void PafPlayer::setVolume(int volume) {
	_volume = volume;
}

uint8_t *lwram_cut;

void PafPlayer::preload(int num) {
//emu_printf("preload %d\n", num);
	lwram_cut = current_lwram;
	
//	assert(num >= 0 && num < kMaxVideosCount);
	if (num < 0 || num >= kMaxVideosCount)
	{
		return;
	}
	
	if (_videoNum != num) {
		unload(_videoNum);
		_videoNum = num;
	}
	_file.seek(num * 4, SEEK_SET);
	_videoOffset = _file.readUint32();
	_file.seek(_videoOffset, SEEK_SET);
	memset(&_pafHdr, 0, sizeof(_pafHdr));
	if (!readPafHeader()) {
		////emu_printf("unload paf\n");
		unload();
		return;
	}
//	uint8_t *buffer = (uint8_t *)calloc(kPageBufferSize * 4 + 256 * 4, 1);
	uint8_t *buffer = (uint8_t *)allocate_memory (TYPE_PAF, kPageBufferSize * 4 + 256 * 4);
	if (!buffer) {
		////emu_printf("preloadPaf() Unable to allocate page buffers\n");
		unload();
		return;
	}
	
	for (int i = 0; i < 4; ++i) {
//		_pageBuffers[i] = (uint8_t *)VDP2_VRAM_A0 + i * kPageBufferSize;
		_pageBuffers[i] = buffer + i * kPageBufferSize;
	}
	_demuxVideoFrameBlocks = (uint8_t *)allocate_memory (TYPE_PAF, _pafHdr.maxVideoFrameBlocksCount * _pafHdr.readBufferSize);
	
	_pafHdr.maxAudioFrameBlocksCount = 0; // vbt : on enleve le son
#if 0
	if (_pafHdr.maxAudioFrameBlocksCount != 0) {
		_demuxAudioFrameBlocks = (uint8_t *)calloc(_pafHdr.maxAudioFrameBlocksCount, _pafHdr.readBufferSize);
		_flushAudioSize = (_pafHdr.maxAudioFrameBlocksCount - 1) * _pafHdr.readBufferSize;
	} else {
		_demuxAudioFrameBlocks = 0;
		_flushAudioSize = 0;
	}
	_audioBufferOffsetRd = 0;
	_audioBufferOffsetWr = 0;
	_audioQueue = _audioQueueTail = 0;
#endif
}

void PafPlayer::play(int num) {
	//emu_printf("play %d %d\n", num, _videoNum);
//	if(num==2)
//	while(1);
	if (_videoNum != num) {
		//emu_printf("preload play\n");
		preload(num);
	}
	if (_videoNum == num) {
		_playedMask |= 1 << num;
		mainLoop();
	}
}

void PafPlayer::unload(int num) {
	current_lwram = lwram_cut;	
	if (_videoNum < 0) {
		return;
	}
//	free(_pageBuffers[0]);
	memset(_pageBuffers, 0, sizeof(_pageBuffers));
//	free(_demuxVideoFrameBlocks);
	_demuxVideoFrameBlocks = 0;
//	free(_demuxAudioFrameBlocks);
	_demuxAudioFrameBlocks = 0;
//	free(_pafHdr.frameBlocksCountTable);
//	free(_pafHdr.framesOffsetTable);
//	free(_pafHdr.frameBlocksOffsetTable);
	memset(&_pafHdr, 0, sizeof(_pafHdr));
	_videoNum = -1;
#ifdef SOUND
	while (_audioQueue) {
		PafAudioQueue *next = _audioQueue->next;
		free(_audioQueue->buffer);
		free(_audioQueue);
		_audioQueue = next;
	}
	_audioQueueTail = 0;
	buf = 0;
#endif
}

bool PafPlayer::readPafHeader() {
	////emu_printf("readPafHeader\n");
	static const char *kSignature = "Packed Animation File V1.0\n(c) 1992-96 Amazing Studio\n";
	_file.read(_bufferBlock, kBufferBlockSize);
/*	if (memcmp(_bufferBlock, kSignature, strlen(kSignature)) != 0) {
		////emu_printf("readPafHeader() Unexpected signature\n");
		return false;
	}
*/
	_pafHdr.frameDuration = READ_LE_UINT32(_bufferBlock + 0x88);
	_pafHdr.startOffset = READ_LE_UINT32(_bufferBlock + 0xA4);
	_pafHdr.preloadFrameBlocksCount = READ_LE_UINT32(_bufferBlock + 0x9C);
	_pafHdr.readBufferSize = READ_LE_UINT32(_bufferBlock + 0x98);
	assert(_pafHdr.readBufferSize == kBufferBlockSize);
	_pafHdr.framesCount = READ_LE_UINT32(_bufferBlock + 0x84);
	if (_pafHdr.framesCount <= 0) {
		////emu_printf("readPafHeader() Invalid number of frames %d\n", _pafHdr.framesCount);
		return false;
	}
	_pafHdr.maxVideoFrameBlocksCount = READ_LE_UINT32(_bufferBlock + 0xA8);
	_pafHdr.maxAudioFrameBlocksCount = READ_LE_UINT32(_bufferBlock + 0xAC);
	_pafHdr.frameBlocksCount = READ_LE_UINT32(_bufferBlock + 0xA0);
	if (_pafHdr.frameBlocksCount <= 0) {
		////emu_printf("readPafHeader() Invalid number of blocks %d\n", _pafHdr.frameBlocksCount);
		return false;
	}
	_pafHdr.frameBlocksCountTable = readPafHeaderTable(_pafHdr.framesCount);
	_pafHdr.framesOffsetTable = readPafHeaderTable(_pafHdr.framesCount);
	_pafHdr.frameBlocksOffsetTable = readPafHeaderTable(_pafHdr.frameBlocksCount);
	return _pafHdr.frameBlocksCountTable != 0 && _pafHdr.framesOffsetTable != 0 && _pafHdr.frameBlocksOffsetTable != 0;
}

uint32_t *PafPlayer::readPafHeaderTable(int count) {
//	uint32_t *dst = (uint32_t *)malloc(count * sizeof(uint32_t));
	uint32_t *dst = (uint32_t *)allocate_memory (TYPE_PAFHEAD, count * sizeof(uint32_t));
	if (!dst) {
		//emu_printf("readPafHeaderTable() Unable to allocate %d bytes\n", count * sizeof(uint32_t));
		return 0;
	}
	for (int i = 0; i < count; ++i) {
		dst[i] = _file.readUint32();
	}
	const int align = (count * 4) & 0x7FF;
	if (align != 0) {
		_file.seek(0x800 - align, SEEK_CUR);
	}
	return dst;
}

void PafPlayer::decodeVideoFrame(const uint8_t *src) {
	const uint8_t *base = src;
	const int code = *src++;
	if (code & 0x20) {
		for (int i = 0; i < 4; ++i) {
			memset(_pageBuffers[i], 0, kPageBufferSize);
		}
		memset(_paletteBuffer, 0, sizeof(_paletteBuffer));
		_paletteChanged = true;
		_currentPageBuffer = 0;
	}
	if (code & 0x40) {
		const int index = src[0] * 3;
		const int count = (src[1] + 1) * 3;
		assert(index + count <= 768);
		src += 2;
		memcpy(_paletteBuffer + index, src, count);
		_paletteChanged = true;
		src += count;
	}
//emu_printf("decode %d -- \n", code & 0xF);	
	
	switch (code & 0xF) {
	case 0:
		decodeVideoFrameOp0(base, src, code);
		break;
	case 1:
		decodeVideoFrameOp1(src);
		break;
	case 2:
		decodeVideoFrameOp2(src);
		break;
	case 4:
		decodeVideoFrameOp4(src);
		break;
	}
}
/*
FORCE_INLINE void pafCopy4x4h(uint8_t *dst, const uint8_t *src) {
	memcpy(dst, src, 4);
	memcpy(dst + 256, src + 4, 4);
	memcpy(dst + 512, src + 8, 4);
	memcpy(dst + 768, src + 12, 4);
}
*/
FORCE_INLINE void pafCopy4x4v(uint8_t *dst, const uint8_t *src) {
	memcpy(dst, src, 4);
	memcpy(dst + 256, src + 256, 4);
	memcpy(dst + 512, src + 512, 4);
	memcpy(dst + 768, src + 768, 4);
}

static void pafCopySrcMask(uint8_t mask, uint8_t *dst, const uint8_t *src) {

	if (mask & 0x8) dst[0] = src[0];
	if (mask & 0x4) dst[1] = src[1];
	if (mask & 0x2) dst[2] = src[2];
	if (mask & 0x1) dst[3] = src[3];
}

static void pafCopySrcMask2(uint8_t mask, uint8_t *dst, const uint8_t *src) {
	// High nibble (first row)
	dst[0] = (mask & 0x80) ? src[0] : dst[0];
	dst[1] = (mask & 0x40) ? src[1] : dst[1];
	dst[2] = (mask & 0x20) ? src[2] : dst[2];
	dst[3] = (mask & 0x10) ? src[3] : dst[3];
	// Low nibble (second row)
	dst += 256;
	src += 256;
	dst[0] = (mask & 0x08) ? src[0] : dst[0];
	dst[1] = (mask & 0x04) ? src[1] : dst[1];
	dst[2] = (mask & 0x02) ? src[2] : dst[2];
	dst[3] = (mask & 0x01) ? src[3] : dst[3];
}

static void pafCopyColorMask(uint8_t mask, uint8_t *dst, uint8_t color) {
	if (mask & 0x8) dst[0] = color;
	if (mask & 0x4) dst[1] = color;
	if (mask & 0x2) dst[2] = color;
	if (mask & 0x1) dst[3] = color;
}

static void pafCopyColorMask2(uint8_t mask, uint8_t *dst, uint8_t color) {
    dst[0] = (mask & 0x80) ? color : dst[0];
    dst[1] = (mask & 0x40) ? color : dst[1];
    dst[2] = (mask & 0x20) ? color : dst[2];
    dst[3] = (mask & 0x10) ? color : dst[3];
    dst += 256;
    dst[0] = (mask & 0x08) ? color : dst[0];
    dst[1] = (mask & 0x04) ? color : dst[1];
    dst[2] = (mask & 0x02) ? color : dst[2];
    dst[3] = (mask & 0x01) ? color : dst[3];
}

static const char *updateSequences[] = {
	"",
	"\x02",
	"\x05\x07",
	"\x05",
	"\x06",
	"\x05\x07\x05\x07",
	"\x05\x07\x05",
	"\x05\x07\x06",
	"\x05\x05",
	"\x03",
	"\x06\x06",
	"\x02\x04",
	"\x02\x04\x05\x07",
	"\x02\x04\x05",
	"\x02\x04\x06",
	"\x02\x04\x05\x07\x05\x07"
};
/*
inline uint8_t *PafPlayer::getVideoPageOffset(uint16_t val) {
	const int x = val & 0x7F; val >>= 7;
	const int y = val & 0x7F; val >>= 7;
	return _pageBuffers[val] + (y * kVideoWidth + x) * 2;
}
*/
FORCE_INLINE uint8_t *fastOffset(uint8_t **pages, const uint8_t *src) {
    const int x = src[1] & 0x7F;
    const int page = src[0] >> 6;
    const int y = ((src[0] << 1) | (src[1] >> 7)) & 0x7F;
//    return pages[page] + (y << 9) + (x << 1);
	return pages[page] + (((y << 8) + x) << 1);
}
#if 1
extern "C" {
    void free(void *ptr);
    void *malloc(size_t);
    void *calloc(size_t nmemb, size_t size);
    extern unsigned char frame_x;
    extern unsigned char frame_z;
    extern Uint8 *cs2ram;
    extern Sint32 iondata;
}

#include <stdint.h>  // For uint32_t, uint16_t, etc.

#define READ_LE_UINT16(p) ((uint16_t)((p)[0] | ((p)[1] << 8)))
/*
FORCE_INLINE void pafCopy4x4h(uint8_t *d, const uint8_t *s) {
    // Fully unrolled with <<8 instead of *256
    d[0]          = s[0];  d[1]          = s[1];  d[2]          = s[2];  d[3]          = s[3];
    d[1<<8]       = s[4];  d[(1<<8)+1]   = s[5];  d[(1<<8)+2]   = s[6];  d[(1<<8)+3]   = s[7];
    d[2<<8]       = s[8];  d[(2<<8)+1]   = s[9];  d[(2<<8)+2]   = s[10]; d[(2<<8)+3]   = s[11];
    d[3<<8]       = s[12]; d[(3<<8)+1]   = s[13]; d[(3<<8)+2]   = s[14]; d[(3<<8)+3]   = s[15];
    // Alternative (even smaller assembly on SH-2):
    // uint32_t *dst = (uint32_t*)d;
   // const uint32_t *src = (const uint32_t*)s;
    // dst[0] = src[0]; dst[64] = src[1]; dst[128] = src[2]; dst[192] = src[3];
}
*/
void PafPlayer::decodeVideoFrameOp0(const uint8_t *base, const uint8_t *src, uint8_t code) {
    uint32_t t0 = g_system->getTimeStamp();

    // === Skip horizontal section ===
    const uint8_t *v = src;
    int n = *v++;
    if (n) {
        if (code & 0x10) {
            int a = (v - base) & 3;
            if (a) v += 4 - a;
        }
        for (int i = 0; i < n; ++i) {
            uint32_t o = (v[1] & 0x7F) << 1;
            uint32_t e = READ_LE_UINT16(v + 2) + o;
            v += 4;
            while (o < e) {
                v += 16;
                ++o;
            }
        }
    }
    uint32_t t1 = g_system->getTimeStamp();

	// === Decode horizontal (optimized) ===
	{
		int n = *src;
		if (n) {
			const uint8_t *s = src + 1;
			if (code & 0x10) {
				int a = (s - base) & 3;
				if (a) s += 4 - a;
			}

			uint8_t **p = _pageBuffers;

			for (int i = 0; i < n; ++i) {
				// Precompute common bitfield parts
				uint8_t hi = s[0];
				uint8_t lo = s[1];
				uint8_t idx = hi >> 6;
				uint32_t temp = ((hi << 1) | (lo >> 7)) & 0x7F;
				uint32_t offset = (temp << 8 | (lo & 0x7F)) << 1;

				uint8_t *d_base = p[idx] + offset;

				uint32_t o = (lo & 0x7F) << 1;
				uint32_t e = READ_LE_UINT16(s + 2) + o;
				s += 4;

				// Precompute row destinations (only 3 shifts total!)
				uint8_t *d0 = d_base;
				uint8_t *d1 = d_base + (1 << 8);
				uint8_t *d2 = d_base + (2 << 8);
				uint8_t *d3 = d_base + (3 << 8);

				while (o < e) {
					// Source rows (stride 16 bytes per row in source block)
					const uint8_t *s0 = s;
					const uint8_t *s1 = s + 4;
					const uint8_t *s2 = s + 8;
					const uint8_t *s3 = s + 12;

					// Fast 4x4 copy using row pointers — no repeated shifts
					d0[0] = s0[0]; d0[1] = s0[1]; d0[2] = s0[2]; d0[3] = s0[3];
					d1[0] = s1[0]; d1[1] = s1[1]; d1[2] = s1[2]; d1[3] = s1[3];
					d2[0] = s2[0]; d2[1] = s2[1]; d2[2] = s2[2]; d2[3] = s2[3];
					d3[0] = s3[0]; d3[1] = s3[1]; d3[2] = s3[2]; d3[3] = s3[3];

					s += 16;
					d_base += 4;
					d0 += 4; d1 += 4; d2 += 4; d3 += 4;

					if ((++o & 0x3F) == 0) {
						d_base += 768;
						d0 += 768; d1 += 768; d2 += 768; d3 += 768;
					}
				}
			}
		}
	}

    uint32_t t2 = g_system->getTimeStamp();

    // === Decode vertical (uses <<8 instead of *256) ===
    {
		uint8_t *d = _pageBuffers[_currentPageBuffer];
		uint8_t **p = _pageBuffers;
		const uint8_t *s = v;

		for (int y = 0; y < 192; y += 4, d += 768) {
			for (int x = 0; x < 256; x += 4, d += 4, s += 2) {
				uint8_t idx = s[0] >> 6;
				uint32_t offset = (((((s[0] << 1) | (s[1] >> 7)) & 0x7F) << 8) | (s[1] & 0x7F)) << 1;
				const uint8_t *t = p[idx] + offset;

				// Precompute row pointers — only 4 shifts per tile
				uint8_t       *d0 = d;
				uint8_t       *d1 = d + (1 << 8);
				uint8_t       *d2 = d + (2 << 8);
				uint8_t       *d3 = d + (3 << 8);

				const uint8_t *t0 = t;
				const uint8_t *t1 = t + (1 << 8);
				const uint8_t *t2 = t + (2 << 8);
				const uint8_t *t3 = t + (3 << 8);

				// Fully unrolled 4×4 copy — pure ASCII, no hidden characters
				d0[0] = t0[0]; d0[1] = t0[1]; d0[2] = t0[2]; d0[3] = t0[3];
				d1[0] = t1[0]; d1[1] = t1[1]; d1[2] = t1[2]; d1[3] = t1[3];
				d2[0] = t2[0]; d2[1] = t2[1]; d2[2] = t2[2]; d2[3] = t2[3];
				d3[0] = t3[0]; d3[1] = t3[1]; d3[2] = t3[2]; d3[3] = t3[3];
			}
		}
    }
    uint32_t t3 = g_system->getTimeStamp();

    // === OP section (mask operations) ===
    const uint8_t *op = v + 6144;
    uint32_t sz = READ_LE_UINT16(op);
    op += 4;
    uint8_t *d = _pageBuffers[_currentPageBuffer];
    const uint8_t *src2 = op + sz;

    for (int y = 0; y < kVideoHeight; y += 4, d += kVideoWidth * 3) {
        for (int x = 0; x < kVideoWidth; x += 4, d += 4) {
            const char *q = updateSequences[(x & 4) ? (*op++ & 15) : (*op >> 4)];
            uint8_t k;
            while ((k = *q++)) {
                uint8_t *d0 = d + 512, *d1;  // 512 is intentional (half-page offset)
                const uint8_t *s2;
                uint8_t m, c;
                switch (k) {
                    case 2: d0 = d;
                    case 3: c = *src2++;
                    case 4: m = *src2++;
                            d1 = d0 + (1<<8);  // <<8 instead of +256
                            pafCopyColorMask(m >> 4, d0, c);
                            pafCopyColorMask(m & 15, d1, c);
                            break;
                    case 5: d0 = d;
                    case 6: s2 = fastOffset(_pageBuffers, src2); src2 += 2;
                    case 7: m = *src2++;
                            d1 = d0 + (1<<8);  // <<8 instead of +256
                            pafCopySrcMask(m >> 4, d0, s2 + (d0 - d));
                            pafCopySrcMask(m & 15, d1, s2 + (d1 - d));
                            break;
                }
            }
        }
    }
    uint32_t t5 = g_system->getTimeStamp();

//    emu_printf("Times: Skip=%u H=%u V=%u Prep=0 OP=%u\n", t1-t0, t2-t1, t3-t2, t5-t3);
}
#else
static const uint8_t* skipHorizontalSection(const uint8_t *base, const uint8_t *src, uint8_t code) {
	const int count = *src++;
	if (count != 0) {
		if ((code & 0x10) != 0) {
			const int align = (src - base) & 3;
			if (align != 0) src += 4 - align;
		}
		for (int i = 0; i < count; ++i) {
			uint32_t offset = (src[1] & 0x7F) * 2;
			const uint32_t end = READ_LE_UINT16(src + 2) + offset;
			src += 4;
			do {
				++offset;
				src += 16;
			} while (offset < end);
		}
	}
	return src;
}

#define MAX_HRUNS 128

typedef struct {
	const uint8_t *srcData;  // Pointer to block data in original stream
	uint16_t dstOffset;      // Offset in page buffer
	uint16_t blockCount;     // Number of 4x4 blocks in this run
	uint32_t startCol;       // Starting column offset (for row breaks)
	uint8_t page;            // Which page buffer (0-3)
} HRun;

typedef struct {
	const uint8_t *base;
	const uint8_t *src;
	uint8_t code;
	HRun *runs;
	int count;
} HContext;

// SLAVE: Build optimized run list (merge consecutive blocks)
static void slave_build_horizontal(void *arg) {
	HContext *ctx = (HContext *)arg;
	const uint8_t *base = ctx->base;
	const uint8_t *src = ctx->src;
	int out = 0;
	
	const int count = *src++;
	if (count == 0) {
		ctx->count = 0;
		return;
	}
	
	if (ctx->code & 0x10) {
		const int align = (src - base) & 3;
		if (align != 0) src += 4 - align;
	}
	
	// Process all segments, merging them into runs
	for (int i = 0; i < count && out < MAX_HRUNS; i++) {
		const int x = src[1] & 0x7F;
		const int page = src[0] >> 6;
		const int y = ((src[0] << 1) | (src[1] >> 7)) & 0x7F;
		
		uint32_t startCol = (src[1] & 0x7F) * 2;
		uint32_t endCol = READ_LE_UINT16(src + 2) + startCol;
		uint32_t blockCount = endCol - startCol;
		src += 4;
		
		// Store as single run
		ctx->runs[out].srcData = src;
		ctx->runs[out].dstOffset = ((y << 8) + x) << 1;
		ctx->runs[out].blockCount = blockCount;
		ctx->runs[out].startCol = startCol;
		ctx->runs[out].page = page;
		out++;
		
		// Skip past all block data
		src += blockCount * 16;
	}
	
	ctx->count = out;
}

// MASTER: Execute runs (optimized - fewer loop iterations)
static void exec_horizontal_runs(uint8_t **pageBuffers, const HRun *runs, int count) {
	for (int i = 0; i < count; i++) {
		const uint8_t *src = runs[i].srcData;
		uint8_t *page = pageBuffers[runs[i].page];
		uint16_t dst = runs[i].dstOffset;
		uint16_t n = runs[i].blockCount;
		uint32_t col = runs[i].startCol;
		
		// Process all blocks in this run
		while (n--) {
			col++;
			uint8_t *d = page + dst;
			
			// Copy 4x4 block (unrolled)
			d[0] = src[0]; d[1] = src[1]; d[2] = src[2]; d[3] = src[3];
			d[256] = src[4]; d[257] = src[5]; d[258] = src[6]; d[259] = src[7];
			d[512] = src[8]; d[513] = src[9]; d[514] = src[10]; d[515] = src[11];
			d[768] = src[12]; d[769] = src[13]; d[770] = src[14]; d[771] = src[15];
			
			src += 16;
			dst += 4;
			
			if ((col & 0x3F) == 0) {
				dst += 768;
			}
		}
	}
}

void PafPlayer::decodeVideoFrameOp0(const uint8_t *base, const uint8_t *src, uint8_t code) {
	unsigned int t0, t1, t2, t3;
	t0 = g_system->getTimeStamp();
	
	// Get v_src pointer (skip H section)
	const uint8_t *v_src = skipHorizontalSection(base, src, code);
	
	// Setup context for slave
	static HRun hRuns[MAX_HRUNS];
	static HContext hCtx;
	hCtx.base = base;
	hCtx.src = src;
	hCtx.code = code;
	hCtx.runs = hRuns;
	hCtx.count = 0;
	
	// 1) Start slave building horizontal runs (parsing only, no pageBuffer access)
	SPR_RunSlaveSH((PARA_RTN*)slave_build_horizontal, &hCtx);
	
	// 2) While slave builds H, execute V on master IN PARALLEL!
	uint8_t *dst = _pageBuffers[_currentPageBuffer];
	const uint8_t *v_ptr = v_src;
	for (int y = 0; y < kVideoHeight; y += 4, dst += kVideoWidth * 3) {
		for (int x = 0; x < kVideoWidth; x += 4, dst += 4) {
			const uint8_t *src2 = fastOffset(_pageBuffers, v_ptr);
			v_ptr += 2;
			
			uint8_t *r0 = dst;
			uint8_t *r1 = dst + 256;
			uint8_t *r2 = dst + 512;
			uint8_t *r3 = dst + 768;
			
			r0[0] = src2[0]; r0[1] = src2[1]; r0[2] = src2[2]; r0[3] = src2[3];
			r1[0] = src2[256]; r1[1] = src2[257]; r1[2] = src2[258]; r1[3] = src2[259];
			r2[0] = src2[512]; r2[1] = src2[513]; r2[2] = src2[514]; r2[3] = src2[515];
			r3[0] = src2[768]; r3[1] = src2[769]; r3[2] = src2[770]; r3[3] = src2[771];
		}
	}
	
	// 3) Now wait for slave to finish building H runs
	SPR_WaitEndSlaveSH();
	t1 = g_system->getTimeStamp();
	
	// 4) Execute horizontal runs (now that V is done and slave build is done)
	exec_horizontal_runs(_pageBuffers, hRuns, hCtx.count);
	t2 = g_system->getTimeStamp();
	
	// 5) OP section
	src = v_src + 6144;
	
	const uint32_t opcodesSize = READ_LE_UINT16(src);
	src += 4;
	const uint8_t *opcodesData = src;
	src += opcodesSize;
	
	uint8_t mask = 0;
	uint8_t color = 0;
	const uint8_t *src2 = 0;
	const char *seq;
	
	dst = _pageBuffers[_currentPageBuffer];
	for (int y = 0; y < kVideoHeight; y += 4, dst += kVideoWidth * 3) {
		for (int x = 0; x < kVideoWidth; x += 4, dst += 4) {
			if (x & 4) {
				seq = updateSequences[*opcodesData & 15];
				++opcodesData;
			} else {
				seq = updateSequences[*opcodesData >> 4];
			}
			
			for (; (code = *seq) != 0; ++seq) {
				uint8_t *d0, *d1;
				d0 = dst + 512;
				
				switch (code) {
				case 2:
					d0 = dst;
				case 3:
					color = *src++;
				case 4:
					mask = *src++;
					d1 = d0 + 256;
					pafCopyColorMask(mask >> 4, d0, color);
					pafCopyColorMask(mask & 15, d1, color);
					break;
				case 5:
					d0 = dst;
				case 6:
					src2 = fastOffset(_pageBuffers, src);
					src += 2;
				case 7:
					mask = *src++;
					d1 = d0 + 256;
					pafCopySrcMask(mask >> 4, d0, src2 + (d0 - dst));
					pafCopySrcMask(mask & 15, d1, src2 + (d1 - dst));
					break;
				}
			}
		}
	}
	
//	t3 = g_system->getTimeStamp();
	
//	emu_printf("Times: V(master)+H_build(slave)=%d H_exec=%d OP=%d runs=%d\n", 	           t1-t0, t2-t1, t3-t2, hCtx.count);
}
#endif

FORCE_INLINE void PafPlayer::decodeVideoFrameOp1(const uint8_t *src) {
	memcpy(_pageBuffers[_currentPageBuffer], src + 2, kVideoWidth * kVideoHeight);
}

FORCE_INLINE void PafPlayer::decodeVideoFrameOp2(const uint8_t *src) {
	const int page = *src++;
	if (page != _currentPageBuffer) {
		memcpy(_pageBuffers[_currentPageBuffer], _pageBuffers[page], kVideoWidth * kVideoHeight);
	}
}

void PafPlayer::decodeVideoFrameOp4(const uint8_t *src) {
	uint8_t *dst = _pageBuffers[_currentPageBuffer];
	src += 2; // compressed size
	const uint8_t *end = dst + (kVideoWidth * kVideoHeight);
	
	while (dst < end) {
		const int8_t code = *src++;
		if (code < 0) {
			// RLE: repeat color
			int count = 1 - code;
			const uint8_t color = *src++;
			do {
				*dst++ = color;
			} while (--count);
		} else {
			// Literal copy
			int count = code + 1;
			do {
				*dst++ = *src++;
			} while (--count);
		}
	}
}
#ifdef SOUND
static void decodeAudioFrame2205(const uint8_t *src, int len, int16_t *dst, int volume) {
	static const int offset = 256 * sizeof(int16_t);
	for (int i = 0; i < len * 2; ++i) { // stereo
		const int16_t sample = READ_LE_UINT16(src + src[offset + i] * sizeof(int16_t));
		dst[i] = CLIP((sample * volume + 64) >> 7, -32768, 32767);
	}
}

void PafPlayer::decodeAudioFrame(const uint8_t *src, uint32_t offset, uint32_t size) {
	assert(size == _pafHdr.readBufferSize);

	// copy should be sequential
	if (offset != _audioBufferOffsetWr) {
		////emu_printf("Unexpected offset 0x%x wr 0x%x rd 0x%x num %d\n", offset, _audioBufferOffsetWr, _audioBufferOffsetRd, _videoNum);
		assert(offset == 0);
		// this happens in paf #3 of Italian release, there is a flush at 0x16800 instead of 0x1f000
		_audioBufferOffsetWr = 0;
		_audioBufferOffsetRd = 0;
	}

	_audioBufferOffsetWr = offset + size;

	const int count = (_audioBufferOffsetWr - _audioBufferOffsetRd) / kAudioStrideSize;
	if (count != 0) {
		PafAudioQueue *sq = (PafAudioQueue *)malloc(sizeof(PafAudioQueue));
		if (sq) {
			sq->offset = 0;
			sq->size = count * kAudioSamples * 2;
			sq->buffer = (int16_t *)malloc(sq->size * sizeof(int16_t));
			if (sq->buffer) {
				for (int i = 0; i < count; ++i) {
					decodeAudioFrame2205(src + _audioBufferOffsetRd + i * kAudioStrideSize, kAudioSamples, sq->buffer + i * kAudioSamples * 2, _volume);
				}
			}
			sq->next = 0;

			g_system->lockAudio();
			if (_audioQueueTail) {
				_audioQueueTail->next = sq;
			} else {
				assert(!_audioQueue);
				_audioQueue = sq;
			}
			_audioQueueTail = sq;
			g_system->unlockAudio();
		}
		else
		{
			////emu_printf("PafAudioQueue() Unable to allocate %d bytes\n", count * sizeof(uint32_t));
		}

		_audioBufferOffsetRd += count * kAudioStrideSize;
	}
	if (_audioBufferOffsetWr == _flushAudioSize) {
		_audioBufferOffsetWr = 0;
		_audioBufferOffsetRd = 0;
	}
}

void PafPlayer::mix(int16_t *buf, int samples) {
	while (_audioQueue && samples > 0) {
		assert(_audioQueue->size != 0);
		const int count = MIN(samples, _audioQueue->size - _audioQueue->offset);
		memcpy(buf, _audioQueue->buffer + _audioQueue->offset, count * sizeof(int16_t));
		buf += count;
		_audioQueue->offset += count;
		if (_audioQueue->offset >= _audioQueue->size) {
			assert(_audioQueue->offset == _audioQueue->size);
			PafAudioQueue *next = _audioQueue->next;
			free(_audioQueue->buffer);
			free(_audioQueue);
			_audioQueue = next;
		}
		samples -= count;
	}
	if (!_audioQueue) {
		_audioQueueTail = 0;
	}
	if (samples > 0) {
		debug(kDebug_PAF, "audioQueue underrun %d", samples);
	}
}

static void mixAudio(void *userdata, int16_t *buf, int len) {
	((PafPlayer *)userdata)->mix(buf, len);
}
#endif

#if 0
void PafPlayer::mainLoop() {
////emu_printf("mainLoop paf\n");
	_file.seek(_videoOffset + _pafHdr.startOffset, SEEK_SET);
	for (int i = 0; i < 4; ++i) {
		memset(_pageBuffers[i], 0, kPageBufferSize);
	}
	memset(_paletteBuffer, 0, sizeof(_paletteBuffer));
	_paletteChanged = true;
	_currentPageBuffer = 0;
	int currentFrameBlock = 0;
#ifdef SOUND
	AudioCallback prevAudioCb;
	if (_demuxAudioFrameBlocks) {
		AudioCallback audioCb;
		audioCb.proc = mixAudio;
		audioCb.userdata = this;
		prevAudioCb = g_system->setAudioCallback(audioCb);
	}
#endif
	// keep original frame rate for audio
	const uint32_t frameMs = (_demuxAudioFrameBlocks != 0) ? _pafHdr.frameDuration : (_pafHdr.frameDuration * _frameMs / kFrameDuration);
	uint32_t frameTime = g_system->getTimeStamp() + frameMs;

	uint32_t blocksCountForFrame = _pafHdr.preloadFrameBlocksCount;
#ifdef DEBUG
	_video->_font = (uint8_t *)0x25e6df94;	// vbt : hardcoded font buffer address vram vdp2
	static uint8_t last_frame_z = 0xFF;
	static char buffer[8];
#endif
//emu_printf("framesCount %d %d\n", _pafHdr.framesCount, _pafHdr.readBufferSize);
	for (int i = 0; i < (int)_pafHdr.framesCount; ++i) {
		// read buffering blocks
		blocksCountForFrame += _pafHdr.frameBlocksCountTable[i];
		while (blocksCountForFrame != 0) {
unsigned int s1 = g_system->getTimeStamp();
			_file.read(_bufferBlock, _pafHdr.readBufferSize);
			const uint32_t dstOffset = _pafHdr.frameBlocksOffsetTable[currentFrameBlock] & ~(1 << 31);
			if (_pafHdr.frameBlocksOffsetTable[currentFrameBlock] & (1 << 31)) {
#ifdef SOUND
				assert(dstOffset + _pafHdr.readBufferSize <= _pafHdr.maxAudioFrameBlocksCount * _pafHdr.readBufferSize);
				memcpy(_demuxAudioFrameBlocks + dstOffset, _bufferBlock, _pafHdr.readBufferSize);
				decodeAudioFrame(_demuxAudioFrameBlocks, dstOffset, _pafHdr.readBufferSize);
#endif
			} else {
//				assert(dstOffset + _pafHdr.readBufferSize <= _pafHdr.maxVideoFrameBlocksCount * _pafHdr.readBufferSize);

				if (dstOffset + _pafHdr.readBufferSize >
					_pafHdr.maxVideoFrameBlocksCount * _pafHdr.readBufferSize) {
					break;
				}
				memcpy(_demuxVideoFrameBlocks + dstOffset, _bufferBlock, _pafHdr.readBufferSize);

unsigned int e1 = g_system->getTimeStamp();
int result = e1-s1;
if(result>0)
	//emu_printf("--duration %s : %d\n","read_copy", result);
			}
			++currentFrameBlock;
			--blocksCountForFrame;
		}
		// decode video data

unsigned int s2 = g_system->getTimeStamp();
		decodeVideoFrame(_demuxVideoFrameBlocks + _pafHdr.framesOffsetTable[i]);
unsigned int e2 = g_system->getTimeStamp();
/*
int result = e2-s2;
if(result>0)
	//emu_printf("--duration %s : %d\n","decodeframe", result);
		if (_pafCb.frameProc) {
			_pafCb.frameProc(_pafCb.userdata, i, _pageBuffers[_currentPageBuffer]);
		} else {
*/
			g_system->copyRect((int)0, (int)0, (int)kVideoWidth, (int)kVideoHeight, _pageBuffers[_currentPageBuffer], (int)kVideoWidth);
//		}
		if (_paletteChanged) {
			_paletteChanged = false;
			g_system->setPalette(_paletteBuffer, 256, 6);
		}
#ifdef DEBUG
		if (frame_z != last_frame_z) {
			last_frame_z = frame_z;

			buffer[0] = '0' + (frame_z / 10);
			buffer[1] = '0' + (frame_z % 10);
			buffer[2] = 0;
		}
		_video->drawString(buffer, (Video::W - 24), 0, 2, (uint8 *)VDP2_VRAM_A0);
#else
//	emu_printf("fps %d\n", frame_z);
#endif

unsigned int e3 = g_system->getTimeStamp();
result = e3-e2;
if(result>0)
	emu_printf("--duration %s : %d\n","copyrect", result);

		g_system->updateScreen(false);
//		g_system->processEvents();
		if (g_system->inp.quit || g_system->inp.keyPressed(SYS_INP_ESC) || g_system->inp.keyPressed(SYS_INP_RUN)) {
			break;
		}

		const int delay = MAX<int>(10, frameTime - g_system->getTimeStamp());
//		g_system->sleep(delay);
		frame_x++;
		// set next decoding video page
		++_currentPageBuffer;
		_currentPageBuffer &= 3;
	}
#ifdef SOUND
	if (_pafCb.endProc) {
		_pafCb.endProc(_pafCb.userdata);
	}

	// restore audio callback
	if (_demuxAudioFrameBlocks) {
		g_system->setAudioCallback(prevAudioCb);
	}
#endif
	unload();
}
#else
uint8_t *buf = NULL;
void PafPlayer::mainLoop() {
    _file.seek(_videoOffset + _pafHdr.startOffset, SEEK_SET);
    
    for (int i = 0; i < 4; ++i) {
        memset(_pageBuffers[i], 0, kPageBufferSize);
    }
    memset(_paletteBuffer, 0, sizeof(_paletteBuffer));
    _paletteChanged = true;
    _currentPageBuffer = 0;
    int currentFrameBlock = 0;

#ifdef SOUND
    AudioCallback prevAudioCb;
    if (_demuxAudioFrameBlocks) {
        AudioCallback audioCb;
        audioCb.proc = mixAudio;
        audioCb.userdata = this;
        prevAudioCb = g_system->setAudioCallback(audioCb);
    }
#endif

    uint32_t blocksCountForFrame = _pafHdr.preloadFrameBlocksCount;
    uint32_t blocksCountForFrame2;
    uint32_t totalBytes2;

#ifdef DEBUG
    _video->_font = (uint8_t *)0x25e6df94;
    static uint8_t last_frame_z = 0xFF;
    static char buffer[8];
#endif

#define DOUBLE 1
#define NUM_BUFFERS 6
#define FRAMES_PER_READ 5

#ifdef DOUBLE
	buf = (uint8_t *)allocate_memory (TYPE_PAFBUF, 250000);

    // Setup buffer array
    uint8_t* buffers[NUM_BUFFERS] = {
        buf,
        buf + 90000,
        buf + 120000,
        buf + 150000,
        buf + 175000,
        buf + 200000
    };
    
    int currentBuffer = 0;  // Buffer being processed
    int readBuffer = 1;     // Buffer being read into
    
    // First batch read (preload + first frame) into buffer 0
    blocksCountForFrame += _pafHdr.frameBlocksCountTable[0];
    uint32_t totalBytes = blocksCountForFrame * _pafHdr.readBufferSize;
    
    unsigned int s0 = g_system->getTimeStamp();
    int r = _file.batchRead(buffers[0], totalBytes);
    int delta = (r - totalBytes);
    
    // Start first async read for frames 1-4 into buffer 1
    if (_pafHdr.framesCount > 1) {
        blocksCountForFrame2 = 0;
        totalBytes = 0;
        
        for (int j = 1; j <= FRAMES_PER_READ && j < (int)_pafHdr.framesCount; j++) {
            blocksCountForFrame2 += _pafHdr.frameBlocksCountTable[j];
            totalBytes += _pafHdr.frameBlocksCountTable[j] * _pafHdr.readBufferSize;
        }
        
        totalBytes2 = totalBytes;
        _file.asynchInit(buffers[readBuffer], totalBytes);
    }
    
    bool asyncReadActive = (_pafHdr.framesCount > 1);
#endif

    // Force 10fps = 100ms per frame
    const uint32_t frameMs = 100;
    uint32_t frameTime = g_system->getTimeStamp();

    for (int i = 0; i < (int)_pafHdr.framesCount; ++i) {
#ifndef DOUBLE
        unsigned int s0 = g_system->getTimeStamp();
        blocksCountForFrame += _pafHdr.frameBlocksCountTable[i];
        uint32_t totalBytes = (blocksCountForFrame * _pafHdr.readBufferSize);
        uint8_t* readBuffer = buf;
        
        int r = _file.batchRead(readBuffer, totalBytes);
        readBuffer += (r - totalBytes);
#else
/*
	Sint32 stat, ndata;
    GFS_NwGetStat(_file._fp->fid, &stat, &ndata);

    if(stat == GFS_SVR_COMPLETED)
    {
asyncReadActive= false;
    }
*/
        // Wait and start next read every 4th frame (1, 5, 9, 13...)
        if (i > 0 && i % FRAMES_PER_READ == 1 && asyncReadActive) {
            // Wait for async read to complete
    uint32_t a = g_system->getTimeStamp();
            int r = _file.asynchWait(buffers[readBuffer], totalBytes2);
            // Switch to the buffer that just finished reading
            currentBuffer = readBuffer;
            delta = (r - totalBytes2);
uint32_t b = g_system->getTimeStamp();           
//emu_printf("chunk %d duration %d\n", totalBytes2, b-a);

            blocksCountForFrame = blocksCountForFrame2;
            
            // Move to next buffer for next async read
            readBuffer = (readBuffer + 1) % NUM_BUFFERS;
            
            // IMMEDIATELY start next async read
            int nextFrameStart = i + FRAMES_PER_READ;
            if (nextFrameStart < (int)_pafHdr.framesCount) {
                blocksCountForFrame2 = 0;
                totalBytes = 0;
                
                for (int j = 0; j < FRAMES_PER_READ && (nextFrameStart + j) < (int)_pafHdr.framesCount; j++) {
                    blocksCountForFrame2 += _pafHdr.frameBlocksCountTable[nextFrameStart + j];
                    totalBytes += _pafHdr.frameBlocksCountTable[nextFrameStart + j] * _pafHdr.readBufferSize;
                }
                
                totalBytes2 = totalBytes;
                _file.asynchInit(buffers[readBuffer], totalBytes);
            } else {
                asyncReadActive = false;
            }
        }
        
        // Use current buffer with delta offset
        uint8_t* currentData = buffers[currentBuffer] + delta;
#endif

        // Process all blocks for current frame
        uint32_t tempBlocksCount = blocksCountForFrame;
        while (tempBlocksCount > 0) {
            const uint32_t dstOffset = _pafHdr.frameBlocksOffsetTable[currentFrameBlock] & ~(1 << 31);
            if (!(_pafHdr.frameBlocksOffsetTable[currentFrameBlock] & (1 << 31))) {
                if (dstOffset + _pafHdr.readBufferSize > _pafHdr.maxVideoFrameBlocksCount * _pafHdr.readBufferSize) {
                    break;
                }
                memcpy(_demuxVideoFrameBlocks + dstOffset, currentData, _pafHdr.readBufferSize);
            }
            ++currentFrameBlock;
            --tempBlocksCount;
            currentData += _pafHdr.readBufferSize;
        }
        
        blocksCountForFrame = tempBlocksCount;
        decodeVideoFrame(_demuxVideoFrameBlocks + _pafHdr.framesOffsetTable[i]);
        g_system->copyRect(0, 0, kVideoWidth, kVideoHeight, _pageBuffers[_currentPageBuffer], kVideoWidth);

        if (_paletteChanged) {
            _paletteChanged = false;
            g_system->setPalette(_paletteBuffer, 256, 6);
            g_system->updateScreen(false);
        }

#ifdef DEBUG
        if (frame_z != last_frame_z) {
            last_frame_z = frame_z;
            buffer[0] = '0' + (frame_z / 10);
            buffer[1] = '0' + (frame_z % 10);
            buffer[2] = 0;
        }
        _video->drawString(buffer, (Video::W - 24), 0, 2, (uint8 *)VDP2_VRAM_A0);
#else
//        emu_printf("fps %d\n", frame_z);
#endif

        // Quit check
        if (g_system->inp.quit || g_system->inp.keyPressed(SYS_INP_ESC) || g_system->inp.keyPressed(SYS_INP_RUN)) {
            break;
        }

        frame_x++;
        ++_currentPageBuffer;
        _currentPageBuffer &= 3;

        // Frame rate synchronization - 10fps = 100ms per frame
        frameTime += frameMs;
        uint32_t currentTime = g_system->getTimeStamp();
        if (frameTime > currentTime) {
            const int delay = frameTime - currentTime;
            g_system->sleep(delay);
//            emu_printf("delay %d ms\n", delay);
        } else {
            emu_printf("behind by %d ms!\n", (int)(currentTime - frameTime));
            frameTime = currentTime;
        }
    }

    unload();
}
#endif
void PafPlayer::setCallback(const PafCallback *pafCb) {
	if (pafCb) {
		_pafCb = *pafCb;
	} else {
		memset(&_pafCb, 0, sizeof(_pafCb));
	}
}
