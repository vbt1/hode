#pragma GCC optimize ("O2")
/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */
extern "C" {
#include <sl_def.h>
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
extern unsigned char frame_x;
extern unsigned char frame_z;
}
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
//emu_printf("openPaf\n");	
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
//emu_printf("PafPlayer\n");
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
	_video = new Video();
}

PafPlayer::~PafPlayer() {
	unload();
	closePaf(_fs, &_file);
}

void PafPlayer::setVolume(int volume) {
	_volume = volume;
}

void PafPlayer::preload(int num) {
	//emu_printf("preload %d\n", num);
	assert(num >= 0 && num < kMaxVideosCount);
	if (_videoNum != num) {
		unload(_videoNum);
		_videoNum = num;
	}
	_file.seek(num * 4, SEEK_SET);
	_videoOffset = _file.readUint32();
	_file.seek(_videoOffset, SEEK_SET);
	memset(&_pafHdr, 0, sizeof(_pafHdr));
	if (!readPafHeader()) {
		//emu_printf("unload paf\n");
		unload();
		return;
	}
	/*
    if (((uintptr_t)cs1ram & 3) != 0) {
        cs1ram += 4 - ((uintptr_t)cs1ram & 3);
    }	
	*/
	uint8_t *buffer = (uint8_t *)cs1ram;//calloc(kPageBufferSize * 4 + 256 * 4, 1);
	if (!buffer) {
		//emu_printf("preloadPaf() Unable to allocate page buffers\n");
		unload();
		return;
	}
	cs1ram+=(kPageBufferSize * 4 + 256 * 4);
	
	for (int i = 0; i < 4; ++i) {
		_pageBuffers[i] = buffer + i * kPageBufferSize;
	}
	_demuxVideoFrameBlocks = (uint8_t *)cs1ram;//calloc(_pafHdr.maxVideoFrameBlocksCount, _pafHdr.readBufferSize);
	cs1ram+=(_pafHdr.maxVideoFrameBlocksCount* _pafHdr.readBufferSize);
	
	_pafHdr.maxAudioFrameBlocksCount = 0; // vbt : on enleve le son
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
}

void PafPlayer::play(int num) {
	//emu_printf("play %d\n", num);
	if (_videoNum != num) {
		preload(num);
	}
	if (_videoNum == num) {
		_playedMask |= 1 << num;
		mainLoop();
	}
}

void PafPlayer::unload(int num) {
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
#endif
}

bool PafPlayer::readPafHeader() {
	//emu_printf("readPafHeader\n");
	static const char *kSignature = "Packed Animation File V1.0\n(c) 1992-96 Amazing Studio\n";
	_file.read(_bufferBlock, kBufferBlockSize);
	if (memcmp(_bufferBlock, kSignature, strlen(kSignature)) != 0) {
		//emu_printf("readPafHeader() Unexpected signature\n");
		return false;
	}
	_pafHdr.frameDuration = READ_LE_UINT32(_bufferBlock + 0x88);
	_pafHdr.startOffset = READ_LE_UINT32(_bufferBlock + 0xA4);
	_pafHdr.preloadFrameBlocksCount = READ_LE_UINT32(_bufferBlock + 0x9C);
	_pafHdr.readBufferSize = READ_LE_UINT32(_bufferBlock + 0x98);
	assert(_pafHdr.readBufferSize == kBufferBlockSize);
	_pafHdr.framesCount = READ_LE_UINT32(_bufferBlock + 0x84);
	if (_pafHdr.framesCount <= 0) {
		//emu_printf("readPafHeader() Invalid number of frames %d\n", _pafHdr.framesCount);
		return false;
	}
	_pafHdr.maxVideoFrameBlocksCount = READ_LE_UINT32(_bufferBlock + 0xA8);
	_pafHdr.maxAudioFrameBlocksCount = READ_LE_UINT32(_bufferBlock + 0xAC);
	_pafHdr.frameBlocksCount = READ_LE_UINT32(_bufferBlock + 0xA0);
	if (_pafHdr.frameBlocksCount <= 0) {
		//emu_printf("readPafHeader() Invalid number of blocks %d\n", _pafHdr.frameBlocksCount);
		return false;
	}
	_pafHdr.frameBlocksCountTable = readPafHeaderTable(_pafHdr.framesCount);
	_pafHdr.framesOffsetTable = readPafHeaderTable(_pafHdr.framesCount);
	_pafHdr.frameBlocksOffsetTable = readPafHeaderTable(_pafHdr.frameBlocksCount);
	return _pafHdr.frameBlocksCountTable != 0 && _pafHdr.framesOffsetTable != 0 && _pafHdr.frameBlocksOffsetTable != 0;
}

uint32_t *PafPlayer::readPafHeaderTable(int count) {
//	uint32_t *dst = (uint32_t *)malloc(count * sizeof(uint32_t));
	uint32_t *dst = (uint32_t *)cs1ram;
	cs1ram+=(count * sizeof(uint32_t));
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
		const int index = src[0];
		const int count = (src[1] + 1) * 3;
		assert(index * 3 + count <= 768);
		src += 2;
		memcpy(_paletteBuffer + index * 3, src, count);
		_paletteChanged = true;
		src += count;
	}
//emu_printf("decode %d\n", code & 0xF);	
	
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

static void pafCopy4x4h(uint8_t *dst, const uint8_t *src) {
/*	for (int i = 0; i < 4; ++i) {
		memcpy(dst, src, 4);
		src += 4;
		dst += 256;
	}
*/
	memcpy(dst, src, 4);
	memcpy(dst + 256, src + 4, 4);
	memcpy(dst + 512, src + 8, 4);
	memcpy(dst + 768, src + 12, 4);
}

/* // vbt : incorrect
static void pafCopy4x4h(uint8_t *dst, const uint8_t *src) {
	for (int i = 0; i < 4; ++i) {
		*(uint32_t*)dst = *(const uint32_t*)src;
		src += 4;
		dst += 256;
	}
}
*/
/*
static void pafCopy4x4h(uint8_t *dst, const uint8_t *src) {
	uint32_t t0, t1, t2, t3;
	t0 = *(const uint32_t*)src;
	t1 = *(const uint32_t*)(src + 4);
	t2 = *(const uint32_t*)(src + 8);
	t3 = *(const uint32_t*)(src + 12);
	*(uint32_t*)dst = t0;
	*(uint32_t*)(dst + 256) = t1;
	*(uint32_t*)(dst + 512) = t2;
	*(uint32_t*)(dst + 768) = t3;
}
*/
static void pafCopy4x4v(uint8_t *dst, const uint8_t *src) {
/*	for (int i = 0; i < 4; ++i) {
		memcpy(dst, src, 4);
		src += 256;
		dst += 256;
	}
*/
	memcpy(dst, src, 4);
	memcpy(dst + 256, src + 256, 4);
	memcpy(dst + 512, src + 512, 4);
	memcpy(dst + 768, src + 768, 4);
}

/*
static void pafCopy4x4v(uint8_t *dst, const uint8_t *src) {
	*(uint32_t*)dst = *(const uint32_t*)src;
	*(uint32_t*)(dst + 256) = *(const uint32_t*)(src + 256);
	*(uint32_t*)(dst + 512) = *(const uint32_t*)(src + 512);
	*(uint32_t*)(dst + 768) = *(const uint32_t*)(src + 768);
}
*/
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

inline uint8_t *PafPlayer::getVideoPageOffset(uint16_t val) {
	const int x = val & 0x7F; val >>= 7;
	const int y = val & 0x7F; val >>= 7;
	return _pageBuffers[val] + (y * kVideoWidth + x) * 2;
}

void PafPlayer::decodeVideoFrameOp0(const uint8_t *base, const uint8_t *src, uint8_t code) {
	const int count = *src++;
	if (count != 0) {
		if ((code & 0x10) != 0) {
			const int align = (src - base) & 3;
			if (align != 0) {
				src += 4 - align;
			}
		}
		for (int i = 0; i < count; ++i) {
			uint8_t *dst = getVideoPageOffset((src[0] << 8) | src[1]);
			uint32_t offset = (src[1] & 0x7F) * 2;
			const uint32_t end = READ_LE_UINT16(src + 2) + offset; src += 4;
			do {
				++offset;
				pafCopy4x4h(dst, src);
				src += 16;
				if ((offset & 0x3F) == 0) {
					dst += kVideoWidth * 3;
				}
				dst += 4;
			} while (offset < end);
		}
	}

	uint8_t *dst = _pageBuffers[_currentPageBuffer];
	for (int y = 0; y < kVideoHeight; y += 4, dst += kVideoWidth * 3) {
		for (int x = 0; x < kVideoWidth; x += 4, dst += 4) {
			const uint8_t *src2 = getVideoPageOffset((src[0] << 8) | src[1]); src += 2;
			pafCopy4x4v(dst, src2);
		}
	}

	const uint32_t opcodesSize = READ_LE_UINT16(src); src += 4;
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
				uint32_t offset = kVideoWidth * 2;
				switch (code) {
				case 2:
					offset = 0;
					/* fall-through */
				case 3:
					color = *src++;
					/* fall-through */
				case 4:
					mask = *src++;
					pafCopyColorMask(mask >> 4, dst + offset, color);
					offset += kVideoWidth;
					pafCopyColorMask(mask & 15, dst + offset, color);
					break;
				case 5:
					offset = 0;
					/* fall-through */
				case 6:
					src2 = getVideoPageOffset((src[0] << 8) | src[1]); src += 2;
					/* fall-through */
				case 7:
					mask = *src++;
					pafCopySrcMask(mask >> 4, dst + offset, src2 + offset);
					offset += kVideoWidth;
					pafCopySrcMask(mask & 15, dst + offset, src2 + offset);
					break;
				}
			}
		}
	}
}

void PafPlayer::decodeVideoFrameOp1(const uint8_t *src) {
	memcpy(_pageBuffers[_currentPageBuffer], src + 2, kVideoWidth * kVideoHeight);
}

void PafPlayer::decodeVideoFrameOp2(const uint8_t *src) {
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
		//emu_printf("Unexpected offset 0x%x wr 0x%x rd 0x%x num %d\n", offset, _audioBufferOffsetWr, _audioBufferOffsetRd, _videoNum);
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
			//emu_printf("PafAudioQueue() Unable to allocate %d bytes\n", count * sizeof(uint32_t));
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

void PafPlayer::mainLoop() {
//emu_printf("mainLoop paf\n");
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
	_video->_font = (uint8_t *)0x25e6df94;	// vbt : hardcoded font buffer address vram vdp2
	static uint8_t last_frame_z = 0xFF;
	static char buffer[8];

//emu_printf("framesCount %d\n", _pafHdr.framesCount);
	for (int i = 0; i < (int)_pafHdr.framesCount; ++i) {
		// read buffering blocks
		blocksCountForFrame += _pafHdr.frameBlocksCountTable[i];
		while (blocksCountForFrame != 0) {
			_file.read(_bufferBlock, _pafHdr.readBufferSize);
			const uint32_t dstOffset = _pafHdr.frameBlocksOffsetTable[currentFrameBlock] & ~(1 << 31);
			if (_pafHdr.frameBlocksOffsetTable[currentFrameBlock] & (1 << 31)) {
#ifdef SOUND
				assert(dstOffset + _pafHdr.readBufferSize <= _pafHdr.maxAudioFrameBlocksCount * _pafHdr.readBufferSize);
				memcpy(_demuxAudioFrameBlocks + dstOffset, _bufferBlock, _pafHdr.readBufferSize);
				decodeAudioFrame(_demuxAudioFrameBlocks, dstOffset, _pafHdr.readBufferSize);
#endif
			} else {
				assert(dstOffset + _pafHdr.readBufferSize <= _pafHdr.maxVideoFrameBlocksCount * _pafHdr.readBufferSize);
				memcpy(_demuxVideoFrameBlocks + dstOffset, _bufferBlock, _pafHdr.readBufferSize);
			}
			++currentFrameBlock;
			--blocksCountForFrame;
		}
		// decode video data
		decodeVideoFrame(_demuxVideoFrameBlocks + _pafHdr.framesOffsetTable[i]);

		if (_pafCb.frameProc) {
			_pafCb.frameProc(_pafCb.userdata, i, _pageBuffers[_currentPageBuffer]);
		} else {
			g_system->copyRect((int)0, (int)0, (int)kVideoWidth, (int)kVideoHeight, _pageBuffers[_currentPageBuffer], (int)kVideoWidth);
		}
		if (_paletteChanged) {
			_paletteChanged = false;
			g_system->setPalette(_paletteBuffer, 256, 6);
		}

		if (frame_z != last_frame_z) {
			last_frame_z = frame_z;

			buffer[0] = '0' + (frame_z / 10);
			buffer[1] = '0' + (frame_z % 10);
			buffer[2] = 0;
		}
		_video->drawString(buffer, (Video::W - 24), 0, 2, (uint8 *)VDP2_VRAM_A0);

		g_system->updateScreen(false);
		g_system->processEvents();
		if (g_system->inp.quit || g_system->inp.keyPressed(SYS_INP_ESC)) {
			break;
		}

		const int delay = MAX<int>(10, frameTime - g_system->getTimeStamp());
		g_system->sleep(delay);
		frameTime = g_system->getTimeStamp() + frameMs;
		frame_x++;
		// set next decoding video page
		++_currentPageBuffer;
		_currentPageBuffer &= 3;
	}

	if (_pafCb.endProc) {
		_pafCb.endProc(_pafCb.userdata);
	}
#ifdef SOUND
	// restore audio callback
	if (_demuxAudioFrameBlocks) {
		g_system->setAudioCallback(prevAudioCb);
	}
#endif
	unload();
}

void PafPlayer::setCallback(const PafCallback *pafCb) {
	if (pafCb) {
		_pafCb = *pafCb;
	} else {
		memset(&_pafCb, 0, sizeof(_pafCb));
	}
}
