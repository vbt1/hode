#pragma GCC optimize ("Os")
/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */
//#define USE_SPRITE 1
//#define USE_FONT 1
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
//#define PRELOAD_ANDY 1
extern "C" {
extern Uint8 *hwram_work_paf;
extern Uint32 position_vram;
extern Uint32 position_vram_save;
#ifdef PRELOAD_ANDY
extern SAT_sprite andy_vdp2[284];
#endif
extern SAT_sprite _sprData[4];
}

//static const bool kUseShadowColorLut = false;
//static const bool kUseShadowColorLut = true; // vbt on utilise la lut

Video::Video() {
//emu_printf("Video\n");
	_displayShadowLayer = false;
	_drawLine.x1 = 0;
	_drawLine.y1 = 0;
	_drawLine.x2 = W - 1;
	_drawLine.y2 = H - 1;
#if 1
	if(hwram_work == 0)
	{
		hwram_work = allocate_memory (TYPE_HWRAM, 588000+116000+32000);
	//	emu_printf("--hwram_work start %p\n", hwram_work);
		hwram_work_paf   = hwram_work;
		_shadowLayer     = allocate_memory (TYPE_LAYER, W * H + 1);
		_frontLayer      = allocate_memory (TYPE_LAYER, W * H);
		_backgroundLayer = allocate_memory (TYPE_LAYER, W * H);
		_backgroundLayer2= allocate_memory (TYPE_LDIMG, W * H);
		_shadowScreenMaskBuffer = allocate_memory (TYPE_LAYER, 256 * 192 * 2 + 256 * 4); //99k
		_transformShadowBuffer = allocate_memory (TYPE_LAYER, 256 * 192 + 256); //49k
		
//	emu_printf("_shadow %p _front %p _back %p end %p\n", _shadowLayer, _frontLayer, _backgroundLayer, _backgroundLayer + W * H, hwram_work);
/*
		if (kUseShadowColorLut) {
	//		_shadowColorLookupTable = (uint8_t *)malloc(256 * 256);
			_shadowColorLookupTable = allocate_memory (TYPE_SHADWLUT, 256 * 256);
		} else {
			_shadowColorLookupTable = 0;
		}
*/
//		emu_printf("--hwram_work end %p size %d\n", hwram_work, (int)hwram_work-(int)hwram_work_paf);
//		hwram_work = hwram_work_paf; // vbt : on ne rend pas la ram !!!
	}

//	_shadowScreenMaskBuffer = (uint8_t *)malloc(256 * 192 * 2 + 256 * 4);
//	_shadowScreenMaskBuffer = allocate_memory (TYPE_SCRMASKBUF, 256 * 192 * 2 + 256 * 4);
	/*for (int i = 0; i < 112; ++i) {
		_shadowColorLut[i] = i + 144;
	}*/
//	_transformShadowBuffer = 0;
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

void Video::updateGamePalette(const uint8_t *pal) {
	memcpy(_palette, pal, 256 * 3);
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
		TEXTURE tx = TEXDEF(w, h, position_vram);
		uint8_t *src = (uint8_t *)ptr+8;
		uint8_t *dst = (uint8_t *)(SpriteVRAM + (tx.CGadr << 3));
		memset(dst,0x00, w*h);
		SAT_decodeSPR(src, dst, w_raw, h);
		position_vram += w*h;

		_sprData[i].cgaddr = tx.CGadr;
		_sprData[i].size   = (w/8)<<8|h;
		_sprData[i].x      = ptr[0];
		_sprData[i].y      = ptr[1] - 112;
		ptr += size + 2;
	}
}

void Video::SAT_decodeSPR(const uint8_t *src, uint8_t *dst, uint16_t spr_w, uint16_t spr_h) {
	int x = 0, y = 0;
	const uint16_t w = (spr_w + 7) & ~7;
	uint8_t *rowBase = dst;

    while (1) {
		const int code = *src++;
        const int count = code & 0x3F;
        const int op    = code & 0xC0;
        uint8_t  *p     = rowBase + x;

        if (op == 0x00) {                // copy
            for (int i = 0; i < count; ++i) {
                uint8_t val = src[i];
                p[i] = val - (val == 0); // branchless 0→255
            }
            x   += count;
            src += count;
        } else if (op == 0x40) {         // fill
            const uint8_t val = *src++;
            memset(p, val, count);
            x += count;
        } else if (op == 0x80) {         // skip x
            x += count ? count : *src++;
        } else {                         // new line
            if (count == 0) {
                const int n = *src++;
                if (n == 0) return;
                y += n;
            } else {
                y += count;
            }
            x        = *src++;
            rowBase  = dst + y * w;
        }
	}
}

void Video::SAT_cleanSprites()
{
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

void Video::decodeNBG(const Sprite *spr, uint8_t *dst) {
    const uint8_t *src   = spr->bitmapBits;
    int            x     = spr->xPos;
    int            y     = spr->yPos;
    const int      xOrig = x;

    while (1) {
        const int code  = *src++;
        const int count = code & 0x3F;
        const int op    = code & 0xC0;

        uint8_t *p      = dst + y * W + x;
//        const int clippedCount = ((unsigned)y < (unsigned)H) ? count : 0;

        if (op == 0x00) {
            memcpy(p, src, count);
            x   += count;
            src += count;
        } else if (op == 0x40) {
            const int val = *src++;
            memset(p, val, count);
            x += count;
        } else if (op == 0x80) {
            x += count ? count : *src++;
        } else {
            if (count == 0) {
                const int n = *src++;
                if (n == 0) return;
                y += n;
            } else {
                y += count;
            }
            x = xOrig + *src++;
        }
    }
}

void Video::decodeSPR(const Sprite *spr, uint8_t *bg, uint8_t *dst)
{
    const uint8_t  *src   = spr->bitmapBits;
    const int      xOrig = spr->xPos;
    const int      yOrig = spr->yPos;
    uint8_t        flags = (spr->num >> 0xE) & 3;
    const uint16_t spr_w = spr->w;
    const uint8_t  spr_h = spr->h;
#ifdef PRELOAD_ANDY
    const bool     compressed = !(spr->ptr->spriteNum == 0 && spr->type == 0);
#endif
	if (yOrig >= H) return;
	else if (yOrig < 0) flags |= kSprClipTop;
	const int y2 = yOrig + spr_h - 1;
	if (y2 < 0) return;
	else if (y2 >= H) flags |= kSprClipBottom;
	if (xOrig >= W) return;
	else if (xOrig < 0) flags |= kSprClipLeft;
	const int x2 = xOrig + spr_w - 1;
	if (x2 < 0) return;
	else if (x2 >= W) flags |= kSprClipRight;

	const bool hFlip = (flags & kSprHorizFlip) != 0;
	const bool vFlip = (flags & kSprVertFlip) != 0;
#ifdef PRELOAD_ANDY
	if (!compressed) {
		src = (uint8_t *)SpriteVRAM + (0x200+andy_vdp2[spr->ptr->currentSprite].cgaddr) * 8;
		const uint16_t spr_w_padded = (spr_w + 7) & ~7;
		for (int iy = 0; iy < spr_h; ++iy) {
			const int dstY = vFlip ? (y2 - iy) : (yOrig + iy);
			for (int ix = 0; ix < spr_w; ++ix) {
				const int srcX = hFlip ? (spr_w - 1 - ix) : ix;
				const int dstX = xOrig + ix;
				const uint8_t pixel = src[iy * spr_w_padded + srcX];
				if (dstY >= 0 && dstY < H && dstX >= 0 && dstX < W) {
					dst[dstY * W + dstX] = pixel;
//					if (bg && pixel == 0)
//						bg[dstY * W + dstX] = 0;
				}
			}
		}
		return;
	}
#endif
	/* ---- COMPRESSED ---- */
	int x = hFlip ? x2 : xOrig;
	int y = vFlip ? y2 : yOrig;
	const int xStart = x;

	const uint8_t clipFlags = flags & (kSprHorizFlip | kSprClipLeft | kSprClipRight);

	while (1) {
		uint8_t *p = dst + y * W + x;
		int code  = *src++;
		int count = code & 0x3F;
		int clippedCount = count;
		if (y < 0 || y >= H)
			clippedCount = 0;

		switch (code >> 6) {
		/* ---------- COPY ---------- */
		case 0:
			if (clipFlags == 0) {
				memcpy(p, src, clippedCount);
				if (bg && y >= 0 && y < H) {
					for (int i = 0; i < clippedCount; ++i)
						if (x + i >= 0 && x + i < W && src[i] == 0)
							bg[y * W + (x + i)] = 0;
				}
				x += count;
			} else if (hFlip) {
				for (int i = 0; i < clippedCount; ++i)
					if (x - i >= 0 && x - i < W) {
						p[-i] = src[i];
						if (bg && src[i] == 0)
							bg[y * W + (x - i)] = 0;
					}
				x -= count;
			} else {
				for (int i = 0; i < clippedCount; ++i)
					if (x + i >= 0 && x + i < W) {
						p[i] = src[i];
						if (bg && src[i] == 0)
							bg[y * W + (x + i)] = 0;
					}
				x += count;
			}
			src += count;
			break;
		/* ---------- FILL ---------- */
		case 1:
			code = *src++;
			if (clipFlags == 0) {
				memset(p, code, clippedCount);
				if (bg && code == 0 && y >= 0 && y < H) {
					for (int i = 0; i < clippedCount; ++i)
						if (x + i >= 0 && x + i < W)
							bg[y * W + (x + i)] = 0;
				}
				x += count;
			} else if (hFlip) {
				for (int i = 0; i < clippedCount; ++i)
					if (x - i >= 0 && x - i < W) {
						p[-i] = code;
						if (bg && code == 0)
							bg[y * W + (x - i)] = 0;
					}
				x -= count;
			} else {
				for (int i = 0; i < clippedCount; ++i)
					if (x + i >= 0 && x + i < W) {
						p[i] = code;
						if (bg && code == 0)
							bg[y * W + (x + i)] = 0;
					}
				x += count;
			}
			break;
		/* ---------- SKIP X ---------- */
		case 2:
			if (count == 0) count = *src++;
			x += hFlip ? -count : count;
			break;
		/* ---------- NEW LINE ---------- */
		case 3:
			if (count == 0) {
				count = *src++;
				if (count == 0) return;
			}
			y += vFlip ? -count : count;
			uint8_t dx = *src++;
			x = hFlip ? (xStart - dx) : (xStart + dx);
			break;
		}
	}
}
#if 0
void Video::decodeSPR(const Sprite *spr, uint8_t *dst)
{
    const uint8_t  *src     = spr->bitmapBits;
    int             x       = 0;
    int             y       = 0;
    uint8_t         flags   = ((uint16_t)spr->num >> 14) & 3; // logical shift
    const uint16_t  spr_w   = spr->w;
    const uint8_t   spr_h   = spr->h;
    const int       xAnchor = spr->xPos;
    const int       yAnchor = spr->yPos;
    const bool      hFlip   = (flags & kSprHorizFlip) != 0;
    const bool      vFlip   = (flags & kSprVertFlip)  != 0;
    const uint16_t  w       = (spr_w + 7) & ~7;

    const uint16_t size = w * spr_h;
    if (position_vram + size >= 0x79000)
        position_vram = 0;
    TEXTURE tx   = TEXDEF(w, spr_h, position_vram);
    uint8_t *dst2 = (uint8_t *)SpriteVRAM + (tx.CGadr << 3);
//	emu_printf("cgaddr %x vram %p pvram %x\n", tx.CGadr << 3,dst2,position_vram);
    position_vram += size;

    SPRITE user_sprite;
    user_sprite.PMOD = CL256Bnk | ECdis | 0x0800;
    user_sprite.COLR = 0;
    user_sprite.SIZE = (w / 8) << 8 | spr_h;
    user_sprite.XA   = ((xAnchor * 5) >> 1) - 319;
    user_sprite.YA   = yAnchor - 112 + 15;
    user_sprite.XB   = ((w * 5) >> 1);
    user_sprite.YB   = spr_h;
    user_sprite.GRDA = 0;
    user_sprite.SRCA = tx.CGadr;
    user_sprite.CTRL = FUNC_Sprite | _ZmLT;
		int pos = user_sprite.XA - (spr_w - 1 - (w - spr_w));
    if (hFlip) {
        user_sprite.CTRL |= (1 << 4);
		user_sprite.XA = (((xAnchor - (w - spr_w)) * 5) >> 1) - 319;
    }
    if (vFlip) {
        user_sprite.CTRL |= (1 << 5);
        user_sprite.YA   -= spr_h - 1;
    }

    slSetSprite(&user_sprite, toFIXED2(240));
    memset(dst2, 0x00, size);

    uint8_t *rowBase = dst2;

    while (1) {
        const int code  = *src++;
        const int count = code & 0x3F;
        const int op    = code & 0xC0;
        uint8_t  *p     = rowBase + x;

        if (op == 0x00) {                // copy
            for (int i = 0; i < count; ++i) {
                uint8_t val = src[i];
                p[i] = val - (val == 0); // branchless 0→255
            }
            x   += count;
            src += count;
        } else if (op == 0x40) {         // fill
            const uint8_t val = *src++;
            memset(p, val == 0 ? 255 : val, count);
            x += count;
        } else if (op == 0x80) {         // skip x
            x += count ? count : *src++;
        } else {                         // new line
            if (count == 0) {
                const int n = *src++;
                if (n == 0) return;
                y += n;
            } else {
                y += count;
            }
            x        = *src++;
            rowBase  = dst2 + y * w;     // multiply only here
        }
    }
}
#else
void Video::decodeSPR(const Sprite *spr, uint8_t *dst)
{
    const uint8_t  *src     = spr->bitmapBits;
    int             x       = 0;
    int             y       = 0;
    uint8_t         flags   = ((uint16_t)spr->num >> 14) & 3; // logical shift
    const uint16_t  spr_w   = spr->w;
    const uint8_t   spr_h   = spr->h;
    const int       xAnchor = spr->xPos;
    const int       yAnchor = spr->yPos;
    const bool      hFlip   = (flags & kSprHorizFlip) != 0;
    const bool      vFlip   = (flags & kSprVertFlip)  != 0;
    const uint16_t  w       = (spr_w + 7) & ~7;

    const uint16_t size = w * spr_h;


    SPRITE user_sprite;
    user_sprite.PMOD = CL256Bnk | ECdis | 0x0800;
    user_sprite.COLR = 0;
    user_sprite.SIZE = (w / 8) << 8 | spr_h;
    user_sprite.XA   = ((xAnchor * 5) >> 1) - 319;
    user_sprite.YA   = yAnchor - 112 + 15;
    user_sprite.XB   = ((w * 5) >> 1);
    user_sprite.YB   = spr_h;
    user_sprite.GRDA = 0;
    user_sprite.CTRL = FUNC_Sprite | _ZmLT;
		int pos = user_sprite.XA - (spr_w - 1 - (w - spr_w));
    if (hFlip) {
        user_sprite.CTRL |= (1 << 4);
		user_sprite.XA = (((xAnchor - (w - spr_w)) * 5) >> 1) - 319;
    }
    if (vFlip) {
        user_sprite.CTRL |= (1 << 5);
        user_sprite.YA   -= spr_h - 1;
    }
#ifdef PRELOAD_ANDY
	if(spr->ptr->spriteNum == 0 && spr->type == 0)
	{
//		emu_printf("it's andy !! num %d w %d h %d type %d\n", 
//		spr->ptr->currentSprite, w, spr_h, spr->type);
		user_sprite.SRCA = 0x200+andy_vdp2[spr->ptr->currentSprite].cgaddr;
		slSetSprite(&user_sprite, toFIXED2(240));
	}
	else
#endif
	{
//		emu_printf("it's not andy !! %d ptr %p\n",spr->num, spr->ptr);
		if (position_vram + size >= 0x79000)
			position_vram = position_vram_save;
		TEXTURE tx   = TEXDEF(w, spr_h, position_vram);
		user_sprite.SRCA = tx.CGadr;
	//	emu_printf("cgaddr %x vram %p pvram %x\n", tx.CGadr << 3,dst2,position_vram);
		position_vram += size;
		slSetSprite(&user_sprite, toFIXED2(240));
		uint8_t *dst2 = (uint8_t *)SpriteVRAM + (tx.CGadr << 3);
		memset(dst2, 0x00, size);

		uint8_t *rowBase = dst2;

		while (1) {
			const int code  = *src++;
			const int count = code & 0x3F;
			const int op    = code & 0xC0;
			uint8_t  *p     = rowBase + x;

			if (op == 0x00) {                // copy
				for (int i = 0; i < count; ++i) {
					uint8_t val = src[i];
					p[i] = val - (val == 0); // branchless 0→255
				}
				x   += count;
				src += count;
			} else if (op == 0x40) {         // fill
				const uint8_t val = *src++;
				memset(p, val == 0 ? 255 : val, count);
				x += count;
			} else if (op == 0x80) {         // skip x
				x += count ? count : *src++;
			} else {                         // new line
				if (count == 0) {
					const int n = *src++;
					if (n == 0) return;
					y += n;
				} else {
					y += count;
				}
				x        = *src++;
				rowBase  = dst2 + y * w;     // multiply only here
			}
		}
	}
}
#endif
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
//#ifdef USE_SPRITE
#if 1
	SPRITE line;
	line.CTRL = FUNC_Line | _ZmLT;
//	line.CTRL = FUNC_Line;
	line.PMOD = CL256Bnk | 0x0800 | ECdis | SPdis;
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

static uint8_t lookupColor(uint8_t a, uint8_t b, const uint8_t *lut) {
	return (a >= 144 && b < 144) ? lut[b] : b;
}

void Video::applyShadowColors(int x, int y, int src_w, int src_h, int dst_pitch, int src_pitch, uint8_t *dst1, uint8_t *dst2, uint8_t *src1, uint8_t *src2) {
	if (dst1 != _shadowLayer)     return;
	if (dst2 != _backgroundLayer) return;

	dst2 += y * dst_pitch + x;
	const uint8_t * const shadow = _shadowLayer;
	const uint8_t * const lut    = _shadowColorLut;
	const int limit = W * H;

	for (int j = 0; j < src_h; ++j) {
		int i = 0;
		for (; i <= src_w - 4; i += 4) {
			const uint16_t o0 = read_le16_aligned(src1 + 0);
			const uint16_t o1 = read_le16_aligned(src1 + 2);
			const uint16_t o2 = read_le16_aligned(src1 + 4);
			const uint16_t o3 = read_le16_aligned(src1 + 6);
			src1 += 8;

			// early shadow loads to hide random-access latency
			const uint8_t s0 = (o0 <= limit) ? shadow[o0] : 0;
			const uint8_t s1 = (o1 <= limit) ? shadow[o1] : 0;
			const uint8_t s2 = (o2 <= limit) ? shadow[o2] : 0;
			const uint8_t s3 = (o3 <= limit) ? shadow[o3] : 0;

			const uint8_t f0 = dst2[i+0];
			const uint8_t f1 = dst2[i+1];
			const uint8_t f2 = dst2[i+2];
			const uint8_t f3 = dst2[i+3];

			if (s0 >= 144 && f0 < 144) dst2[i+0] = lut[f0];
			if (s1 >= 144 && f1 < 144) dst2[i+1] = lut[f1];
			if (s2 >= 144 && f2 < 144) dst2[i+2] = lut[f2];
			if (s3 >= 144 && f3 < 144) dst2[i+3] = lut[f3];
		}
		// tail
		for (; i < src_w; ++i) {
			const uint16_t o = read_le16_aligned(src1); src1 += 2;
			const uint8_t s = (o <= limit) ? shadow[o] : 0;
			const uint8_t f = dst2[i];
			if (s >= 144 && f < 144) dst2[i] = lut[f];
		}
		dst2 += dst_pitch;
	}
}
#if 0
void Video::buildShadowColorLookupTable(const uint8_t *src, uint8_t *dst) {
	/*if (kUseShadowColorLut) {
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
	}*/
	memcpy(_shadowColorLut, src, 144); // indexes 144-256 are not remapped
/*
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
*/
}
#endif
#ifdef USE_FONT
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

void Video::drawString(const char *s, int x, int y, uint8_t color, uint8_t *dst) {
//emu_printf("drawString %s col %d x%d y%d\n", s, color, x, y);
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
