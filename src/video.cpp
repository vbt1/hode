#pragma GCC optimize ("O2")
/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */
//#define USE_SPRITE 1
extern "C" {
#include <sl_def.h>
}
#include "game.h"
#include "menu.h"
#include "video.h"
//#include "mdec.h"
#include "system.h"
#include "systemstub.h"
#include "util.h"

extern Uint32 position_vram;
extern SAT_sprite _sprData[4];
static const bool kUseShadowColorLut = false;
//static const bool kUseShadowColorLut = true; // vbt on utilise la lut

Video::Video() {
//	//emu_printf("video init\n");
	_displayShadowLayer = false;
	_drawLine.x1 = 0;
	_drawLine.y1 = 0;
	_drawLine.x2 = W - 1;
	_drawLine.y2 = H - 1;
#if 1
//	_shadowLayer = (uint8_t *)malloc(W * H + 1); // projectionData offset can be equal to W * H
	_shadowLayer = allocate_memory (TYPE_LAYER, W * H + 1);
//	_frontLayer = (uint8_t *)malloc(W * H);
	_frontLayer = allocate_memory (TYPE_LAYER, W * H);
//	_backgroundLayer = (uint8_t *)malloc(W * H);
	_backgroundLayer = allocate_memory (TYPE_LAYER, W * H);
	
	if (kUseShadowColorLut) {
//		_shadowColorLookupTable = (uint8_t *)malloc(256 * 256);
		_shadowColorLookupTable = allocate_memory (TYPE_SHADWLUT, 256 * 256);
	} else {
		_shadowColorLookupTable = 0;
	}
//	_shadowScreenMaskBuffer = (uint8_t *)malloc(256 * 192 * 2 + 256 * 4);
	_shadowScreenMaskBuffer = allocate_memory (TYPE_SCRMASKBUF, 256 * 192 * 2 + 256 * 4);
	for (int i = 144; i < 256; ++i) {
		_shadowColorLut[i] = i;
	}
	_transformShadowBuffer = 0;
	_transformShadowLayerDelta = 0;
#ifdef PSX
	memset(&_mdec, 0, sizeof(_mdec));
	_backgroundPsx = 0;
#endif
#endif
}

Video::~Video() {
	//emu_printf("free video\n");
//	free(_shadowLayer);
//	free(_frontLayer);
//	free(_backgroundLayer);
//	free(_shadowColorLookupTable);
//	free(_shadowScreenMaskBuffer);
#ifdef PSX
	free(_mdec.planes[kOutputPlaneY].ptr);
	free(_mdec.planes[kOutputPlaneCb].ptr);
	free(_mdec.planes[kOutputPlaneCr].ptr);
#endif
}
#ifdef PSX
void Video::initPsx() {
	static const int w = (W + 15) & ~15;
	static const int h = (H + 15) & ~15;
	static const int w2 = w / 2;
	static const int h2 = h / 2;
	_mdec.planes[kOutputPlaneY].ptr = (uint8_t *)malloc(w * h);
	_mdec.planes[kOutputPlaneY].pitch = w;
	_mdec.planes[kOutputPlaneCb].ptr = (uint8_t *)malloc(w2 * h2);
	_mdec.planes[kOutputPlaneCb].pitch = w2;
	_mdec.planes[kOutputPlaneCr].ptr = (uint8_t *)malloc(w2 * h2);
	_mdec.planes[kOutputPlaneCr].pitch = w2;
}
#endif
static int colorBrightness(int r, int g, int b) {
	return (r + g * 2) * 19 + b * 7;
}

void Video::updateGamePalette(const uint16_t *pal) {
	for (int i = 0; i < 256 * 3; ++i) {
		_palette[i] = pal[i] >> 8;
	}
	g_system->setPalette(_palette, 256, 8);
}

void Video::updateGameDisplay(uint8_t *buf) {
	g_system->copyRect(0, 0, W, H, buf, 256);
#ifdef PSX
	if (_mdec.planes[kOutputPlaneY].ptr) {
		updateYuvDisplay();
	}
#endif
}
#ifdef PSX
void Video::updateYuvDisplay() {
	g_system->copyYuv(Video::W, Video::H, _mdec.planes[0].ptr, _mdec.planes[0].pitch, _mdec.planes[1].ptr, _mdec.planes[1].pitch, _mdec.planes[2].ptr, _mdec.planes[2].pitch);
}

void Video::copyYuvBackBuffer() {
	if (_backgroundPsx) {
		_mdec.x = 0;
		_mdec.y = 0;
		_mdec.w = W;
		_mdec.h = H;
		decodeMDEC(_backgroundPsx, W * H * sizeof(uint16_t), 0, 0, W, H, &_mdec);
	}
}

void Video::clearYuvBackBuffer() {
	_backgroundPsx = 0;
}
#endif
void Video::updateScreen() {
	g_system->updateScreen(true);
}

void Video::clearBackBuffer() {
	g_system->fillRect(0, 0, W, H, CLEAR_COLOR);
}

void Video::clearPalette() {
	memset(_palette, 0, sizeof(_palette));
	g_system->clearPalette();
}

void Video::SAT_loadTitleSprites(const DatSpritesGroup *spriteGroup, const uint8_t *ptr)
{
	ptr += spriteGroup->firstFrameOffset;

	for (uint32_t i = 0; i < spriteGroup->count; ++i) {
		const uint16_t size = READ_LE_UINT16(ptr + 2);
		const uint16_t w_raw = READ_LE_UINT16(ptr + 4);
		const uint16_t w = (w_raw + 7) & ~7;
		const uint16_t h = READ_LE_UINT16(ptr + 6);
////emu_printf("num %d w%d h%d\n", i,w,h);
		TEXTURE tx = TEXDEF(w, h, position_vram);
		uint8_t *src = (uint8_t *)ptr+8;
		uint8_t dst[104*19]; //104 pour demo fr
		memset(dst,0x00, w*h);
		SAT_decodeSPR(src, dst, 0, 0, 0, w_raw, h);
		memcpy((void*)(SpriteVRAM + (tx.CGadr << 3)), (void*)dst, w*h);
		position_vram += w*h;

		_sprData[i].cgaddr = tx.CGadr;
		_sprData[i].size   = (w/8)<<8|h;
		_sprData[i].x      = ptr[0];
		_sprData[i].y      = ptr[1] - 112;
		ptr += size + 2;
	}
}

void Video::SAT_decodeSPR(const uint8_t *src, uint8_t *dst, int x, int y, uint8_t flags, uint16_t spr_w, uint16_t spr_h) {

	const int y2 = y + spr_h - 1;
	const int x2 = x + spr_w - 1;

	if (flags & kSprHorizFlip) {
		x = x2;
	}
	if (flags & kSprVertFlip) {
		y = y2;
	}
	const int xOrig = x;
	const uint16_t w = (spr_w + 7) & ~7;

	while (1) {
		uint8_t *p = dst + y * w + x;
		int code = *src++;
		int count = code & 0x3F;
		int clippedCount = count;

		switch (code >> 6) {
		case 0:
			if ((flags & (kSprHorizFlip | kSprClipLeft | kSprClipRight)) == 0) {
				memcpy(p, src, clippedCount);
				x += count;
			} else if (flags & kSprHorizFlip) {
				for (int i = 0; i < clippedCount; ++i) {
					if (x - i >= 0 && x - i < spr_w) {
						p[-i] = src[i];
					}
				}
				x -= count;
			} else {
				for (int i = 0; i < clippedCount; ++i) {
					if (x + i >= 0 && x + i < spr_w) {
						p[i] = src[i];
					}
				}
				x += count;
			}
			src += count;
			break;
		case 1:
			code = *src++;
			if ((flags & (kSprHorizFlip | kSprClipLeft | kSprClipRight)) == 0) {
				memset(p, code, clippedCount);
				x += count;
			} else if (flags & kSprHorizFlip) {
				for (int i = 0; i < clippedCount; ++i) {
					if (x - i >= 0 && x - i < spr_w) {
						p[-i] = code;
					}
				}
				x -= count;
			} else {
				for (int i = 0; i < clippedCount; ++i) {
					if (x + i >= 0 && x + i < spr_w) {
						p[i] = code;
					}
				}
				x += count;
			}
			break;
		case 2:
			if (count == 0) {
				count = *src++;
			}
			if (flags & kSprHorizFlip) {
				x -= count;
			} else {
				x += count;
			}
			break;
		case 3:
			if (count == 0) {
				count = *src++;
				if (count == 0) {
					return;
				}
			}
			if (flags & kSprVertFlip) {
				y -= count;
			} else {
				y += count;
			}
			if (flags & kSprHorizFlip) {
				x = xOrig - *src++;
			} else {
				x = xOrig + *src++;
			}
			break;
		}
	}
}

void Video::SAT_cleanSprites()
{
emu_printf("SAT_cleanSprites\n");
	SPRITE user_sprite;
	user_sprite.CTRL= FUNC_End;
	user_sprite.PMOD=0;
	user_sprite.SRCA=0;
	user_sprite.COLR=0;

	user_sprite.SIZE=0;
	user_sprite.XA=0;
	user_sprite.YA=0;

	user_sprite.XB=0;
	user_sprite.YB=0;
	user_sprite.GRDA=0;

	slSetSprite(&user_sprite, toFIXED2(240));	// à remettre // ennemis et objets
	slSynch(); // vbt à remettre
}

void Video::decodeBG(const uint8_t *src, uint8_t *dst, int x, int y, uint8_t flags, uint16_t spr_w, uint16_t spr_h) {
	if (y >= H) {
		return;
	} else if (y < 0) {
		flags |= kSprClipTop;
	}
	const int y2 = y + spr_h - 1;
	if (y2 < 0) {
		return;
	} else if (y2 >= H) {
		flags |= kSprClipBottom;
	}

	if (x >= W) {
		return;
	} else if (x < 0) {
		flags |= kSprClipLeft;
	}
	const int x2 = x + spr_w - 1;
	if (x2 < 0) {
		return;
	} else if (x2 >= W) {
		flags |= kSprClipRight;
	}

	if (flags & kSprHorizFlip) {
		x = x2;
	}
	if (flags & kSprVertFlip) {
		y = y2;
	}
	const int xOrig = x;
	while (1) {
		uint8_t *p = dst + y * W + x;
		int code = *src++;
		int count = code & 0x3F;
		int clippedCount = count;
		if (y < 0 || y >= H) {
			clippedCount = 0;
		}
		switch (code >> 6) {
		case 0:
			if ((flags & (kSprHorizFlip | kSprClipLeft | kSprClipRight)) == 0) {
				memcpy(p, src, clippedCount);
				x += count;
			} else if (flags & kSprHorizFlip) {
				for (int i = 0; i < clippedCount; ++i) {
					if (x - i >= 0 && x - i < W) {
						p[-i] = src[i];
					}
				}
				x -= count;
			} else {
				for (int i = 0; i < clippedCount; ++i) {
					if (x + i >= 0 && x + i < W) {
						p[i] = src[i];
					}
				}
				x += count;
			}
			src += count;
			break;
		case 1:
			code = *src++;
			if ((flags & (kSprHorizFlip | kSprClipLeft | kSprClipRight)) == 0) {
				memset(p, code, clippedCount);
				x += count;
			} else if (flags & kSprHorizFlip) {
				for (int i = 0; i < clippedCount; ++i) {
					if (x - i >= 0 && x - i < W) {
						p[-i] = code;
					}
				}
				x -= count;
			} else {
				for (int i = 0; i < clippedCount; ++i) {
					if (x + i >= 0 && x + i < W) {
						p[i] = code;
					}
				}
				x += count;
			}
			break;
		case 2:
			if (count == 0) {
				count = *src++;
			}
			if (flags & kSprHorizFlip) {
				x -= count;
			} else {
				x += count;
			}
			break;
		case 3:
			if (count == 0) {
				count = *src++;
				if (count == 0) {
					return;
				}
			}
			if (flags & kSprVertFlip) {
				y -= count;
			} else {
				y += count;
			}
			if (flags & kSprHorizFlip) {
				x = xOrig - *src++;
			} else {
				x = xOrig + *src++;
			}
			break;
		}
	}
}

void Video::decodeSPR(const uint8_t *src, uint8_t *dst,
                      int x, int y, uint8_t flags,
                      uint16_t spr_w, uint16_t spr_h)
{
	if (y >= H) return;
	else if (y < 0) flags |= kSprClipTop;

	const int y2 = y + spr_h - 1;
	if (y2 < 0) return;
	else if (y2 >= H) flags |= kSprClipBottom;

	if (x >= W) return;
	else if (x < 0) flags |= kSprClipLeft;

	const int x2 = x + spr_w - 1;
	if (x2 < 0) return;
	else if (x2 >= W) flags |= kSprClipRight;

#ifdef USE_SPRITE
	const int xAnchor = x;
	const int yAnchor = y;
#endif
	
	if (flags & kSprHorizFlip) x = x2;
	if (flags & kSprVertFlip)  y = y2;

#ifdef USE_SPRITE
	const uint16_t w_raw = spr_w;
	const uint16_t w     = (w_raw + 7) & ~7;
	const uint16_t h     = spr_h;
	const uint16_t size  = w * h;

	if (position_vram + size >= 0x79000)
		position_vram = 0;

	TEXTURE tx = TEXDEF(w, h, position_vram);
	uint8_t *dst2 = (uint8_t *)SpriteVRAM + (tx.CGadr << 3);
	position_vram += size;

	SPRITE user_sprite;
	user_sprite.PMOD = CL256Bnk | ECdis | 0x0800;
	user_sprite.COLR = 0;
	user_sprite.SIZE = (w / 8) << 8 | h;
	user_sprite.CTRL = (FUNC_Sprite | _ZmLT);
	user_sprite.XA   = ((xAnchor * 5) >> 1) - 320;
	user_sprite.YA   = yAnchor - 112 + 16;
	user_sprite.XB   = (w * 5) >> 1;
	user_sprite.YB   = spr_h;
	user_sprite.GRDA = 0;
	user_sprite.SRCA = tx.CGadr;

	slSetSprite(&user_sprite, toFIXED2(240));

	memset(dst2, 0x00, size);

	int ix2 = (flags & kSprHorizFlip) ? (w_raw - 1) : 0;
	int iy2 = (flags & kSprVertFlip)  ? (h - 1) : 0;
	const int ix2Orig = ix2;
#endif
	const int xOrig = x;
	
	// Pre-compute commonly used flag checks
	const bool hFlip = (flags & kSprHorizFlip) != 0;
	const bool vFlip = (flags & kSprVertFlip) != 0;
#ifndef USE_SPRITE
	const uint8_t clipFlags = flags & (kSprHorizFlip | kSprClipLeft | kSprClipRight);
#endif

	while (1) {
#ifndef USE_SPRITE
		uint8_t *p = dst + y * W + x;
#else
		uint8_t *p2 = dst2 + iy2 * w + ix2;
#endif
		int code  = *src++;
		int count = code & 0x3F;

#ifndef USE_SPRITE
		int clippedCount = count;
		if (y < 0 || y >= H)
			clippedCount = 0;
#endif

		switch (code >> 6) {

		/* ---------- COPY ---------- */
		case 0:
#ifndef USE_SPRITE
			if (clipFlags == 0) {
				memcpy(p, src, clippedCount);
				x += count;
			} else if (hFlip) {
				for (int i = 0; i < clippedCount; ++i)
					if (x - i >= 0 && x - i < W) p[-i] = src[i];
				x -= count;
			} else {
				for (int i = 0; i < clippedCount; ++i)
					if (x + i >= 0 && x + i < W) p[i] = src[i];
				x += count;
			}
#else
			if (!hFlip) {
				memcpy(p2, src, count);
				ix2 += count;
			} else {
				for (int i = 0; i < count; ++i)
					p2[-i] = src[i];
				ix2 -= count;
			}
#endif
			src += count;
			break;

		/* ---------- FILL ---------- */
		case 1:
			code = *src++;
#ifndef USE_SPRITE
			if (clipFlags == 0) {
				memset(p, code, clippedCount);
				x += count;
			} else if (hFlip) {
				for (int i = 0; i < clippedCount; ++i)
					if (x - i >= 0 && x - i < W) p[-i] = code;
				x -= count;	
			} else {
				for (int i = 0; i < clippedCount; ++i)
					if (x + i >= 0 && x + i < W) p[i] = code;
				x += count;
			}
#else
			if (!hFlip) {
				memset(p2, code, count);
				ix2 += count;
			} else {
				for (int i = 0; i < count; ++i)
					p2[-i] = code;
				ix2 -= count;
			}
#endif
			break;

		/* ---------- SKIP X ---------- */
		case 2:
			if (count == 0) count = *src++;
#ifdef USE_SPRITE
			ix2 += hFlip ? -count : count;
#else
			x   += hFlip ? -count : count;
#endif
			break;

		/* ---------- NEW LINE ---------- */
		case 3:
			if (count == 0) {
				count = *src++;
				if (count == 0) return;
			}

#ifdef USE_SPRITE
			iy2 += vFlip ? -count : count;
#else
			y   += vFlip ? -count : count;
#endif
			uint8_t dx = *src++;
#ifdef USE_SPRITE
			ix2 = hFlip ? (ix2Orig - dx) : (ix2Orig + dx);
#else
			x   = hFlip ? (xOrig - dx) : (xOrig + dx);
#endif
			break;
		}
	}
}

void Video::decodeRLE(const uint8_t *src, uint8_t *dst, int size) {
//	emu_printf("decode RLE\n");
	uint8_t *dstEnd = dst + size;
	
	while (dst < dstEnd) {
		int8_t code = *src++;
		int count;
		
		if (code < 0) {
			// RLE run - fill
			count = 1 - code;
			const uint8_t color = *src++;
			memset(dst, color, count);
		} else {
			// Literal run - copy
			count = code + 1;
			memcpy(dst, src, count);
			src += count;
		}
		dst += count;
	}
}

// https://en.wikipedia.org/wiki/Cohen%E2%80%93Sutherland_algorithm
int Video::computeLineOutCode(int x, int y) {
	int mask = 0;
	if (y > _drawLine.y2) mask |= 1 << 24;
	if (x > _drawLine.x2) mask |= 1 << 16;
	if (y < _drawLine.y1) mask |= 1 <<  8;
	if (x < _drawLine.x1) mask |= 1;
	return mask;
}

bool Video::clipLineCoords(int &x1, int &y1, int &x2, int &y2) {
	int mask1 = computeLineOutCode(x2, y2);
	while (1) {
		const int mask2 = computeLineOutCode(x1, y1);
		if (mask2 == 0 && mask1 == 0) {
			break;
		}
		if ((mask1 & mask2) != 0) {
			return true;
		}
		if (mask2 & 1) { // (x < _drawLine.x1)
			y1 += (y2 - y1) * (_drawLine.x1 - x1) / (x2 - x1);
			x1 = _drawLine.x1;
			continue;
		}
		if (mask2 & 0x100) { // (y < _drawLine.y1)
			x1 += (x2 - x1) * (_drawLine.y1 - y1) / (y2 - y1);
			y1 = _drawLine.y1;
			continue;
		}
		if (mask2 & 0x10000) { // (x > _drawLine.x2)
			y1 += (y2 - y1) * (_drawLine.x2 - x1) / (x2 - x1);
			x1 = _drawLine.x2;
			continue;
		}
		if (mask2 & 0x1000000) { // (y > _drawLine.y2)
			x1 += (x2 - x1) * (_drawLine.y2 - y1) / (y2 - y1);
			y1 = _drawLine.y2;
			continue;
		}
		SWAP(x1, x2);
		SWAP(y1, y2);
		assert(mask2 == 0);
		mask1 = 0;
	}
	return false;
}

void Video::drawLine(int x1, int y1, int x2, int y2, uint8_t color) {
	
	if (clipLineCoords(x1, y1, x2, y2)) {
		return;
	}
#ifdef USE_SPRITE	
	SPRITE line;
	line.CTRL = FUNC_Line | _ZmLT;
//	line.CTRL = FUNC_Line;
	line.PMOD = CL256Bnk | 0x0800;
	line.COLR = color;
	line.XA = ((x1 * 5) >> 1) - 320;
//	line.XA = x1 - 160;
	line.YA = y1 - 112+16;
	line.XB = ((x2 * 5) >> 1) - 320;
//	line.XB = x2 - 160;
	line.YB = y2 - 112+16;

	slSetSprite(&line, toFIXED2(240));	
#else	
	assert(x1 >= _drawLine.x1 && x1 <= _drawLine.x2);
	assert(y1 >= _drawLine.y1 && y1 <= _drawLine.y2);
	assert(x2 >= _drawLine.x1 && x2 <= _drawLine.x2);
	assert(y2 >= _drawLine.y1 && y2 <= _drawLine.y2);
	int dstPitch = W;
	int dx = x2 - x1;
	if (dx == 0) {
		int dy = y2 - y1;
		if (dy < 0) {
			y1 += dy;
			dy = -dy;
		}
		uint8_t *dst = _frontLayer + y1 * W + x1;
		for (int i = 0; i <= dy; ++i) {
			*dst = color;
			dst += dstPitch;
		}
		return;
	}
	if (dx < 0) {
		x1 += dx;
		dx = -dx;
		SWAP(y1, y2);
	}
	uint8_t *dst = _frontLayer + y1 * W + x1;
	int dy = y2 - y1;
	if (dy == 0) {
		memset(dst, color, dx);
		return;
	}
	if (dy < 0) {
		dy = -dy;
		dstPitch = -dstPitch;
	}
	int step = 0;
	if (dx > dy) {
		SWAP(dx, dy);
		dx *= 2;
		const int stepInc = dy * 2;
		step -= stepInc;
		for (int i = 0; i <= dy; ++i) {
			*dst = color;
			step += dx;
			if (step >= 0) {
				step -= stepInc;
				dst += dstPitch;
			}
			++dst;
		}
	} else {
		dx *= 2;
		const int stepInc = dy * 2;
		step -= stepInc;
		for (int i = 0; i <= dy; ++i) {
			*dst = color;
			step += dx;
			if (step >= 0) {
				step -= stepInc;
				++dst;
			}
			dst += dstPitch;
		}
	}
#endif
}

#if 0
static uint8_t lookupColor(uint8_t a, uint8_t b, const uint8_t *lut) {
	return (a >= 144 && b < 144) ? lut[b] : b;
}

void Video::applyShadowColors(int x, int y, int src_w, int src_h, int dst_pitch, int src_pitch, uint8_t *dst1, uint8_t *dst2, uint8_t *src1, uint8_t *src2) {
// vbt comparer les 2 versions
	
	assert(dst1 == _shadowLayer);
	assert(dst2 == _frontLayer);
	
	dst2 += y * dst_pitch + x;
	
	for (int j = 0; j < src_h; ++j) {
		// Simple 2x unroll - reduces loop overhead without complexity
		int i = 0;
		for (; i < (src_w & ~1); i += 2) {
			uint16_t offset0 = READ_LE_UINT16(src1); src1 += 2;
			uint16_t offset1 = READ_LE_UINT16(src1); src1 += 2;
			
			uint8_t a0 = dst1[offset0];
			uint8_t b0 = dst2[i];
			uint8_t a1 = dst1[offset1];
			uint8_t b1 = dst2[i + 1];
			
			dst2[i] = (a0 >= 144 && b0 < 144) ? _shadowColorLut[b0] : b0;
			dst2[i + 1] = (a1 >= 144 && b1 < 144) ? _shadowColorLut[b1] : b1;
		}
		
		// Handle odd pixel
		if (i < src_w) {
			uint16_t offset = READ_LE_UINT16(src1); src1 += 2;
			dst2[i] = lookupColor(dst1[offset], dst2[i], _shadowColorLut);
		}
		
		dst2 += dst_pitch;
	}
/*
	assert(dst1 == _shadowLayer);
	assert(dst2 == _frontLayer);
	// src1 == projectionData
	// src2 == shadowPalette

	dst2 += y * dst_pitch + x;
	for (int j = 0; j < src_h; ++j) {
		for (int i = 0; i < src_w; ++i) {
			int offset = READ_LE_UINT16(src1); src1 += 2;
			assert(offset <= W * H);
			if (kUseShadowColorLut) {
				// build lookup offset
				//   msb : _shadowLayer[ _projectionData[ (x, y) ] ]
				//   lsb : _frontLayer[ (x, y) ]
				offset = (dst1[offset] << 8) | dst2[i];

				// lookup color matrix
				//   if msb < 144 : _frontLayer.color
				//   if msb >= 144 : if _frontLayer.color < 144 ? shadowPalette[ _frontLayer.color ] : _frontLayer.color
				dst2[i] = _shadowColorLookupTable[offset];
			} else {
				dst2[i] = lookupColor(_shadowLayer[offset], dst2[i], _shadowColorLut);
			}
		}
		dst2 += dst_pitch;
	}
*/
}
#else

static inline uint8_t lookupColor(uint8_t a, uint8_t b, const uint8_t *lut) {
	return (a >= 144 && b < 144) ? lut[b] : b;
}

void Video::applyShadowColors(int x, int y, int src_w, int src_h, int dst_pitch, int src_pitch, uint8_t *dst1, uint8_t *dst2, uint8_t *src1, uint8_t *src2) {
    assert(dst1 == _shadowLayer);
    assert(dst2 == _frontLayer);

    dst2 += y * dst_pitch + x;

    for (int j = 0; j < src_h; ++j) {
        // Simple 2x unroll - reduces loop overhead without complexity
        int i = 0;
        for (; i < (src_w & ~1); i += 2) {
            uint16_t offset0 = READ_LE_UINT16(src1); src1 += 2;
            uint16_t offset1 = READ_LE_UINT16(src1); src1 += 2;

            uint8_t a0 = dst1[offset0];
            uint8_t b0 = dst2[i];
            uint8_t a1 = dst1[offset1];
            uint8_t b1 = dst2[i + 1];

            dst2[i] = (a0 >= 144 && b0 < 144) ? _shadowColorLut[b0] : b0;
            dst2[i + 1] = (a1 >= 144 && b1 < 144) ? _shadowColorLut[b1] : b1;
        }

        // Handle odd pixel
        if (i < src_w) {
            uint16_t offset = READ_LE_UINT16(src1); src1 += 2;
            dst2[i] = lookupColor(dst1[offset], dst2[i], _shadowColorLut);
        }

        dst2 += dst_pitch;
    }
}
#endif

void Video::buildShadowColorLookupTable(const uint8_t *src, uint8_t *dst) {
	if (kUseShadowColorLut) {
		assert(dst == _shadowColorLookupTable);
		// 256x256
		//   0..143 : 0..255
		// 144..255 : src[0..143] 144..255
		for (int i = 0; i < 144; ++i) {
			for (int j = 0; j < 256; ++j) {
				*dst++ = j;
			}
		}
		for (int i = 0; i < 112; ++i) {
			memcpy(dst, src, 144);
			dst += 144;
			for (int j = 0; j < 112; ++j) {
				*dst++ = 144 + j;
			}
		}
	}
	memcpy(_shadowColorLut, src, 144); // indexes 144-256 are not remapped
	if (0) {
		// lookup[a * 256 + b]
		//
		// if (a < 144) return b;
		// else if (b < 144) return src[b]
		// else return b;
		//
		// return (a >= 144 && b < 144) ? src[b] : b;
		for (int a = 0; a < 256; ++a) {
			for (int b = 0; b < 256; ++b) {
				const int res1 = (a >= 144 && b < 144) ? src[b] : b;
				const int res2 = dst[a * 256 + b - 65536];
				if (res1 != res2) {
					fprintf(stdout, "buildShadowColorLookupTable a %d b %d res1 %d res2 %d\n", a, b, res1, res2);
				}
				assert(res1 == res2);
			}
		}
	}
}

// returns the font index
uint8_t Video::findStringCharacterFontIndex(uint8_t chr) const {
	// bugfix: the original code seems to ignore the last 3 entries
	for (int i = 0; i < 39 * 2; i += 2) {
		if (_fontCharactersTable[i] == chr) {
			return _fontCharactersTable[i + 1];
		}
	}
	return 255;
}

void Video::drawStringCharacter(int x, int y, uint8_t chr, uint8_t color, uint8_t *dst) {
	const uint8_t *p = _font + ((chr & 15) + (chr >> 4) * 256) * 16;
	dst += y * 512 + x;
	for (int j = 0; j < 16; ++j) {
		for (int i = 0; i < 16; ++i) {
			if (p[i] != 0) {
				dst[i] = color;
			}
		}
		p += 16 * 16;
		dst += 512;
	}
}
#if 1
void Video::drawString(const char *s, int x, int y, uint8_t color, uint8_t *dst) {
	for (int i = 0; s[i]; ++i) {
		uint8_t chr = s[i];
		if (chr != ' ') {
			if (chr >= 'a' && chr <= 'z') {
				chr += 'A' - 'a';
			}
			chr = findStringCharacterFontIndex(chr);
			if (chr == 255) {
				continue;
			}
			drawStringCharacter(x, y, chr, color, dst);
		}
		x += 8;
	}
}

uint8_t Video::findWhiteColor() const {
	uint8_t color = 0;
	int whiteQuant = 0;
	for (int i = 0; i < 256; ++i) {
		const int q = colorBrightness(_palette[i * 3], _palette[i * 3 + 1], _palette[i * 3 + 2]);
		if (q > whiteQuant) {
			whiteQuant = q;
			color = i;
		}
	}
	return color;
}
#endif

#ifdef PSX
void Video::decodeBackgroundPsx(const uint8_t *src, int size, int w, int h, int x, int y) {
	if (size < 0) {
		_backgroundPsx = src;
	} else {
		_mdec.x = x;
		_mdec.y = y;
		_mdec.w = w;
		_mdec.h = h;
		decodeMDEC(src, size, 0, 0, w, h, &_mdec);
	}
}

void Video::decodeBackgroundOverlayPsx(const uint8_t *src, int x, int y) {
	const uint16_t size = READ_LE_UINT16(src + 2);
	if (size > 6) {
		const int count = READ_LE_UINT32(src + 4);
		assert(count >= 1 && count <= 3);
		int offset = 8;
		for (int i = 0; i < count && offset < size; ++i) {
			_mdec.x = x + src[offset];
			_mdec.y = y + src[offset + 1];
			const int len = READ_LE_UINT16(src + offset + 2);
			_mdec.w = src[offset + 4] * 16;
			_mdec.h = src[offset + 5] * 16;
			const int mbOrderLength = src[offset + 6];
			const int mbOrderOffset = src[offset + 7];
			const uint8_t *data = &src[offset + 8];
			if (mbOrderOffset == 0) {
				decodeMDEC(data, len - 8, 0, 0, _mdec.w, _mdec.h, &_mdec);
			} else {
				// different macroblocks order
				decodeMDEC(data + mbOrderOffset, len - 8 - mbOrderOffset, data, mbOrderLength, _mdec.w, _mdec.h, &_mdec);
			}
			offset += len;
		}
		assert(offset == size + 2);
	}
}
#endif
