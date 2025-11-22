/*
 * REminiscence - Flashback interpreter
 * Copyright (C) 2005-2019 Gregory Montoir (cyx@users.sourceforge.net)
 */
//#define SLAVE_SOUND 1
//#define SOUND 1
#define FONT_ADDR 0x25c01000

extern "C" {
#include <sl_def.h>
#include <string.h>	
//#include <sgl.h>

//#include <sega_mem.h>
#include <sega_int.h>
#ifdef SOUND
#include <sega_pcm.h>
#endif
//#include <sega_snd.h>
//#include "sega_csh.h"
//#include "sega_spr.h"
#include <sega_sys.h>
#include "gfs_wrap.h"
#include "sat_mem_checker.h"
//#include "cdtoc.h"
volatile Uint32 ticker = 0;
Uint8 tickPerVblank = 0;
#ifdef FRAME
extern unsigned char frame_x;
extern unsigned char frame_y;
extern unsigned char frame_z;
#endif
}
extern void snd_init();
//extern void emu_printf(const char *format, ...);

#include "sys.h"
//#include "mixer.h"
#include "system.h"
#include "systemstub.h"

 //#include "saturn_print.h"

#undef assert
//#define assert(x) if(!(x)){emu_printf("assert %s %d %s\n", __FILE__,__LINE__,__func__);}
#define assert(x) if(!(x)){}
/*
#undef VDP2_VRAM_A0
#define VDP2_VRAM_A0 NULL 
#undef VDP2_VRAM_B0
#define VDP2_VRAM_B0 NULL 
*/
#define	    toFIXED(a)		((FIXED)(65536.0 * (a)))
/* Needed to unlock cd drive */
#define SYS_CDINIT1(i) ((**(void(**)(int))0x60002dc)(i)) // Init functions for Saturn CD drive
#define SYS_CDINIT2() ((**(void(**)(void))0x600029c)())

/* Needed for audio */
#define SND_BUFFER_SIZE (128)
#define SND_BUF_SLOTS 1

/* Needed for video */
#define TVSTAT	(*(Uint16 *)0x25F80004)
#define CRAM_BANK 0x5f00000 // Beginning of color ram memory addresses
#define BACK_COL_ADR (VDP2_VRAM_A1 + 0x1fffe) // Address for background colour

/* Input devices */
#define MAX_INPUT_DEVICES 1

#define PAD_PUSH_UP    (!(push & PER_DGT_KU))
#define PAD_PUSH_DOWN  (!(push & PER_DGT_KD))
#define PAD_PUSH_LEFT  (!(push & PER_DGT_KL))
#define PAD_PUSH_RIGHT (!(push & PER_DGT_KR))
#define PAD_PUSH_A  (!(push & PER_DGT_TA))
#define PAD_PUSH_B  (!(push & PER_DGT_TB))
#define PAD_PUSH_C  (!(push & PER_DGT_TC))
#define PAD_PUSH_X  (!(push & PER_DGT_TX))
#define PAD_PUSH_Z  (!(push & PER_DGT_TZ))
#define PAD_PUSH_LTRIG  (!(push & PER_DGT_TL))
#define PAD_PUSH_RTRIG  (!(push & PER_DGT_TR))
#define PAD_PUSH_START (!(push & PER_DGT_ST))

#define PAD_PULL_UP    (!(pull & PER_DGT_KU))
#define PAD_PULL_DOWN  (!(pull & PER_DGT_KD))
#define PAD_PULL_LEFT  (!(pull & PER_DGT_KL))
#define PAD_PULL_RIGHT (!(pull & PER_DGT_KR))
#define PAD_PULL_A  (!(pull & PER_DGT_TA))
#define PAD_PULL_B  (!(pull & PER_DGT_TB))
#define PAD_PULL_C  (!(pull & PER_DGT_TC))
#define PAD_PULL_X  (!(pull & PER_DGT_TX))
#define PAD_PULL_Z  (!(pull & PER_DGT_TZ))
#define PAD_PULL_LTRIG  (!(pull & PER_DGT_TL))
#define PAD_PULL_RTRIG  (!(pull & PER_DGT_TR))
#define PAD_PULL_START (!(pull & PER_DGT_ST))
/*
typedef struct {
	volatile Uint8 access;
} SatMutex;

typedef struct {
	uint16 x, y;
	uint16 w, h;
} SAT_Rect;
*/
/* Required for audio sound buffers */
//Uint8 buffer_filled[2];
Uint8 ring_bufs[2][SND_BUFFER_SIZE * SND_BUF_SLOTS];
#ifdef SOUND
static PcmWork pcm_work[2];
static PcmHn pcm[2];
#endif
//Uint8 curBuf = 0;
//Uint8 curSlot = 0;
//static Mixer *mix = NULL;
//static volatile Uint8 audioEnabled = 1;

/* CDDA */

//CDTableOfContents toc;
//CdcPly	playdata;
//CdcPos	posdata;
CdcStat statdata;

//static SystemStub *sys = NULL;

/* FUNCTIONS */
#ifdef SOUND
static Uint8 firstSoundRun = 1;
static Uint8 runningSlave = 0;
static PcmHn createHandle(int bufno);
static void play_manage_buffers(void);
static void fill_buffer_slot(void);
void fill_play_audio(void);
void sat_restart_audio(void);
#endif
void vblIn(void); // This is run at each vblnk-in
uint8 isNTSC(void);

/* SDL WRAPPER */
struct SystemStub_SDL : System {
	enum {
		kJoystickCommitValue = 3200,
		kKeyMappingsSize = 20,
		kAudioHz = 22050
	};

//	uint8 _overscanColor;
	uint16 _pal[512];
//	uint16 _screenW, _screenH;

	/* Controller data */
	PerDigital *input_devices[MAX_INPUT_DEVICES];
	Uint8 connected_devices;
	SystemStub_SDL();
	virtual ~SystemStub_SDL();
	virtual void init(const char *title, int w, int h);
	virtual void destroy();
//	virtual void setPaletteEntry(uint16 i, const Color *c);
//	virtual void getPaletteEntry(uint16 i, Color *c);
//	virtual void setOverscanColor(uint8 i);
	virtual void initTimeStamp();
	virtual uint32 getOutputSampleRate();
	virtual void setup_input (void); // Setup input controllers
	virtual void setScaler(const char *name, int multiplier);
	virtual void setGamma(float gamma);
	virtual void setPalette(const uint8_t *pal, int n, int depth);
	virtual void clearPalette();
	virtual void copyRect(int x, int y, int w, int h, const uint8_t *buf, int pitch);
	virtual void copyYuv(int w, int h, const uint8_t *y, int ypitch, const uint8_t *u, int upitch, const uint8_t *v, int vpitch);
	virtual void fillRect(int x, int y, int w, int h, uint8_t color);
	virtual void copyRectWidescreen(int w, int h, const uint8_t *buf, const uint8_t *pal);
	virtual void shakeScreen(int dx, int dy);
	virtual void updateScreen(bool drawWidescreen);
	virtual void processEvents();
	virtual void sleep(int duration);
	virtual uint32_t getTimeStamp();
	virtual void startAudio(AudioCallback callback);
	virtual void stopAudio();
	virtual void lockAudio();
	virtual void unlockAudio();
	virtual AudioCallback setAudioCallback(AudioCallback callback);



	void prepareGfxMode();
	void load_audio_driver(void);
	void init_cdda(void);
	void sound_external_audio_enable(uint8_t vol_l, uint8_t vol_r);
};
System* g_system = nullptr;

// static const int AUDIO_FREQ = 44100;
static const int AUDIO_SAMPLES_COUNT = 2048;

static const int SCREEN_W = 480;
static const int SCREEN_H = 272;
static const int SCREEN_PITCH = 512;

static const int GAME_W = 256;
static const int GAME_H = 192;

static const int BLUR_TEX_W = 16;
static const int BLUR_TEX_H = 16;

static uint16_t __attribute__((aligned(16))) _clut[256];
System *SystemStub_SDL_create() {
	return new SystemStub_SDL();
}

SystemStub_SDL::SystemStub_SDL() {
}

SystemStub_SDL::~SystemStub_SDL() {
}

void SystemStub_SDL::init(const char *title, int w, int h) {
emu_printf("init system\n");
#if 1
	memset(&inp, 0, sizeof(inp)); // Clean inout
emu_printf("load_audio_driver\n");
	load_audio_driver(); // Load M68K audio driver
	init_cdda();
	sound_external_audio_enable(7, 7);
emu_printf("prepareGfxMode\n");
//	prepareGfxMode(); // Prepare graphic output
emu_printf("setup_input\n");
	setup_input(); // Setup controller inputs

//	memset(_pal, 0, sizeof(_pal));

//	audioEnabled = 0;
//	curBuf = 0;
//	curSlot = 0;
//emu_printf("SystemStub_SDL::init\n");	
#ifdef SLAVE_SOUND
	*(Uint8*)OPEN_CSH_VAR(buffer_filled[0]) = 0;
	*(Uint8*)OPEN_CSH_VAR(buffer_filled[1]) = 0;
#else
//	buffer_filled[0] = 0;
//	buffer_filled[1] = 0;
#endif
	if(isNTSC())
		tickPerVblank = 17;
	else
		tickPerVblank = 20;
emu_printf("slIntFunction\n");
	slIntFunction(vblIn); // Function to call at each vblank-in // vbt à remettre
#endif
	return;
}


void SystemStub_SDL::destroy() {
//	cleanupGfxMode();
	SYS_Exit(0);
}

void SystemStub_SDL::setScaler(const char *name, int multiplier) {
}

void SystemStub_SDL::setGamma(float gamma) {
}

void SystemStub_SDL::setPalette(const uint8_t *pal, int n, int depth) {
	const int shift = 8 - depth;
	for (int i = 0; i < n; ++i) {

		int r = pal[i * 3];
		int g = pal[i * 3 + 1];
		int b = pal[i * 3 + 2];
		if (shift != 0) {
			r = (r << shift) | (r >> (depth - shift));
			g = (g << shift) | (g >> (depth - shift));
			b = (b << shift) | (b >> (depth - shift));
		}
//		emu_printf("r%d g%d b%d\n",r,g,b);
		_clut[i] = ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3) | RGB_Flag; // BGR for saturn		
	}
	slTransferEntry((void*)_clut, (void*)(CRAM_BANK), 256 * 2);
//	sceKernelDcacheWritebackRange(_clut, sizeof(_clut));
}

/*
void SystemStub_SDL::setPalette(uint8 *palette, uint16 colors) {
	assert(colors <= 256);
	for (int i = 0; i < colors; ++i) {
		uint8 r = palette[i * 3];
		uint8 g = palette[i * 3 + 1];
		uint8 b = palette[i * 3 + 2];

		_pal[i] = ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3) | RGB_Flag; // BGR for saturn
	}
}

void SystemStub_SDL::setPaletteEntry(uint16 i, const Color *c) {
	_pal[i] = RGB(c->r, c->g, c->b);
}

void SystemStub_SDL::getPaletteEntry(uint16 i, Color *c) {
	Uint8 b = ((_pal[i] >> 10) & 0x1F);
	Uint8 g = ((_pal[i] >> 5)  & 0x1F);
	Uint8 r = ((_pal[i] >> 0)  & 0x1F);

	c->r = r;
	c->g = g;
	c->b = b;
}
*/
void SystemStub_SDL::copyRectWidescreen(int w, int h, const uint8_t *buf, const uint8_t *pal) 
{
	int pitch = 256;
	int x = 0;
	int y = 0;
	emu_printf("copyRect %d %d\n",w,h);
		uint8 *srcPtr = (uint8 *)(buf + y * pitch + x);
		uint8 *dstPtr = (uint8 *)(VDP2_VRAM_A0 + (y * (pitch*2)) + x);

		for (uint16 idx = 0; idx < h; ++idx) {
			DMA_ScuMemCopy(dstPtr, srcPtr, w);
			srcPtr += pitch;
			dstPtr += (pitch*2);
			SCU_DMAWait();
		}
	emu_printf("end copyRectwide %d %d %d %d\n",x,y,w,h);	
}

void SystemStub_SDL::copyRect(int x, int y, int w, int h, const uint8_t *buf, int pitch) {
	// Calculate initial source and destination pointers
//emu_printf("copyRect %d %d %d %d\n",x,y,w,h);
	uint8 *srcPtr = (uint8 *)(buf + y * pitch + x);
	uint8 *dstPtr = (uint8 *)(VDP2_VRAM_A0 + (y * (pitch*2)) + x);

	for (uint16 idx = 0; idx < h; ++idx) {
		DMA_ScuMemCopy(dstPtr, srcPtr, w);
		srcPtr += pitch;
		dstPtr += (pitch*2);
		SCU_DMAWait();
	}
//emu_printf("end copyRect %d %d %d %d\n",x,y,w,h);
}

void SystemStub_SDL::updateScreen(bool drawWidescreen) {
	slTransferEntry((void*)_clut, (void*)(CRAM_BANK), 256 * 2);  // vbt à remettre
//	slTransferEntry((void*)_clut, (void*)(CRAM_BANK+512), 256 * 2);  // vbt à remettre
}

void SystemStub_SDL::processEvents() {
//emu_printf("processEvents\n");
	inp.prevMask = inp.mask;
	pad.prevMask = pad.mask;
//	pad.mask &= ~(SYS_INP_UP | SYS_INP_DOWN | SYS_INP_LEFT | SYS_INP_RIGHT);
	inp.mask = 0;
	
	Uint16 push;
	Uint16 pull;

	switch(input_devices[0]->id) { // Check only the first controller...
		case PER_ID_StnAnalog: // ANALOG PAD
		case PER_ID_StnPad: // DIGITAL PAD 
			push = (volatile Uint16)(input_devices[0]->push);
			pull = (volatile Uint16)(input_devices[0]->pull);

			if (PAD_PUSH_DOWN)
				pad.mask |= SYS_INP_DOWN;
			else
			{
				if (PAD_PULL_DOWN)
					pad.mask &= ~SYS_INP_DOWN;
			}

			if (PAD_PUSH_UP)
				pad.mask |= SYS_INP_UP;
			else
			{
				if (PAD_PULL_UP)
					pad.mask &= ~SYS_INP_UP;
			}

			if (PAD_PUSH_RIGHT)
				pad.mask |= SYS_INP_RIGHT;
			else
			{
				if (PAD_PULL_RIGHT)
					pad.mask &= ~SYS_INP_RIGHT;
			}

			if (PAD_PUSH_LEFT)
				pad.mask |= SYS_INP_LEFT;
			else
			{
				if (PAD_PULL_LEFT)
					pad.mask &= ~SYS_INP_LEFT;
			}

			if (PAD_PUSH_A)
				pad.mask |= SYS_INP_JUMP;
			else
			{
				if (PAD_PULL_A)
					pad.mask &= ~SYS_INP_JUMP;
			}

			if (PAD_PUSH_B)
				pad.mask |= SYS_INP_SHOOT;
			else
			{
				if (PAD_PULL_B)
					pad.mask &= ~SYS_INP_SHOOT;
			}

			if (PAD_PUSH_C)
				pad.mask |= SYS_INP_RUN;
			else
			{
				if (PAD_PULL_C)
					pad.mask &= ~SYS_INP_RUN;
			}
			
			if (PAD_PUSH_START)
				pad.mask |= SYS_INP_ESC;
			break;
		default:
			break;
	}
	inp.mask |= pad.mask;
	return;
}
/*
void SystemStub_SDL::sleep(uint32 duration) {
	static Uint8 counter = 0;

	uint32 wait_tick = ticker + duration;
	counter++;

	while(wait_tick >= ticker);
}
*/
uint32_t SystemStub_SDL::getTimeStamp() {
	return ticker;
}

void SystemStub_SDL::initTimeStamp() {
	ticker = 0;
}
#ifdef SOUND
void SystemStub_SDL::startAudio(AudioCallback callback, void *param) {
	mix = (Mixer*)param;

	memset(ring_bufs, 0, SND_BUFFER_SIZE * 2 * SND_BUF_SLOTS);

	PCM_Init(); // Initialize PCM playback

//	audioEnabled = 1; // Enable audio

	// Prepare handles
	pcm[0] = createHandle(0);
	pcm[1] = createHandle(1);

	// start playing
	PCM_Start(pcm[0]); 
	PCM_EntryNext(pcm[1]);
}
void SystemStub_SDL::stopAudio() {
//	audioEnabled = 0;

	// Stopping playback
	PCM_Stop(pcm[0]);
	PCM_Stop(pcm[1]);

	// Destroy handles
	PCM_DestroyMemHandle(pcm[0]);
	PCM_DestroyMemHandle(pcm[1]);

	// Deinitialize PCM playback
	PCM_Finish();


	return;
}
#endif

uint32 SystemStub_SDL::getOutputSampleRate() {
	return kAudioHz;
}
/*
void *SystemStub_SDL::createMutex() {
//emu_printf("SystemStub_SDL::createMutex\n");	
	SatMutex *mtx = (SatMutex*)malloc(sizeof(SatMutex));
#ifdef SLAVE_SOUND
	*(Uint8*)OPEN_CSH_VAR(mtx->access) = 0;
#else
	mtx->access = 0;
#endif
	return mtx;
}

void SystemStub_SDL::destroyMutex(void *mutex) {
	sat_free(mutex);
	return;
}

void SystemStub_SDL::lockMutex(void *mutex) {
	SatMutex *mtx = (SatMutex*)mutex;
#ifdef SLAVE_SOUND	
	while(*(Uint8*)OPEN_CSH_VAR(mtx->access) > 0) asm("nop");
	(*(Uint8*)OPEN_CSH_VAR(mtx->access))++;
#else
//	while(mtx->access > 0) asm("nop");
	mtx->access++;
#endif
	return;
}

void SystemStub_SDL::unlockMutex(void *mutex) {
//emu_printf("SystemStub_SDL::unlockMutex\n");	
	SatMutex *mtx = (SatMutex*)mutex;
#ifdef SLAVE_SOUND
	(*(Uint8*)OPEN_CSH_VAR(mtx->access))--;
#else
	mtx->access--;
#endif
	return;
}
*/
void SystemStub_SDL::prepareGfxMode() {
	slTVOff(); // Turn off display for initialization

	slColRAMMode(CRM16_2048); // Color mode: 1024 colors, choosed between 16 bit

//	slBitMapNbg1(COL_TYPE_256, BM_512x512, (void*)VDP2_VRAM_A0); // Set this scroll plane in bitmap mode
	slBMPaletteNbg0(1); // NBG1 (game screen) uses palette 1 in CRAM
	slBMPaletteNbg1(2); // NBG1 (game screen) uses palette 2 in CRAM
	slColRAMOffsetSpr(2) ;  // spr palette
#ifdef _352_CLOCK_
	// As we are using 352xYYY as resolution and not 320xYYY, this will take the game back to the original aspect ratio
#endif
	
	memset((void*)VDP2_VRAM_A0, 0x00, 512*240); // Clean the VRAM banks. // à remettre
	memset((void*)(SpriteVRAM + cgaddress),0,0x30000);
	slPriorityNbg0(4); // Game screen
	slPriorityNbg1(6); // Game screen
	slPrioritySpr0(4);
	
	slScrTransparent(NBG0ON); // Do NOT elaborate transparency on NBG1 scroll
//	slZoomNbg0(toFIXED(0.8), toFIXED(1.0));
//	slZoomNbg1(toFIXED(0.8), toFIXED(1.0));


	slZdspLevel(7); // vbt : ne pas d?placer !!!
	slBack1ColSet((void *)BACK_COL_ADR, 0x8000); // Black color background
	
//	extern Uint16 VDP2_RAMCTL;	
//	VDP2_RAMCTL = VDP2_RAMCTL & 0xFCFF;
//	extern Uint16 VDP2_TVMD;
//	VDP2_TVMD &= 0xFEFF;
	slScrAutoDisp(NBG1ON); // à faire toujours en dernier
//	slScrCycleSet(0x55EEEEEE , NULL , 0x44EEEEEE , NULL);
/*
	slWindow(63 , 0 , 574 , 447 , 241 ,320 , 224);

	SPRITE *sys_clip = (SPRITE *) SpriteVRAM;
	(*sys_clip).XC = 574;

	slScrWindow0(63 , 0 , 574 , 447 );
	slScrWindowModeNbg0(win0_IN);
	slScrWindow1(63 , 0 , 574 , 447 );
	slScrWindowModeNbg1(win1_IN);
	slScrWindowModeSPR(win0_IN);
*/	
	slScrPosNbg0(0,0) ;
	slScrPosNbg1(0,0) ;
//	slSpecialPrioModeNbg0(spPRI_Dot);
//	slSpecialPrioBitNbg0(1); // obligatoire
//	slSpecialFuncCodeA(sfCOL_ef);
//	slSpecialFuncCodeB(0x4);
	slTVOn(); // Initialization completed... tv back on
	slSynch();  // faire un slsynch à la fin de la config
	return;
}
/*
void SystemStub_SDL::cleanupGfxMode() {
	slTVOff();
	return;
}

void SystemStub_SDL::forceGfxRedraw() {
	return;
}

void SystemStub_SDL::drawRect(SAT_Rect *rect, uint8 color, uint16 *dst, uint16 dstPitch) {
	return;
}*/

	void SystemStub_SDL::clearPalette() {}
	void SystemStub_SDL::copyYuv(int w, int h, const uint8_t *y, int ypitch,
						 const uint8_t *u, int upitch, const uint8_t *v, int vpitch) {}
	void SystemStub_SDL::fillRect(int x, int y, int w, int h, uint8_t color) {}
	void SystemStub_SDL::shakeScreen(int dx, int dy) {}
	void SystemStub_SDL::processEvents();
	
	void SystemStub_SDL::sleep(int duration) 
	{
/*
		static Uint8 counter = 0;

		uint32 wait_tick = ticker + duration;
		counter++;

		while(wait_tick >= ticker);
*/
	}

	void SystemStub_SDL::startAudio(AudioCallback callback) {}
	void SystemStub_SDL::stopAudio() {}
	void SystemStub_SDL::lockAudio() {}
	void SystemStub_SDL::unlockAudio() {}
	AudioCallback setAudioCallback(AudioCallback callback) { return callback; }


// Store the info on connected peripheals inside an array
void SystemStub_SDL::setup_input (void) {
	if ((Per_Connect1 + Per_Connect2) == 0) {
		connected_devices = 0;
		return; // Nothing connected...
	}
	
	Uint8 index, input_index = 0;

	// check up to 6 peripheals on left connector
	for(index = 0; (index < Per_Connect1) && (input_index < MAX_INPUT_DEVICES); index++)
		if(Smpc_Peripheral[index].id != PER_ID_NotConnect) {
			input_devices[input_index] = &(Smpc_Peripheral[index]);
			input_index++;
		}

	// check up to 6 peripheals on right connector 
	for(index = 0; (index < Per_Connect2) && (input_index < MAX_INPUT_DEVICES); index++)
		if(Smpc_Peripheral[index + 15].id != PER_ID_NotConnect) {
			input_devices[input_index] = &(Smpc_Peripheral[index + 15]);
			input_index++;
		}

	connected_devices = input_index;
}

void SystemStub_SDL::load_audio_driver(void) {
	snd_init();
	return;
}


AudioCallback SystemStub_SDL::setAudioCallback(AudioCallback callback) {
	AudioCallback cb; //_audioCb;
/*	lockAudio();
	_audioCb = callback;
	unlockAudio();*/
	return cb;
}

void SystemStub_SDL::init_cdda(void)
{
	CdcPly playdata;
//	CDC_TgetToc((Uint32*)&toc);
	
    CDC_PLY_STYPE(&playdata) = CDC_PTYPE_TNO;	/* set by track number.*/
    CDC_PLY_STNO( &playdata) = 2;		/* start track number. */
    CDC_PLY_SIDX( &playdata) = 1;		/* start index number. */
    CDC_PLY_ETYPE(&playdata) = CDC_PTYPE_TNO;	/* set by track number.*/
    CDC_PLY_ETNO( &playdata) = 48;		/* end track number. */
    CDC_PLY_EIDX( &playdata) = 99;		/* start index number. */
    CDC_PLY_PMODE(&playdata) = CDC_PTYPE_NOCHG;//CDC_PM_DFL + 30;	/* Play Mode. */ // lecture en boucle
//    CDC_PLY_PMODE(&playdata) = CDC_PTYPE_NOCHG;//CDC_PM_DFL+30;//CDC_PM_DFL ;	/* Play Mode. */ // lecture unique
	statdata.report.fad = 0;
	
}

 void SystemStub_SDL::sound_external_audio_enable(uint8_t vol_l, uint8_t vol_r) {
    volatile uint16_t *slot_ptr;

    //max sound volume is 7
    if (vol_l > 7) {
        vol_l = 7;
    }
    if (vol_r > 7) {
        vol_r = 7;
    }

    // Setup SCSP Slot 16 and Slot 17 for playing
    slot_ptr = (volatile Uint16 *)(0x25B00000 + (0x20 * 16));
    slot_ptr[0] = 0x1000;
    slot_ptr[1] = 0x0000; 
    slot_ptr[2] = 0x0000; 
    slot_ptr[3] = 0x0000; 
    slot_ptr[4] = 0x0000; 
    slot_ptr[5] = 0x0000; 
    slot_ptr[6] = 0x00FF; 
    slot_ptr[7] = 0x0000; 
    slot_ptr[8] = 0x0000; 
    slot_ptr[9] = 0x0000; 
    slot_ptr[10] = 0x0000; 
    slot_ptr[11] = 0x001F | (vol_l << 5);
    slot_ptr[12] = 0x0000; 
    slot_ptr[13] = 0x0000; 
    slot_ptr[14] = 0x0000; 
    slot_ptr[15] = 0x0000; 

    slot_ptr = (volatile Uint16 *)(0x25B00000 + (0x20 * 17));
    slot_ptr[0] = 0x1000;
    slot_ptr[1] = 0x0000; 
    slot_ptr[2] = 0x0000; 
    slot_ptr[3] = 0x0000; 
    slot_ptr[4] = 0x0000; 
    slot_ptr[5] = 0x0000; 
    slot_ptr[6] = 0x00FF; 
    slot_ptr[7] = 0x0000; 
    slot_ptr[8] = 0x0000; 
    slot_ptr[9] = 0x0000; 
    slot_ptr[10] = 0x0000; 
    slot_ptr[11] = 0x000F | (vol_r << 5);
    slot_ptr[12] = 0x0000; 
    slot_ptr[13] = 0x0000; 
    slot_ptr[14] = 0x0000; 
    slot_ptr[15] = 0x0000;

    *((volatile Uint16 *)(0x25B00400)) = 0x020F;
}

inline void timeTick() {
	if(ticker > (0xFFFFFFFF - tickPerVblank)) {
		ticker = 0;
	} else {
		ticker += tickPerVblank;
	}
}

void vblIn (void) {
//emu_printf("vblIn\n");
	// Process input
/*
//	if(!loadingMap)
	{
		uint8_t hz = ((TVSTAT & 1) == 0)?60:50;
#ifdef FRAME
		frame_y++;

		if(frame_y>=hz)
		{
			frame_z = frame_x;
			frame_x = 0;
			frame_y = 0;
		}
#endif
//		system_saturn.updateScreen(0);
	}*/
	g_system->processEvents();
	timeTick();
}

uint8 isNTSC (void) {
	if(!(TVSTAT & 1))
		return 1;
	else
		return 0;
}

void SCU_DMAWait(void) {
//	Uint32 res;

	while((DMA_ScuResult()) == 2);
/*	
	if(res == 1) {
		//emu_printf("SCU DMA COPY FAILED!\n");
	}*/
}
