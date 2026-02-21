/*
 * Heart of Darkness engine rewrite
 * Copyright (C) 2009-2011 Gregory Montoir (cyx@users.sourceforge.net)
 */
#pragma GCC optimize ("Os")
 extern "C" {
#include 	<sega_gfs.h>
//#include 	<gfs_def.h>
#include 	<sl_def.h>
Sint32		iostat, iondata;
Sint32		gfsstat;
Uint32 ioskip_bytes;
}

#include <sys/param.h>
#include "fileio.h"
#include "util.h"


#define GFS_BYTE_SCT(byte, sctsiz)  \
    ((Sint32)(((Uint32)(byte)) + ((Uint32)(sctsiz)) - 1) / ((Uint32)(sctsiz)))

//static const bool kCheckSectorFileCrc = false;

#ifdef PSP
static const bool kSeekAbsolutePosition = true;
#else
static const bool kSeekAbsolutePosition = false;
#endif


File::File()
	: _fp(0) {
		emu_printf("File\n");
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
//emu_printf("File::seek %d %d\n");
	if(_fp)
	{
		if (kSeekAbsolutePosition && whence == SEEK_CUR) {
//	//emu_printf("here\n");
//	while(1);
			pos += sat_ftell(_fp);
			whence = SEEK_SET;
		}
//emu_printf("sat_fseek\n");
		sat_fseek(_fp, pos, whence);
	}
//emu_printf("sat_fseek done\n");
}
/*
void File::batchSeek()
{
	Uint32 start_sector = (_fp->f_seek_pos)/SECTOR_SIZE;
	GFS_Seek(_fp->fid, start_sector, GFS_SEEK_SET);
}
*/
	void File::batchSeek(int32_t off) {
		_fp->f_seek_pos += off;
	}

Uint32 File::batchRead(uint8_t *ptr, uint32_t len) {
//emu_printf("start %d len %d\n",(_fp->f_seek_pos)/SECTOR_SIZE, len);
	Uint32 start_sector = (_fp->f_seek_pos)/SECTOR_SIZE;
	GFS_Seek(_fp->fid, start_sector, GFS_SEEK_SET);
	while(!GFS_NwIsComplete(_fp->fid));
	Uint32 skip_bytes = _fp->f_seek_pos & (SECTOR_SIZE - 1);
	Sint32 tot_bytes = len + skip_bytes;
	Sint32 tot_sectors = GFS_BYTE_SCT(tot_bytes, SECTOR_SIZE);
	Uint32 readBytes = GFS_Fread(_fp->fid, tot_sectors, ptr, tot_bytes);
	_fp->f_seek_pos += (readBytes - skip_bytes);
	return readBytes;
}

//#define NWREAD 1

void File::asynchInit(uint8_t *ptr, uint32_t len) {
    Uint32 start_sector = (_fp->f_seek_pos)/SECTOR_SIZE;
//	GFS_NwStop(_fp->fid);
	GFS_Seek(_fp->fid, start_sector, GFS_SEEK_SET);
	while(!GFS_NwIsComplete(_fp->fid));
    ioskip_bytes = _fp->f_seek_pos & (SECTOR_SIZE - 1);
    Sint32 tot_bytes = len + ioskip_bytes;
    Sint32 tot_sectors = GFS_BYTE_SCT(tot_bytes, SECTOR_SIZE);
#ifdef NWREAD
    GFS_NwFread(_fp->fid, tot_sectors, ptr, tot_bytes);
	GFS_NwExecOne(_fp->fid);
#else
	GFS_NwCdRead(_fp->fid, tot_sectors);
	GFS_NwExecOne(_fp->fid);
#endif
    iostat = -1;
}
/*
void File::asynchRead() {
//emu_printf("asynchRead\n");
#ifdef NWREAD
    if(iostat == GFS_SVR_COMPLETED && iostat != -1)
	{
//emu_printf("asynchRead return\n");
        return ;
	}
//emu_printf("execone\n");
    GFS_NwExecOne(_fp->fid);
//emu_printf("getstat\n");
    GFS_NwGetStat(_fp->fid, &iostat, &iondata);
#else
	GFS_NwExecOne(_fp->fid);	
#endif
//emu_printf("getstat %d %d\n", iostat,iondata);
    // Don't update _fp->f_seek_pos here - let asynchWait() handle it
}
*/

int File::asynchWait(uint8_t *ptr, Sint32 len) {
#ifdef NWREAD
    if(iostat == GFS_SVR_COMPLETED && iostat != -1)
        return iondata;
    do {
        GFS_NwExecOne(_fp->fid);
        gfsstat = GFS_NwGetStat(_fp->fid, &iostat, &iondata);
    } while(gfsstat != GFS_SVR_COMPLETED);
#else
    Sint32 tot_bytes = len + ioskip_bytes;
    Sint32 tot_sectors = GFS_BYTE_SCT(tot_bytes, SECTOR_SIZE);
//emu_printf("1 %d %p %d\n", tot_sectors, ptr, tot_bytes);    
    // Just read the full requested amount - no limiting here
    iondata = GFS_Fread(_fp->fid, tot_sectors, ptr, tot_bytes);
//emu_printf("2\n");    
//    GFS_NwStop(_fp->fid);
//emu_printf("3\n");    

#endif
    _fp->f_seek_pos += (iondata - ioskip_bytes);
    iostat = -1;
    return iondata;
}

int File::read(uint8_t *ptr, int size) {
//emu_printf("sat_fread %d\n", size);
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
#ifdef SECTOR_ALIGNED
SectorFile::SectorFile() {
	memset(_buf, 0, sizeof(_buf));
	_bufPos = 2044;
}
#endif
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
#ifdef SECTOR_ALIGNED
void SectorFile::refillBuffer(uint8_t *ptr) {
//emu_printf("SectorFile::refillBuffer %p\n", ptr);
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
//emu_printf("SectorFile::seek\n");
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
#endif
