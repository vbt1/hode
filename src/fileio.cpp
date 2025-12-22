/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */
 
 extern "C" {
#include 	<sl_def.h>
Sint32		iostat, iondata;
Uint32 ioskip_bytes;
}

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


File::File()
	: _fp(0) {
}

File::~File() {
}

void File::setFp(GFS_FILE *fp) {
////emu_printf("setFp\n");
	_fp = fp;
////emu_printf("setFp %p\n", _fp);
}

void File::seekAlign(uint32_t pos) {
////emu_printf("File::seekAlign %d\n", pos);
	sat_fseek(_fp, pos, SEEK_SET);
}

void File::seek(int pos, int whence) {
////emu_printf("File::seek %d %d\n");
	if(_fp)
	{
		if (kSeekAbsolutePosition && whence == SEEK_CUR) {
//	//emu_printf("here\n");
//	while(1);
			pos += sat_ftell(_fp);
			whence = SEEK_SET;
		}
//	//emu_printf("sat_fseek\n");
		sat_fseek(_fp, pos, whence);
	}
}
/*
void File::batchSeek()
{
	Uint32 start_sector = (_fp->f_seek_pos)/SECTOR_SIZE;
	GFS_Seek(_fp->fid, start_sector, GFS_SEEK_SET);
}
*/
Uint32 File::batchRead(uint8_t *ptr, uint32_t len) {
//emu_printf("start %d len %d\n",(_fp->f_seek_pos)/SECTOR_SIZE, len);
	Uint32 start_sector = (_fp->f_seek_pos)/SECTOR_SIZE;
//	GFS_SetReadPara(_fp->fid, 43);
//	GFS_SetTmode(_fp->fid, GFS_TMODE_SDMA0);
	//GFS_SetTmode(_fp->fid, GFS_TMODE_SCU);
	GFS_Seek(_fp->fid, start_sector, GFS_SEEK_SET);
	Uint32 skip_bytes = _fp->f_seek_pos & (SECTOR_SIZE - 1);
	Sint32 tot_bytes = len + skip_bytes;
	Sint32 tot_sectors = GFS_BYTE_SCT(tot_bytes, SECTOR_SIZE);

    Sint32		stat, ndata;
	GFS_NwFread(_fp->fid, tot_sectors, ptr, tot_bytes);
    do {
          GFS_NwExecOne(_fp->fid);
          GFS_NwGetStat(_fp->fid, &stat, &ndata);
    }while(stat != GFS_SVR_COMPLETED);	
	_fp->f_seek_pos += (ndata - skip_bytes);
	return ndata;
}

void File::asynchInit(uint8_t *ptr, uint32_t len) {
	Uint32 start_sector = (_fp->f_seek_pos)/SECTOR_SIZE;
	GFS_SetTmode(_fp->fid, GFS_TMODE_SDMA0);
	GFS_Seek(_fp->fid, start_sector, GFS_SEEK_SET);
//	GFS_SetReadPara(_fp->fid, 6);
	ioskip_bytes = _fp->f_seek_pos & (SECTOR_SIZE - 1);
	Sint32 tot_bytes = len + ioskip_bytes;
	Sint32 tot_sectors = GFS_BYTE_SCT(tot_bytes, SECTOR_SIZE);
	GFS_NwFread(_fp->fid, tot_sectors, ptr, tot_bytes);
	iostat = -1;
//	GFS_NwExecOne(_fp->fid);
//	GFS_NwGetStat(_fp->fid, &iostat, &iondata);
}

void File::asynchRead() {
    GFS_NwExecOne(_fp->fid);
    GFS_NwGetStat(_fp->fid, &iostat, &iondata);

	if(iostat == GFS_SVR_COMPLETED)
	{
//		ioskip_bytes = _fp->f_seek_pos & (SECTOR_SIZE - 1);
		_fp->f_seek_pos += (iondata - ioskip_bytes);
		iostat = -1;
	}
}

void File::asynchWait() {
	if(iostat == GFS_SVR_COMPLETED && iostat !=-1)
		return;

	//ioskip_bytes = _fp->f_seek_pos & (SECTOR_SIZE - 1);
    do {
          GFS_NwExecOne(_fp->fid);
          GFS_NwGetStat(_fp->fid, &iostat, &iondata);
    }while(iostat != GFS_SVR_COMPLETED);
//	ioskip_bytes = _fp->f_seek_pos & (SECTOR_SIZE - 1);
	_fp->f_seek_pos += (iondata - ioskip_bytes);
	iostat = -1;
}

int File::read(uint8_t *ptr, int size) {
////emu_printf("sat_fread %d\n", size);
	return sat_fread(ptr, 1, size, _fp);
}

uint8_t File::readByte() {
////emu_printf("readByte\n");
	uint8_t buf;
	read(&buf, 1);
	return buf;
}

uint16_t File::readUint16() {
////emu_printf("readUint16\n");
	uint8_t buf[2];
	read(buf, 2);
	return READ_LE_UINT16(buf);
}

uint32_t File::readUint32() {
////emu_printf("readUint32\n");
	uint8_t buf[4];
	read(buf, 4);
	return READ_LE_UINT32(buf);
}

SectorFile::SectorFile() {
	memset(_buf, 0, sizeof(_buf));
	_bufPos = 2044;
}

int fioAlignSizeTo2048(int size) {
	return ((size + 2043) / 2044) * 2048;
}
/*
uint32_t fioUpdateCRC(uint32_t sum, const uint8_t *buf, uint32_t size) {
//	assert((size & 3) == 0);
	for (uint32_t offset = 0; offset < size; offset += 4) {
		sum ^= READ_LE_UINT32(buf + offset);
	}
	return sum;
}
*/
void SectorFile::refillBuffer(uint8_t *ptr) {
////emu_printf("SectorFile::refillBuffer %p\n", ptr);
	if (ptr) {
		static const int kPayloadSize = kFioBufferSize - 4;
		const int size = sat_fread(ptr, 1, kPayloadSize, _fp);
//		assert(size == kPayloadSize);
		if(size != kPayloadSize)
			return;
			
		uint8_t buf[4];
		const int count = sat_fread(buf, 1, 4, _fp);
		if(count != 4)
		{
//			//emu_printf("mayday 1\n");
			return;
		}
		/*
		assert(count == 4);
		if (kCheckSectorFileCrc) {
			const uint32_t crc = fioUpdateCRC(0, ptr, kPayloadSize);
			assert(crc == READ_LE_UINT32(buf));
		}*/
	} else {
////emu_printf("SectorFile no pointer %p buf %p\n", ptr, _buf);
		const int size = sat_fread(_buf, 1, kFioBufferSize, _fp);
//		const int size = batchRead (_buf, kFioBufferSize);
		
		if(size != kFioBufferSize)
		{
//			//emu_printf("mayday 2\n");
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
////emu_printf("refillBuffer seekalign\n");
		refillBuffer();
	}
	_bufPos = pos - alignPos;
}

void SectorFile::seek(int pos, int whence) {
////emu_printf("SectorFile::seek\n");
	if (whence == SEEK_SET) {
//		assert((pos & 2047) == 0);
		if((pos & 2047) != 0)
			return;
		_bufPos = 2044;
		File::seek(pos, SEEK_SET);
	} else {
//		assert(whence == SEEK_CUR && pos >= 0);
		if(!(whence == SEEK_CUR && pos >= 0))
			return;
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
////emu_printf("refillBuffer du seek\n");
			refillBuffer();
			_bufPos = pos % 2044;
		}
	}
}

int SectorFile::read(uint8_t *ptr, int size) {
////emu_printf("SectorFile::read %p\n", ptr);
	const int bufLen = 2044 - _bufPos;
	if (size >= bufLen) {
		if (bufLen) {
			memcpy(ptr, _buf + _bufPos, bufLen);
			ptr += bufLen;
			size -= bufLen;
		}
		const int count = (fioAlignSizeTo2048(size) / 2048) - 1;
//emu_printf("refillBuffer %p count %d\n", ptr, count);		
		for (int i = 0; i < count; ++i) {
////emu_printf("refillBuffer %p count %d\n", ptr, count);
			refillBuffer(ptr);
			ptr += 2044;
			size -= 2044;
		}
////emu_printf("refillBuffer read sz %d\n", size);
		refillBuffer();
	}
	if (size != 0) {
//		assert(size <= 2044 - _bufPos);
		if(size > 2044 - _bufPos)
		{
//			//emu_printf("mayday 3\n");
			return -1;
		}	

		memcpy(ptr, _buf + _bufPos, size);
		_bufPos += size;
	}
	return 0;
}
