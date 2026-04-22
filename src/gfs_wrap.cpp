#pragma GCC optimize ("O2")
#include "util.h"
extern "C" {

#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "gfs_wrap.h"
#include "sat_mem_checker.h"

#define MNG_SVR(mng)            ((mng)->svr)
#define SVR_NFILE(svr)          ((svr)->nfile)
#define GFS_FILE_USED(file)     ((file)->used)
#define MNG_FILE(mng)           ((mng)->file)
//#define CACHE_SIZE (SECTOR_SIZE * 20)
#define TOT_SECTOR 8
#define CACHE_SIZE (SECTOR_SIZE * TOT_SECTOR)

Uint8 *current_lwram = (Uint8 *)VBT_L_START;
Uint8 *save_current_lwram;
char 	*strtok (char *__restrict, const char *__restrict);
int	 strncasecmp(const char *, const char *, size_t) __pure;
GfsDirTbl gfsDirTbl;
GfsDirName gfsDirName[DIR_MAX];
Uint32 gfsLibWork[GFS_WORK_SIZE(OPEN_MAX)/sizeof(Uint32)];     
Sint32 gfsDirN;

static char satpath[25];

static Uint8 cache[CACHE_SIZE] __attribute__ ((aligned (4)));
//uint8_t _sector_buf[SECTOR_SIZE] __attribute__((aligned(4)));
static Uint32 cache_offset = 0;

Uint8 *_sector_buf_ptr = NULL;
extern Sint32  gfcd_fatal_err;
extern GfsMng   *gfs_mng_ptr;
void	*malloc(size_t);
void CSH_Purge(void *adrs, Uint32 P_size);
}

// Pool statique pour GFS_FILE - hors d'atteinte de allocate_memory
static GFS_FILE gfs_file_pool[OPEN_MAX];
static bool gfs_file_used[OPEN_MAX] = {false};

// Used for initialization and such
#ifdef DEBUG_GFS
void errGfsFunc(void *obj, int ec)
{
	char texte[50];
	sprintf(texte, "ErrGfs %X %X",obj, ec); 
	
	texte[49]='\0';

	//emu_printf("%s\n", texte);

}
#endif

#define GFS_LOCAL
#define GFCD_ERR_OK             0       /* æ­£å¸¸çµ‚äº† */
#define GFCD_ERR_WAIT           -1      /* å‡¦ç†å¾…ã¡ */
#define GFCD_ERR_NOCDBLK        -2      /* CDãƒ–ãƒ­ãƒƒã‚¯ãŒæŽ¥ç¶šã•ã‚Œã¦ã„ãªã„ */
#define GFCD_ERR_NOFILT         -3      /* ç©ºãçµžã‚ŠãŒãªã„ */
#define GFCD_ERR_NOBUF          -4      /* ç©ºãåŒºç"»ãŒãªã„ */
#define GFCD_ERR_INUSE          -5      /* æŒ‡å®šã•ã‚ŒãŸè³‡æºãŒä½¿ç"¨ä¸­ */
#define GFCD_ERR_RANGE          -6      /* å¼•æ•°ãŒç¯„å›²å¤– */
#define GFCD_ERR_UNUSE          -7      /* æœªç¢ºä¿ã®ã‚‚ã®ã‚'æ"ä½œã—ã‚ˆã†ã¨ã—ãŸ */
#define GFCD_ERR_QFULL          -8      /* ã‚³ãƒžãƒ³ãƒ‰ã‚­ãƒ¥ãƒ¼ãŒã„ã£ã±ã„ */
#define GFCD_ERR_NOTOWNER       -9      /* éžæ‰€æœ‰è€…ãŒè³‡æºã‚'æ"ä½œã—ã‚ˆã†ã¨ã—ãŸ */
#define GFCD_ERR_CDC            -10     /* CDCã‹ã‚‰ã®ã‚¨ãƒ©ãƒ¼ */
#define GFCD_ERR_CDBFS          -11     /* CDãƒ–ãƒ­ãƒƒã‚¯ãƒ•ã‚¡ã‚¤ãƒ«ã‚·ã‚¹ãƒ†ãƒ ã‚¨ãƒ©ãƒ¼ */
#define GFCD_ERR_TMOUT          -12     /* ã‚¿ã‚¤ãƒ ã‚¢ã‚¦ãƒˆ */
#define GFCD_ERR_OPEN           -13     /* ãƒˆãƒ¬ã‚¤ãŒé–‹ã„ã¦ã„ã‚‹ */
#define GFCD_ERR_NODISC         -14     /* ãƒ‡ã‚£ã‚¹ã‚¯ãŒå…¥ã£ã¦ã„ãªã„ */
#define GFCD_ERR_CDROM          -15     /* CD-ROMã§ãªã„ãƒ‡ã‚£ã‚¹ã‚¯ãŒå…¥ã£ã¦ã„ã‚‹ */
#define GFCD_ERR_FATAL          -16     /* ã‚¹ãƒ†ãƒ¼ã‚¿ã‚¹ãŒFATAL */

#define MNG_ERROR(mng)          ((mng)->error)

GFS_LOCAL Sint32 gfs_mngSetErrCode(Sint32 code)
{
    GfsErrStat  *err;

    switch (gfcd_fatal_err) {
    case    GFCD_ERR_OK:
        break;
#if !defined(DEBUG_LIB)
    case    GFCD_ERR_OPEN:
        code = GFS_ERR_CDOPEN;
        break;
    case    GFCD_ERR_NODISC:
        code = GFS_ERR_CDNODISC;
        break;
#endif
    case    GFCD_ERR_FATAL:
        code = GFS_ERR_FATAL;
        break;
    default:
        break;
    }
    err = &MNG_ERROR(gfs_mng_ptr);
    GFS_ERR_CODE(err) = code;

    if ((code != GFS_ERR_OK)&&(GFS_ERR_FUNC(err) != NULL)) {
        GFS_ERR_FUNC(err)(GFS_ERR_OBJ(err), code);
    }
    return code;
}
/*
void GFS_SetErrFunc(GfsErrFunc func, void *obj)
{
    GFS_ERR_FUNC(&MNG_ERROR(gfs_mng_ptr)) = func;
    GFS_ERR_OBJ(&MNG_ERROR(gfs_mng_ptr)) = obj;
    gfs_mngSetErrCode(GFS_ERR_OK);
}
*/

void init_GFS() { //Initialize GFS system

	CDC_CdInit(0x00,0x00,0x05,0x0f);

    GFS_DIRTBL_TYPE(&gfsDirTbl) = GFS_DIR_NAME;
    GFS_DIRTBL_DIRNAME(&gfsDirTbl) = gfsDirName;
    GFS_DIRTBL_NDIR(&gfsDirTbl) = DIR_MAX;
    gfsDirN = GFS_Init(OPEN_MAX, gfsLibWork, &gfsDirTbl);
#ifdef DEBUG_GFS
	GFS_SetErrFunc((GfsErrFunc)errGfsFunc, NULL );
#endif
	memset(cache, 0, CACHE_SIZE);
	memset(gfs_file_used, 0, sizeof(gfs_file_used));
}

GFS_FILE *sat_fopen(const char *path, const int position) {
//emu_printf("-- sat_fopen %s\n", path);	
	memset(satpath, 0, 25);

	if (path == NULL) 
	{	
		return NULL; // nothing to do...
	}
	Uint16 idx;
	GFS_FILE *fp = NULL;
	idx = 0;

	Uint16 path_len = strlen(path);
	strncpy(satpath, path, path_len + 1);

	GfsHn fid = NULL;
	// OPEN FILE
	fid = GFS_Open(GFS_NameToId((Sint8*)satpath));
//emu_printf("--- satpath %s fileid %d fid %d\n",satpath, GFS_NameToId((Sint8*)satpath),fid);
	
	if(fid != NULL) { // Opened!
		Sint32 fsize;

		// Allouer depuis le pool statique
		fp = NULL;
		for (int i = 0; i < OPEN_MAX; i++) {
			if (!gfs_file_used[i]) {
				gfs_file_used[i] = true;
				fp = &gfs_file_pool[i];
				break;
			}
		}

		if (fp == NULL) {return NULL;}
		fp->fid = fid;
		GFS_GetFileInfo(fid, NULL, NULL, &fsize, NULL);
		fp->f_size = fsize;

		Sint32 tot_sectors = TOT_SECTOR;
		
		if (!position) // on conserve le cache existant
		{
			fp->f_seek_pos = 0;
			cache_offset = 0;
			if(position!=-1)
			{
//				GFS_SetTmode(fid, GFS_TMODE_SDMA1);
				GFS_Seek(fp->fid, 0, GFS_SEEK_SET);
				while(!GFS_NwIsComplete(fp->fid));
				
				memset((Uint8*)cache, 0, CACHE_SIZE);
				GFS_Fread(fp->fid, tot_sectors, (Uint8*)cache, CACHE_SIZE);
			}
			else
			{
				GFS_CdMovePickup(fp->fid);
				GFS_SetTransPara(fp->fid,12);
				GFS_SetTmode(fp->fid,GFS_TMODE_SDMA1);
			}
		}
		else
		{
//			GFS_SetTmode(fid, GFS_TMODE_SDMA1);
			GFS_Seek(fp->fid, position, GFS_SEEK_SET);
			while(!GFS_NwIsComplete(fp->fid));
			GFS_Fread(fp->fid, tot_sectors, (Uint8*)cache, CACHE_SIZE);
		}
	}
	else
	{
//		//emu_printf("no fid!!!\n");	
	}

	return fp;
}

int sat_fclose(GFS_FILE* fp) {
//Sint32 id, fsize;
//GFS_GetFileInfo(fp->fid, &id, NULL, &fsize, NULL);
//emu_printf("--- sat_fclose fid %d name %s\n",id, GFS_IdToName(id));
	
	GFS_Close(fp->fid);

	// Libérer le slot dans le pool
	for (int i = 0; i < OPEN_MAX; i++) {
		if (&gfs_file_pool[i] == fp) {
			gfs_file_used[i] = false;
			break;
		}
	}

	return 0; // always ok :-)
}

int sat_fseek(GFS_FILE *stream, long offset, int whence) {
////emu_printf("sat_fseek %d %d\n", offset, whence);	
	if(stream == NULL) return -1;

	switch(whence) {
		case SEEK_SET:
////emu_printf("sat_fseek SEEK_SET\n");		
			if(offset < 0 || offset >= stream->f_size)
			{
				//emu_printf("SEEK_SET failed !\n");
				return -1;
			}
			stream->f_seek_pos = offset;
			
			break;

		case SEEK_CUR:
////emu_printf("sat_fseek SEEK_CUR\n");	
			if((offset + stream->f_seek_pos) >= stream->f_size) 
			{
////emu_printf("SEEK_CUR failed !\n");	
				return -1;
			}
			stream->f_seek_pos += offset;
////emu_printf("stream->f_seek_pos %d\n", stream->f_seek_pos);			
			break;
/*
		case SEEK_END:
			if(stream->f_size + offset < 0) return -1;
			stream->f_seek_pos = (stream->f_size + offset);

			break;*/
	}

	return 0;
}
/*
int sat_ftell(GFS_FILE *stream)
{
	return (stream ? GFS_Tell(stream->fid) : 0);
}
*/
#define GFS_BYTE_SCT(byte, sctsiz)  \
    ((Sint32)(((Uint32)(byte)) + ((Uint32)(sctsiz)) - 1) / ((Uint32)(sctsiz)))

size_t sat_fread(void *ptr, size_t size, size_t nmemb, GFS_FILE *stream) {
//emu_printf("sat_fread ptr %p size %d pos %d stream->fid %d\n", ptr, size, stream->f_seek_pos, stream->f_size);	

	if (ptr == NULL || stream == NULL) return 0; // nothing to do then
	if (size == 0 || nmemb == 0) return 0;

	Uint8 *read_buffer = NULL;
	Sint32 tot_bytes; // Total bytes to read
	Sint32 tot_sectors;
	Uint32 readBytes;

//Sint32 id, fsize;
//GFS_GetFileInfo(stream->fid, &id, NULL, &fsize, NULL);
//emu_printf("--- sat_fread fid %d name %s\n",id, GFS_IdToName(id));


	Uint32 start_sector = (stream->f_seek_pos)/SECTOR_SIZE;
//	Uint32 skip_bytes = (stream->f_seek_pos)%SECTOR_SIZE; // Bytes to skip at the beginning of sector
	Uint32 skip_bytes = stream->f_seek_pos & (SECTOR_SIZE - 1);
//emu_printf("before seek fread %d  sz %d skip_bytes %d\n", start_sector, size, skip_bytes);
	if(GFS_Seek(stream->fid, start_sector, GFS_SEEK_SET) < 0) 
	{	
//emu_printf("GFS_Seek return 0\n");
	return 0;
	}
	tot_bytes = (nmemb * size) + skip_bytes;
//emu_printf("start_sector %d stream->f_seek_pos %d\n",start_sector,stream->f_seek_pos);
//	tot_sectors = GFS_ByteToSct(stream->fid, tot_bytes);
	tot_sectors = GFS_BYTE_SCT(tot_bytes, SECTOR_SIZE);
	if(tot_sectors < 0) return 0;
	Uint32 remaining_data, request_block;
	
	remaining_data = stream->f_size - stream->f_seek_pos;
	request_block = nmemb * size;
//	cache_offset=0; // vbt : on force la mise à zéro du cache, corrige un cas d'écran incorrect
	Uint32 dataToRead = MIN(request_block, remaining_data);

	if (/*(stream->file_hash == current_cached) &&*/ ((dataToRead + skip_bytes) < CACHE_SIZE)) {
		Uint32 end_offset;

partial_cache:
		end_offset = cache_offset + CACHE_SIZE;

		if(((stream->f_seek_pos + dataToRead) < end_offset) && (stream->f_seek_pos >= cache_offset)) {
			Uint32 offset_in_cache = stream->f_seek_pos - cache_offset;

//emu_printf("offset_in_cache %d \n", offset_in_cache);
			memcpy(ptr, cache + offset_in_cache, dataToRead);
			stream->f_seek_pos += dataToRead;
			return dataToRead;
		} 
	
		else if ((((stream->f_seek_pos + dataToRead) >= end_offset) || (stream->f_seek_pos < cache_offset))) {
//emu_printf("cache 0x%.8X - 0x%.8X req 0x%.8X\n", cache_offset, end_offset, stream->f_seek_pos);
			start_sector = stream->f_seek_pos / SECTOR_SIZE;
			skip_bytes = stream->f_seek_pos & (SECTOR_SIZE - 1); // Use bitwise AND instead of modulo

			tot_bytes = CACHE_SIZE;
			tot_sectors = GFS_BYTE_SCT(tot_bytes, SECTOR_SIZE);

			GFS_Seek(stream->fid, start_sector, GFS_SEEK_SET);
			while(!GFS_NwIsComplete(stream->fid));
			
			readBytes = GFS_Fread(stream->fid, tot_sectors, (Uint8*)cache, tot_bytes);
			cache_offset = start_sector * SECTOR_SIZE; // obligatoire
//			readBytes = tot_bytes;
			goto partial_cache;
		}
	}

	if(skip_bytes) {
//emu_printf("skip bytes %d %d\n", tot_bytes,skip_bytes);
		read_buffer = (Uint8*)cache;
		readBytes = GFS_Fread(stream->fid, tot_sectors, read_buffer, tot_bytes);
		memcpy(ptr, read_buffer + skip_bytes, readBytes - skip_bytes);
	} else {
//emu_printf("no skip bytes %d %p\n", tot_bytes, ptr);
		readBytes = GFS_Fread(stream->fid, tot_sectors, ptr, tot_bytes);
	}
	stream->f_seek_pos += (readBytes - skip_bytes); // Update the seek cursor 
	return (readBytes - skip_bytes);
}

int sat_feof(GFS_FILE *stream) {
	if((stream->f_size - 1) <= stream->f_seek_pos) return 1;
	else return 0;
}

// We won't ever write on CD... this is a dummy placeholder. 
size_t sat_fwrite(const void *ptr, size_t size, size_t nmemb, GFS_FILE *stream) {
	return 0;
}

int sat_ferror(GFS_FILE *stream) {
	return 0;
}

void CSH_Purge(void *adrs, Uint32 P_size)
{
	typedef Uint32 Linex[0x10/sizeof(Uint32)];	/* ラインは 0x10 バイト単位 */
	Linex *ptr, *end;
	Uint32 zero = 0;
	ptr = (Linex*)(((Uint32)adrs & 0x1fffffff) | 0x40000000);	/* キャッシュパージ領域 */
	end = (Linex*)((Uint32)ptr + P_size - 0x10);	/* 終了ポインタ（-0x10 はポストインクリメントの為） */
	ptr = (Linex*)((Uint32)ptr & -sizeof(Linex));	/* ラインアライメントに整合 */
	do {
		(*ptr)[0] = zero;			
	} while (ptr++ < end);
}
