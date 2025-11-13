/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */
#include <sys/param.h>
#include "fileio.h"
#include "util.h"

#define GFS_BYTE_SCT(byte, sctsiz)  \
    ((Sint32)(((Uint32)(byte)) + ((Uint32)(sctsiz)) - 1) / ((Uint32)(sctsiz)))

static const bool kCheckSectorFileCrc = false;

#ifdef PSP
static const bool kSeekAbsolutePosition = true;
#else
static const bool kSeekAbsolutePosition = false;
#endif

#if 0
File::File()
	: _fp(0) {
}

File::~File() {
}
#endif
void File::setFp(GFS_FILE *fp) {
emu_printf("setFp\n");
	_fp = fp;
emu_printf("setFp %p\n", _fp);
}

void File::seekAlign(uint32_t pos) {
emu_printf("File::seek\n");
	sat_fseek(_fp, pos, SEEK_SET);
}

void File::seek(int pos, int whence) {
emu_printf("File::seek\n");
	if (kSeekAbsolutePosition && whence == SEEK_CUR) {
		pos += sat_ftell(_fp);
		whence = SEEK_SET;
	}
emu_printf("sat_fseek\n");
	sat_fseek(_fp, pos, whence);
}

int File::read(uint8_t *ptr, int size) {
emu_printf("sat_fread %d\n", size);
	return sat_fread(ptr, 1, size, _fp);
}
#if 0
uint8_t File::readByte() {
	uint8_t buf;
	read(&buf, 1);
	return buf;
}
#endif
uint16_t File::readUint16() {
	uint8_t buf[2];
	read(buf, 2);
	return READ_LE_UINT16(buf);
}

uint32_t File::readUint32() {
	uint8_t buf[4];
	read(buf, 4);
	
emu_printf("%d %d %d %d\n",buf[0],buf[1],buf[2],buf[3]);	
	
	return READ_LE_UINT32(buf);
}

SectorFile::SectorFile() {
	memset(_buf, 0, sizeof(_buf));
	_bufPos = 2044;
}

int fioAlignSizeTo2048(int size) {
	return ((size + 2043) / 2044) * 2048;
}

uint32_t fioUpdateCRC(uint32_t sum, const uint8_t *buf, uint32_t size) {
	assert((size & 3) == 0);
	for (uint32_t offset = 0; offset < size; offset += 4) {
		sum ^= READ_LE_UINT32(buf + offset);
	}
	return sum;
}

void SectorFile::refillBuffer(uint8_t *ptr) {
emu_printf("SectorFile::refillBuffer %p\n", ptr);
	if (ptr) {
		static const int kPayloadSize = kFioBufferSize - 4;
		const int size = sat_fread(ptr, 1, kPayloadSize, _fp);
		assert(size == kPayloadSize);
		uint8_t buf[4];
		const int count = sat_fread(buf, 1, 4, _fp);
		if(count != 4)
		{
			emu_printf("mayday 1\n");
			return;
		}
		/*
		assert(count == 4);
		if (kCheckSectorFileCrc) {
			const uint32_t crc = fioUpdateCRC(0, ptr, kPayloadSize);
			assert(crc == READ_LE_UINT32(buf));
		}*/
	} else {
emu_printf("SectorFile no pointer %p buf %p\n", ptr, _buf);
//		const int size = sat_fread(_buf, 1, kFioBufferSize, _fp);
		const int size = batchRead (_buf, kFioBufferSize);
		
		if(size != kFioBufferSize)
		{
			emu_printf("mayday 2\n");
			return;
		}		
		
//		assert(size == kFioBufferSize);
		/*if (kCheckSectorFileCrc) {
			const uint32_t crc = fioUpdateCRC(0, _buf, kFioBufferSize);
			assert(crc == 0);
		}*/
		_bufPos = 0;
	}
}

void SectorFile::seekAlign(uint32_t pos) {
	pos += (pos / 2048) * 4;
	const long alignPos = pos & ~2047;
	if (alignPos != (sat_ftell(_fp) - 2048)) {
		sat_fseek(_fp, alignPos, SEEK_SET);
emu_printf("refillBuffer seekalign\n");
		refillBuffer();
	}
	_bufPos = pos - alignPos;
}

void SectorFile::seek(int pos, int whence) {
emu_printf("SectorFile::seek\n");
	if (whence == SEEK_SET) {
		assert((pos & 2047) == 0);
		_bufPos = 2044;
		File::seek(pos, SEEK_SET);
	} else {
		assert(whence == SEEK_CUR && pos >= 0);
		const int bufLen = 2044 - _bufPos;
		if (pos < bufLen) {
			_bufPos += pos;
		} else {
			pos -= bufLen;
			const int count = (fioAlignSizeTo2048(pos) / 2048) - 1;
			if (count > 0) {
				const int alignPos = count * 2048;
				sat_fseek(_fp, alignPos, SEEK_CUR);
			}
emu_printf("refillBuffer du seek\n");
			refillBuffer();
			_bufPos = pos % 2044;
		}
	}
}

int SectorFile::read(uint8_t *ptr, int size) {
emu_printf("SectorFile::read %p\n", ptr);
	const int bufLen = 2044 - _bufPos;
	if (size >= bufLen) {
		if (bufLen) {
			memcpy(ptr, _buf + _bufPos, bufLen);
			ptr += bufLen;
			size -= bufLen;
		}
		const int count = (fioAlignSizeTo2048(size) / 2048) - 1;
		
		for (int i = 0; i < count; ++i) {
emu_printf("refillBuffer %p count %d\n", ptr, count);
			refillBuffer(ptr);
			ptr += 2044;
			size -= 2044;
		}
emu_printf("refillBuffer read sz %d\n", size);
		refillBuffer();
	}
	if (size != 0) {
//		assert(size <= 2044 - _bufPos);
		if(size > 2044 - _bufPos)
		{
			emu_printf("mayday 3\n");
			return -1;
		}	

		memcpy(ptr, _buf + _bufPos, size);
		_bufPos += size;
	}
	return 0;
}
