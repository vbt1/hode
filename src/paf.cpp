#pragma GCC optimize ("O2")
//#define DEBUG 1
//#define DEBUG2 1
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
extern Uint8 *cs1ram;
//extern Uint8 *cs2ram;
extern Sint32 iondata;
Sint32 GFCD_GetBufSiz(void);
Sint32  CDC_GetBufSiz(Sint32 *totalsiz, Sint32 *bufnum, Sint32 *freesiz);
uint8_t *hwram_work_paf;
}

// =============================================================================
// SH-2 safe memory helpers
// =============================================================================

FORCE_INLINE uint32_t load4_u16(const uint8_t *p) {
	const uint16_t hi = *(const uint16_t *)p;
	const uint16_t lo = *(const uint16_t *)(p + 2);
	return ((uint32_t)hi << 16) | lo;
}

FORCE_INLINE uint32_t load4_any(const uint8_t *p) {
	uint32_t v;
	__builtin_memcpy(&v, p, 4);
	return v;
}

FORCE_INLINE void store4_a(uint8_t *p, uint32_t v) {
	*(uint32_t *)p = v;
}

FORCE_INLINE void copy4x4_tile(
	uint8_t *d0, uint8_t *d1, uint8_t *d2, uint8_t *d3,
	const uint8_t *t0, const uint8_t *t1,
	const uint8_t *t2, const uint8_t *t3)
{
	store4_a(d0, load4_u16(t0));
	store4_a(d1, load4_u16(t1));
	store4_a(d2, load4_u16(t2));
	store4_a(d3, load4_u16(t3));
}

// =============================================================================

static bool openPaf(FileSystem *fs, File *f) {
	GFS_FILE *fp = fs->openAssetFile("HOD.PAF");
	if (fp) { f->setFp(fp); return true; }
	return false;
}

static void closePaf(FileSystem *fs, File *f) {
	if (f->_fp) 
	{ 
		GFS_NwStop(f->_fp->fid);
		fs->closeFile(f->_fp); 
		f->_fp = 0; 
	}
}

PafPlayer::PafPlayer(FileSystem *fs, Video *vid)
	: _fs(fs), _video(vid) {
	_skipCutscenes = !openPaf(_fs, &_file);
	_videoNum = -1;
	memset(&_pafHdr, 0, sizeof(_pafHdr));
	memset(_pageBuffers, 0, sizeof(_pageBuffers));
/*
	_demuxAudioFrameBlocks = 0;
	_demuxVideoFrameBlocks = 0;
	_audioQueue = _audioQueueTail = 0;
*/
	_playedMask = 0;
	memset(&_pafCb, 0, sizeof(_pafCb));
	_volume = 128;
	_frameMs = kFrameDuration;
}

PafPlayer::~PafPlayer() {
	unload();
	closePaf(_fs, &_file);
}

void PafPlayer::setVolume(int volume) { _volume = volume; }

uint8_t *lwram_cut = 0;

void PafPlayer::preload(int num) {
	if (num < 0 || num >= kMaxVideosCount) return;

	if (_file._fp == 0) openPaf(_fs, &_file);
//	else {
//		emu_printf("no openPaf!!!\n");
//	}
	if (_videoNum != num) { unload(_videoNum); _videoNum = num; }
	
	_paletteBuffer = allocate_memory(TYPE_PAF, 256 * 3);
	_bufferBlock = allocate_memory(TYPE_PAF, kBufferBlockSize);
	
	_file.seek(num * 4, SEEK_SET);
	_videoOffset = _file.readUint32();
	_file.seek(_videoOffset, SEEK_SET);
	memset(&_pafHdr, 0, sizeof(_pafHdr));
	if (!readPafHeader()) { unload(); return; }

	uint8_t *buffer = allocate_memory(TYPE_PAF, kPageBufferSize * 4 + 256 * 4);
	if (!buffer) { unload(); return; }
	for (int i = 0; i < 4; ++i)
		_pageBuffers[i] = buffer + i * kPageBufferSize;

	_demuxVideoFrameBlocks = (uint8_t *)allocate_memory(TYPE_PAF,
		_pafHdr.maxVideoFrameBlocksCount * _pafHdr.readBufferSize);

	_pafHdr.maxAudioFrameBlocksCount = 0;
#ifdef SOUND
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
	slScrAutoDisp(NBG1ON);
	lwram_cut = current_lwram;
//	num=kPafAnimation_CanyonAndyFallingCannon;
	if (_videoNum != num) preload(num);
	if (_videoNum == num) { _playedMask |= 1 << num; mainLoop(); }
}

void PafPlayer::unload(int num) {
	if (lwram_cut)
		current_lwram = lwram_cut;
	hwram_work_paf = _video->_shadowLayer;

	if (_videoNum < 0) return;
	memset(_pageBuffers, 0, sizeof(_pageBuffers));
	_demuxVideoFrameBlocks = 0;
	_demuxAudioFrameBlocks = 0;
	memset(&_pafHdr, 0, sizeof(_pafHdr));
	_videoNum = -1;
#ifdef SOUND
	while (_audioQueue) {
		PafAudioQueue *next = _audioQueue->next;
		free(_audioQueue->buffer); free(_audioQueue);
		_audioQueue = next;
	}
	_audioQueueTail = 0;
#endif
	memset(_video->_backgroundLayer, 0, Video::W * Video::H);
	g_system->copyRectWidescreen(Video::W, Video::H, _video->_backgroundLayer, _video->_palette);
	slScrAutoDisp(NBG0ON|NBG1ON);
}

bool PafPlayer::readPafHeader() {
	_file.read(_bufferBlock, kBufferBlockSize);
	_pafHdr.frameDuration            = READ_LE_UINT32(_bufferBlock + 0x88);
	_pafHdr.startOffset              = READ_LE_UINT32(_bufferBlock + 0xA4);
	_pafHdr.preloadFrameBlocksCount  = READ_LE_UINT32(_bufferBlock + 0x9C);
	_pafHdr.readBufferSize           = READ_LE_UINT32(_bufferBlock + 0x98);
	if(_pafHdr.readBufferSize != kBufferBlockSize)
		return false;
	_pafHdr.framesCount              = READ_LE_UINT32(_bufferBlock + 0x84);
	if (_pafHdr.framesCount <= 0) return false;
	_pafHdr.maxVideoFrameBlocksCount = READ_LE_UINT32(_bufferBlock + 0xA8);
	_pafHdr.maxAudioFrameBlocksCount = READ_LE_UINT32(_bufferBlock + 0xAC);
	_pafHdr.frameBlocksCount         = READ_LE_UINT32(_bufferBlock + 0xA0);
	if (_pafHdr.frameBlocksCount <= 0) return false;
	_pafHdr.frameBlocksCountTable  = readPafHeaderTable(_pafHdr.framesCount);
	_pafHdr.framesOffsetTable      = readPafHeaderTable(_pafHdr.framesCount);
	_pafHdr.frameBlocksOffsetTable = readPafHeaderTable(_pafHdr.frameBlocksCount);
	return _pafHdr.frameBlocksCountTable != 0
	    && _pafHdr.framesOffsetTable      != 0
	    && _pafHdr.frameBlocksOffsetTable != 0;
}

uint32_t *PafPlayer::readPafHeaderTable(int count) {
	uint32_t *dst = (uint32_t *)allocate_memory(TYPE_PAFHEAD, count * sizeof(uint32_t));
	if (!dst) return 0;
	for (int i = 0; i < count; ++i) dst[i] = _file.readUint32();
	const int align = (count * 4) & 0x7FF;
	if (align != 0) _file.seek(0x800 - align, SEEK_CUR);
	return dst;
}

// =============================================================================
// Helpers mask
// =============================================================================

FORCE_INLINE void pafCopy4x4v(uint8_t *dst, const uint8_t *src) {
	store4_a(dst,       load4_u16(src));
	store4_a(dst + 256, load4_u16(src + 256));
	store4_a(dst + 512, load4_u16(src + 512));
	store4_a(dst + 768, load4_u16(src + 768));
}

static void pafCopySrcMask(uint8_t mask, uint8_t *dst, const uint8_t *src) {
	if (mask & 0x8) dst[0] = src[0];
	if (mask & 0x4) dst[1] = src[1];
	if (mask & 0x2) dst[2] = src[2];
	if (mask & 0x1) dst[3] = src[3];
}

static void pafCopyColorMask(uint8_t mask, uint8_t *dst, uint8_t color) {
	if (mask & 0x8) dst[0] = color;
	if (mask & 0x4) dst[1] = color;
	if (mask & 0x2) dst[2] = color;
	if (mask & 0x1) dst[3] = color;
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

FORCE_INLINE uint8_t *fastOffset(uint8_t **pages, const uint8_t *src) {
	const int x    =  src[1] & 0x7F;
	const int page =  src[0] >> 6;
	const int y    = ((src[0] << 1) | (src[1] >> 7)) & 0x7F;
	return pages[page] + (((y << 8) + x) << 1);
}

#include <stdint.h>
#define READ_LE_UINT16(p) ((uint16_t)((p)[0] | ((p)[1] << 8)))

void PafPlayer::decodeVideoFrame(const uint8_t *src) {
	const uint8_t *base = src;
	const int code = *src++;
	if (code & 0x20) {
		for (int i = 0; i < 4; ++i)
			memset(_pageBuffers[i], 0, kPageBufferSize);
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
	switch (code & 0xF) {
	case 0: decodeVideoFrameOp0(base, src, code); break;
	case 1: decodeVideoFrameOp1(src);             break;
	case 2: decodeVideoFrameOp2(src);             break;
	case 4: decodeVideoFrameOp4(src);             break;
	}
}

// =============================================================================
// Op0
// =============================================================================

void PafPlayer::decodeVideoFrameOp0(const uint8_t *base, const uint8_t *src, uint8_t code) {
	uint8_t **p = _pageBuffers;

	// ── 1. HORIZONTAL ─────────────────────────────────────────────────────────
	const uint8_t *v = src;
	int n = *v++;

	if (n) {
		if (code & 0x10) {
			int a = (v - base) & 3;
			if (a) v += 4 - a;
		}
		for (int i = 0; i < n; ++i) {
			const uint8_t  hi  = v[0];
			const uint8_t  lo  = v[1];
			const uint8_t  idx = hi >> 6;
			const uint32_t off = (((((hi << 1) | (lo >> 7)) & 0x7F) << 8)
			                       | (lo & 0x7F)) << 1;
			uint8_t *db = p[idx] + off;
			uint32_t o  = (lo & 0x7F) << 1;
			uint32_t e  = READ_LE_UINT16(v + 2) + o;
			v += 4;

			uint8_t *d0 = db;
			uint8_t *d1 = db + 256;
			uint8_t *d2 = db + 512;
			uint8_t *d3 = db + 768;

			while (o < e) {
				store4_a(d0, load4_any(v));
				store4_a(d1, load4_any(v +  4));
				store4_a(d2, load4_any(v +  8));
				store4_a(d3, load4_any(v + 12));
				v += 16;
				d0 += 4; d1 += 4; d2 += 4; d3 += 4;
				if ((++o & 0x3F) == 0) {
					d0 += 768; d1 += 768; d2 += 768; d3 += 768;
				}
			}
		}
	}

	// ── 2. VERTICAL ───────────────────────────────────────────────────────────
	{
		uint8_t       *d = _pageBuffers[_currentPageBuffer];
		const uint8_t *s = v;
		for (int y = 0; y < 192; y += 4, d += 768) {
			for (int x = 0; x < 256; x += 4, d += 4, s += 2) {
				const uint8_t  b0  = s[0], b1 = s[1];
				const uint8_t  idx = b0 >> 6;
				const uint32_t off = (((((b0 << 1) | (b1 >> 7)) & 0x7F) << 8)
				                       | (b1 & 0x7F)) << 1;
				const uint8_t *t = p[idx] + off;
				copy4x4_tile(
					d,       d + 256, d + 512, d + 768,
					t,       t + 256, t + 512, t + 768);
			}
		}
	}

	// ── 3. OP ─────────────────────────────────────────────────────────────────
	const uint8_t *op = v + 6144;
	/*if (op >= _demuxVideoFrameBlocks + (_pafHdr.maxVideoFrameBlocksCount * _pafHdr.readBufferSize)) {
		emu_printf("OP OOB!\n");
		return;
	}*/
	uint32_t sz = READ_LE_UINT16(op);
	op += 4;
	uint8_t       *d    = _pageBuffers[_currentPageBuffer];
	const uint8_t *src2 = op + sz;

	for (int y = 0; y < kVideoHeight; y += 4, d += kVideoWidth * 3) {
		for (int x = 0; x < kVideoWidth; x += 4, d += 4) {
			const char *q = updateSequences[(x & 4) ? (*op++ & 15) : (*op >> 4)];
			uint8_t k;
			while ((k = *q++)) {
				uint8_t *d0 = d + 512, *d1;
				const uint8_t *s2;
				uint8_t m, c;
				switch (k) {
					case 2: d0 = d;
					case 3: c  = *src2++;
					case 4: m  = *src2++;
					        d1 = d0 + 256;
					        pafCopyColorMask(m >> 4, d0, c);
					        pafCopyColorMask(m & 15, d1, c);
					        break;
					case 5: d0 = d;
					case 6: s2 = fastOffset(_pageBuffers, src2); src2 += 2;
					case 7: m  = *src2++;
					        d1 = d0 + 256;
					        pafCopySrcMask(m >> 4, d0, s2 + (d0 - d));
					        pafCopySrcMask(m & 15, d1, s2 + (d1 - d));
					        break;
				}
			}
		}
	}
}

// =============================================================================
// Op1 / Op2 / Op4
// =============================================================================

FORCE_INLINE void PafPlayer::decodeVideoFrameOp1(const uint8_t *src) {
	memcpy(_pageBuffers[_currentPageBuffer], src + 2, kVideoWidth * kVideoHeight);
}

FORCE_INLINE void PafPlayer::decodeVideoFrameOp2(const uint8_t *src) {
	const int page = *src++;
	if (page != _currentPageBuffer)
		memcpy(_pageBuffers[_currentPageBuffer], _pageBuffers[page], kVideoWidth * kVideoHeight);
}

void PafPlayer::decodeVideoFrameOp4(const uint8_t *src) {
	uint8_t       *dst = _pageBuffers[_currentPageBuffer];
	src += 2;
	const uint8_t *end = dst + (kVideoWidth * kVideoHeight);

	while (dst < end) {
		const int8_t code = (int8_t)*src++;
		if (code < 0) {
			int            count   = 1 - code;
			const uint8_t  color   = *src++;
			const uint32_t color32 = (uint32_t)color
			                       | ((uint32_t)color <<  8)
			                       | ((uint32_t)color << 16)
			                       | ((uint32_t)color << 24);
			while (((uintptr_t)dst & 3) && count > 0) { *dst++ = color; --count; }
			while (count >= 4) { store4_a(dst, color32); dst += 4; count -= 4; }
			while (count > 0)  { *dst++ = color; --count; }
		} else {
			int count = code + 1;
			while (((uintptr_t)dst & 3) && count > 0) { *dst++ = *src++; --count; }
			while (count >= 4) {
				store4_a(dst, load4_any(src));
				dst += 4; src += 4; count -= 4;
			}
			while (count > 0) { *dst++ = *src++; --count; }
		}
	}
}

// =============================================================================
// Audio
// =============================================================================

#ifdef SOUND
static void decodeAudioFrame2205(const uint8_t *src, int len, int16_t *dst, int volume) {
	static const int offset = 256 * sizeof(int16_t);
	for (int i = 0; i < len * 2; ++i) {
		const int16_t sample = READ_LE_UINT16(src + src[offset + i] * sizeof(int16_t));
		dst[i] = CLIP((sample * volume + 64) >> 7, -32768, 32767);
	}
}

void PafPlayer::decodeAudioFrame(const uint8_t *src, uint32_t offset, uint32_t size) {
	assert(size == _pafHdr.readBufferSize);
	if (offset != _audioBufferOffsetWr) {
		assert(offset == 0);
		_audioBufferOffsetWr = 0;
		_audioBufferOffsetRd = 0;
	}
	_audioBufferOffsetWr = offset + size;
	const int count = (_audioBufferOffsetWr - _audioBufferOffsetRd) / kAudioStrideSize;
	if (count != 0) {
		PafAudioQueue *sq = (PafAudioQueue *)malloc(sizeof(PafAudioQueue));
		if (sq) {
			sq->offset = 0;
			sq->size   = count * kAudioSamples * 2;
			sq->buffer = (int16_t *)malloc(sq->size * sizeof(int16_t));
			if (sq->buffer) {
				for (int i = 0; i < count; ++i)
					decodeAudioFrame2205(src + _audioBufferOffsetRd + i * kAudioStrideSize,
					                     kAudioSamples,
					                     sq->buffer + i * kAudioSamples * 2, _volume);
			}
			sq->next = 0;
			g_system->lockAudio();
			if (_audioQueueTail) _audioQueueTail->next = sq;
			else { assert(!_audioQueue); _audioQueue = sq; }
			_audioQueueTail = sq;
			g_system->unlockAudio();
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
			free(_audioQueue->buffer); free(_audioQueue);
			_audioQueue = next;
		}
		samples -= count;
	}
	if (!_audioQueue) _audioQueueTail = 0;
}

static void mixAudio(void *userdata, int16_t *buf, int len) {
	((PafPlayer *)userdata)->mix(buf, len);
}
#endif

//#define SLOT_SIZE      262144   // taille _demuxVideoFrameBlocks = 128 × 2048
//#define ASYNCH_MAX     145408   //
#define FRAMES_PER_READ 6
#define NUM_BUFFERS     2
#define ASYNCH_MAX     24576 * FRAMES_PER_READ    // cap empirique asynchInit
#define BUFFER_SIZE    (ASYNCH_MAX + SECTOR_SIZE)  // +2048 pour le secteur en cache

static uint32_t calcBatch(const PafHeader &hdr, int start,
                           uint32_t &blocksCount, int &framesBatchCount) {
	blocksCount      = 0;
	framesBatchCount = 0;
	uint32_t total   = 0;
	for (int j = start; j < (int)hdr.framesCount && framesBatchCount < FRAMES_PER_READ; j++) {
		if (j > start && hdr.framesOffsetTable[j] == 0) break;        // wrap circulaire
		uint32_t needed = hdr.frameBlocksCountTable[j] * hdr.readBufferSize;
		if (total + needed > ASYNCH_MAX) break;                        // cap asynchInit
//		if (hdr.framesOffsetTable[j] + 6144 >= SLOT_SIZE) break;      // OP OOB
		blocksCount      += hdr.frameBlocksCountTable[j];
		total            += needed;
		framesBatchCount++;
	}
	return total;
}

struct PafAsyncCtx {
	uint8_t  *buffers[NUM_BUFFERS];
	int       readBuf;
	int       nextWaitFrame;
	uint32_t  totalBytes;
	uint32_t  blocksCount;
	uint32_t  nextBlocksCount;
	uint8_t  *data;
	bool      active;
};

FORCE_INLINE uint32_t calcNextBatch(const PafHeader &hdr, int start,
                               uint32_t &blocksOut) {
	blocksOut = 0;
	uint32_t total = 0;
	for (int j = 0; j < FRAMES_PER_READ && (start + j) < (int)hdr.framesCount; j++) {
		blocksOut += hdr.frameBlocksCountTable[start + j];
		total     += hdr.frameBlocksCountTable[start + j] * hdr.readBufferSize;
	}
	return total;
}

static void pafSwapBatch(PafAsyncCtx &ctx, File &file, const PafHeader &hdr) {
	int r = file.asynchWait(ctx.buffers[ctx.readBuf], (Sint32)ctx.totalBytes);
	ctx.data       = ctx.buffers[ctx.readBuf] + (r - (int)ctx.totalBytes);
	ctx.blocksCount = ctx.nextBlocksCount;  // blocs du batch qu'on vient de recevoir
	ctx.readBuf    = 1 - ctx.readBuf;

	int next = ctx.nextWaitFrame + FRAMES_PER_READ;
	if (next < (int)hdr.framesCount) {
		uint32_t nextBC = 0;
		ctx.totalBytes    = calcNextBatch(hdr, next, nextBC);
		ctx.nextBlocksCount = nextBC;
		ctx.nextWaitFrame  = next;
		file.asynchInit(ctx.buffers[ctx.readBuf], ctx.totalBytes);
	} else {
		ctx.active = false;
	}
}

static void pafDemuxBlocks(PafAsyncCtx &ctx, const PafHeader &hdr,
                            uint8_t *demux, int &currentFrameBlock) {
	uint32_t count = ctx.blocksCount;
	while (count > 0) {
		const uint32_t entry     = hdr.frameBlocksOffsetTable[currentFrameBlock];
		const uint32_t dstOffset = entry & ~(1u << 31);
		if (!(entry & (1u << 31))) {
			if (dstOffset + hdr.readBufferSize <= hdr.maxVideoFrameBlocksCount * hdr.readBufferSize)
				memcpy(demux + dstOffset, ctx.data, hdr.readBufferSize);
		}
		++currentFrameBlock;
		--count;
		ctx.data += hdr.readBufferSize;
	}
	ctx.blocksCount = 0;
}

void PafPlayer::mainLoop() {
	_file.seek(_videoOffset + _pafHdr.startOffset, SEEK_SET);

	for (int i = 0; i < 4; ++i)
		memset(_pageBuffers[i], 0, kPageBufferSize);
	memset(_paletteBuffer, 0, sizeof(_paletteBuffer));
	g_system->setPalette(_paletteBuffer, 256, 6);
	_paletteChanged    = true;
	_currentPageBuffer = 0;
	int currentFrameBlock = 0;

	PafAsyncCtx ctx;
	ctx.buffers[0] = (uint8_t *)hwram_work_paf;
//	ctx.buffers[1] = (uint8_t *)hwram_work_paf+24576+2048;
	ctx.buffers[1] = (uint8_t *)current_lwram;

	ctx.readBuf    = 1;
	ctx.active     = (_pafHdr.framesCount > 1);

	if(_pafHdr.framesCount !=41)
	{
		uint32_t preloadBytes = _pafHdr.preloadFrameBlocksCount * _pafHdr.readBufferSize;
		int r = _file.batchRead(ctx.buffers[0], preloadBytes);
		ctx.data        = ctx.buffers[0] + (r - (int)preloadBytes);
		ctx.blocksCount = _pafHdr.preloadFrameBlocksCount;
	}
	else
	{
		uint32_t preloadBlocks = _pafHdr.preloadFrameBlocksCount + _pafHdr.frameBlocksCountTable[0];

		uint32_t preloadBytes  = preloadBlocks * _pafHdr.readBufferSize;
		int r = _file.batchRead(ctx.buffers[0], preloadBytes);
		ctx.data        = ctx.buffers[0] + (r - (int)preloadBytes);
		ctx.blocksCount = preloadBlocks;
	}

	ctx.nextWaitFrame = 1;
	if (ctx.active) {
		ctx.totalBytes = calcNextBatch(_pafHdr, 1, ctx.nextBlocksCount);
		_file.asynchInit(ctx.buffers[ctx.readBuf], ctx.totalBytes);
	}

#ifdef DEBUG
	static uint8_t last_frame_z = 0xFF;
	static char dbgbuf[8];
#endif

#ifdef DEBUG2
	uint32_t t_async    = 0;
	uint32_t t_memcpy   = 0;
	uint32_t t_decode   = 0;
	uint32_t t_copyrect = 0;
	uint32_t t_pal      = 0;
#endif
	const uint32_t frameMs   = 100;
	uint32_t       frameTime = g_system->getTimeStamp();

	for (int i = 0; i < (int)_pafHdr.framesCount; ++i) {
#ifdef DEBUG2
		uint32_t t0 = g_system->getTimeStamp();
#endif
		if (i == ctx.nextWaitFrame && ctx.active)
			pafSwapBatch(ctx, _file, _pafHdr);
#ifdef DEBUG2
		uint32_t t1 = g_system->getTimeStamp();
		t_async += t1 - t0;
#endif
		pafDemuxBlocks(ctx, _pafHdr, _demuxVideoFrameBlocks, currentFrameBlock);
#ifdef DEBUG2
		uint32_t t2 = g_system->getTimeStamp();
		t_memcpy += t2 - t1;
#endif
		decodeVideoFrame(_demuxVideoFrameBlocks + _pafHdr.framesOffsetTable[i]);
#ifdef DEBUG2
		uint32_t t3 = g_system->getTimeStamp();
		t_decode += t3 - t2;
#endif
		g_system->copyRect(0, 0, kVideoWidth, kVideoHeight,
		                   _pageBuffers[_currentPageBuffer], kVideoWidth);
#ifdef DEBUG2
		uint32_t t4 = g_system->getTimeStamp();
		t_copyrect += t4 - t3;
#endif
		if (_paletteChanged) {
			_paletteChanged = false;
			g_system->setPalette(_paletteBuffer, 256, 6);
			g_system->updateScreen(false);
		}


#ifdef DEBUG2
		uint32_t t5 = g_system->getTimeStamp();
		t_pal += t5 - t4;
		if ((i % 10) == 9) {
			emu_printf("f=%d as=%u mc=%u de=%u cr=%u pa=%u\n", i, t_async/10, t_memcpy / 10, t_decode / 10, t_copyrect / 10, t_pal / 10);
			t_async = t_memcpy = t_decode = t_copyrect = t_pal = 0;
		}
#endif

#ifdef DEBUG
		if (frame_z != last_frame_z) {
			last_frame_z = frame_z;
			dbgbuf[0] = '0' + (frame_z / 10);
			dbgbuf[1] = '0' + (frame_z % 10);
			dbgbuf[2] = 0;
		}
		_video->drawString(dbgbuf, (Video::W - 24), 0, 2, (uint8 *)VDP2_VRAM_A0);
#endif
//emu_printf("fps %d\n", frame_z);

		if (g_system->inp.quit
		 || g_system->inp.keyPressed(SYS_INP_ESC)
		 || g_system->inp.keyPressed(SYS_INP_RUN))
			break;

		frame_x++;
		++_currentPageBuffer;
		_currentPageBuffer &= 3;

		frameTime += frameMs;
		uint32_t now = g_system->getTimeStamp();
		if (frameTime > now)
			g_system->sleep(frameTime - now);
		else {
#ifdef DEBUG2
			emu_printf("behind=%d\n", (int)(now - frameTime));
#endif
			frameTime = now;
		}
	}
	if (ctx.active)
		_file.asynchWait(ctx.buffers[ctx.readBuf], (Sint32)ctx.totalBytes);

	unload();
	closePaf(_fs, &_file);
}

void PafPlayer::setCallback(const PafCallback *pafCb) {
	if (pafCb) _pafCb = *pafCb;
	else memset(&_pafCb, 0, sizeof(_pafCb));
}