/*---------------------------------------------------------------------------------

	Simple alarm app by miri

---------------------------------------------------------------------------------*/
#include <nds.h>
#include <stdio.h>
#include <time.h>
#include <calico.h>
#include <maxmod9.h>
#include <fat.h>
#include <../../ipc.h>
#include "soundbank.h"
#include "soundbank_bin.h"

#include "n09_bk.h"
#include "ascii_small_bk.h"
#include "batt_bg_bk.h"
#include "batt_stat_bk.h"
#include "lowerBg_bk.h"
#include "ringing_window_bk.h"
#include "snooze_window_bk.h"
#include "selection.h"

// pxiSendAndReceive can only handle up to 26 bits
// The sprite IDs for oamset are [0 - 127] (common to main and sub)

// sprite IDs
// Upper screen
// 0-4	: big clock (colon:2)
// 5-16	: alarm
// 17-28	: snooze
// 29-45?3	: date
// 46-50	: battery
// Lower screen
// 51-86	: alarm 0-2 strings (12 chars each)
// 87	: alarm sound num
// 88-94	: long press progress
// 95	: selection

// for emulator (Works on melonDS.)
// #define EMU

// save file name
static char saveFileName[] = "alarmApp.sav";

static volatile IpcStruct* IPC;
static IpcStruct mIPC __attribute__((aligned(32)));

#define SEL_MAX 16
static volatile int sel = 0;
static volatile int keyWaitNum = 10;
static volatile int numModWait = 7;
bool noAlarmSet = false;

// alarm variable
static u8 almIsOn;
static u8 almHour;
static u8 almMin;
static u8 almSel = 255;	// alarm element index (unselect:255)
static bool waitSnooze = false;	// wait for snooze
static bool snoozeHold = false;	// holding for reset snooze
static int seNum = 0;	// alarm SE number 
static const int seNumLen = 2;

// current time variable
static u8 h;
static u8 m;
static u8 s;
static u16 year;
static u8 month;
static u8 day;
static u8 week;

#define VOL_MAX 31
static u8 prev_vol;	// To maintain the current volume

static u8 prev_h;	// For display triggers
static u8 prev_m;	// For display triggers
static u8 prev_day;	// For display triggers
static u8 prev_almIsOn;	// For display triggers
static u8 prev_almHour;	// For display triggers
static u8 prev_almMin;	// For display triggers
static bool prev_snoozeOn;	// For display triggers
static u8 prev_snoozeMin;	// For display triggers
static const char weekStrArr[][4] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

#define ELEM_MAX 3
#define SNOOZE_DEFAULT 10
#define SNOOZE_MAX 60
#define UNSNOOZE_FRAME 180	// max:255(because u8)
#define MIN_OF_DAY (24*60)
static volatile struct AlarmElem {
	u8 hour;
	u8 min;
	u8 snoozeMin;
	// u8 seNum??
	bool isOn;
	bool snoozeOn;
} alm[ELEM_MAX];
static u8 unsnoozeCounter = 0;

#define SPRITE_DEP 0	// Depth of sprites on the upper screen
#define NUM_GFX_OFFSET (64*64/2/2)
#define SMALL_GFX_OFFSET (16*16/2/2)
u16* clockGfx;
u16* smallGfx;
u16* battBgGfx;
u16* battStatGfx;
u16* triGfx;
u16* buttonGfx;
u16* smallGfxSub;
u16* selectionGfxSub;
int battStartId, battX, battY;
// Use macros to standardize address calculations.
#define SPRITE_PAL(idx) (SPRITE_PALETTE + ((idx) * 16))
#define SPRITE_PAL_SUB(idx) (SPRITE_PALETTE_SUB + ((idx) * 16))
#define BG_PAL(idx) (BG_PALETTE + ((idx) * 16))
#define BG_PAL_SUB(idx) (BG_PALETTE_SUB + ((idx) * 16))
enum SPRITE_PAL_ID {
	id_n09,
	id_ascii_small,
	id_batt_bg,
	id_batt_stat
};
enum SPRITE_PAL_SUB_ID {
	id_ascii_small_sub,
	id_selection,
	id_up_tri,
	id_button
};
enum BG_PALETTE_SUB_ID {
	id_lowerBg,
	id_ringing_window,
	id_snooze_window
};
// Colon blinking adjustment value
#define COLON_ON_TIME 60
#define COLON_PERIOD 120
static volatile int colonCounter = 0;
int startId, startId2;	// alarm & snooze gfx start index
// For display on the lower screen, etc.
u16* mapPtr;
int colSpanY = 56;
#define TOUCH_AREA_MAX 25
static volatile struct touchAreaStruct {
	u8 x;
	u8 y;
	u8 w;
	u8 h;
} touchArea[TOUCH_AREA_MAX];
#define SELECTION_SPRITE_ID 95

u8 hourCalc(u8 hour, int add)
{
	int result = (int)hour + add;
	if (result >= 24) result = 0;
	else if (result < 0) result = 23;
	return (u8)result;
}
u8 minCalc(u8 min, int add)
{
	int result = (int)min + add;
	if (result >= 60) result = 0;
	else if (result < 0) result = 59;
	return (u8)result;
}
void getTime() {
	time_t timer = time(NULL);
	struct tm *tm = localtime(&timer);
	if(tm != NULL) {
		h = tm->tm_hour;
		m = tm->tm_min;
		s = tm->tm_sec;
		year = tm->tm_year+1900;
		month = tm->tm_mon+1;
		day = tm->tm_mday;
		week = tm->tm_wday;
	}
}
void almInit(void) {
	for(int i=0; i<ELEM_MAX; i++) {
		alm[i].isOn = false;
		alm[i].snoozeOn = false;
		alm[i].hour = 0;
		alm[i].min = 0;
		alm[i].snoozeMin = SNOOZE_DEFAULT;
	}
}
void writeSave(char* fname, void *data, size_t size) {
	FILE *file = fopen(fname, "wb"); // binary write mode
	if (file) {
		fwrite(data, size, 1, file);
		fclose(file);
	}
}
void loadSave(char* fname, void *data, size_t size) {
	FILE *file = fopen(fname, "rb"); // binary loading mode
	if (file) {
		fread(data, size, 1, file);
		fclose(file);
	} else {	// first time load save
		almInit();
	}
}

inline void drawClockNum(int id, int n, int x, int y)
{
	oamSet(&oamMain, 
		id,               // Sprite ID
		x, y,            // Display coordinates
		SPRITE_DEP,               // Priority
		id_n09,               // Palette index
		SpriteSize_64x64, 
		SpriteColorFormat_16Color, 
		clockGfx+n*NUM_GFX_OFFSET,             // Location of transferred data
		-1, false, false, false, false, false);
}
void drawCharSmall(int id, int x, int y, char* str, size_t strLen)
{
	int n, sum = 0;
	u8 span;
	for (int i=0;i<strLen;i++) {
		span = 11;
		n = str[i];
		if ((i == strLen-1) && (n == 0)) break;	// NULL(End of strings)
		if (n==' ') {sum += span; oamClearSprite(&oamMain, id+i); continue;}	// space
		else if (n=='/') {n = ';'; span -= 2;}	// replace character
		else if (n=='(') {n = '?'; span -= 3;}	// replace character
		else if (n==')') {n = '@'; span -= 3;}	// replace character
		else if ((n>='0') && (n<=';')) span -= 1;	// 0-9
		else if ((n=='I')) {span -= 4;}
		else if ((n=='O')||(n=='Q')) {sum += 1; span += 1;}
		else if ((n=='M')) {sum += 2; span += 1;}
		else if ((n=='W')) {sum += 2; span += 2;}
		n -= 48;
		if ((n<0)) n = 0;	// It's also a good idea to set a maximum value.
		oamSet(&oamMain, 
			id+i,               // Sprite ID
			x+sum, y,            // Display coordinates
			SPRITE_DEP,               // Priority
			id_ascii_small,               // Palette index
			SpriteSize_16x16, 
			SpriteColorFormat_16Color, 
			smallGfx+n*SMALL_GFX_OFFSET,             // Location of transferred data
			-1, false, false, false, false, false);
		sum += span;
	}
}
void drawCharSmallSub(int id, int x, int y, char* str, size_t strLen, u8 priority)
{
	int n, sum = 0;
	u8 span;
	for (int i=0;i<strLen;i++) {
		span = 11;
		n = str[i];
		if ((i == strLen-1) && (n == 0)) break;	// NULL(End of strings)
		if (n==' ') {sum += span; oamClearSprite(&oamSub, id+i); continue;}	// space
		else if (n=='/') {n = ';'; span -= 2;}	// replace character
		else if (n=='(') {n = '?'; span -= 3;}	// replace character
		else if (n==')') {n = '@'; span -= 3;}	// replace character
		else if ((n>='0') && (n<=';')) span -= 1;	// 0-9
		else if ((n=='I')) {span -= 4;}
		else if ((n=='O')||(n=='Q')) {sum += 1; span += 1;}
		else if ((n=='M')) {sum += 2; span += 1;}
		else if ((n=='W')) {sum += 2; span += 2;}
		n -= 48;
		if ((n<0)) n = 0;	// It's also a good idea to set a maximum value.
		oamSet(&oamSub, 
			id+i,               // Sprite ID
			x+sum, y,            // Display coordinates
			priority,               // Priority
			id_ascii_small_sub,               // Palette index
			SpriteSize_16x16, 
			SpriteColorFormat_16Color, 
			smallGfxSub+n*SMALL_GFX_OFFSET,             // Location of transferred data
			-1, false, false, false, false, false);
		sum += span;
	}
}
void drawBattStat(u8 statNum)
{
	if (statNum == 0) {
		oamSet(&oamMain, 
			battStartId+4,               // Sprite ID
			battX, battY,            // Display coordinates
			SPRITE_DEP,               // Priority
			id_batt_bg,               // Palette index
			SpriteSize_64x32, 
			SpriteColorFormat_16Color, 
			battBgGfx,             // Location of transferred data
			-1, false, false, false, false, false);
		return;
	}
	u8 i = 0;
	for (int level=3;level<16;level+=4) {
		if (statNum >= level){
			u8 offset = 10*(3-i)+5;
			u8 gfxOffset = 128;
			if (i == 3) {offset = 0; gfxOffset = 0;}
			oamSet(&oamMain, 
				battStartId+i,               // Sprite ID
				battX+offset+2, battY,            // Display coordinates
				SPRITE_DEP,               // Priority
				id_batt_stat,               // Palette index
				SpriteSize_16x32, 
				SpriteColorFormat_16Color, 
				battStatGfx+gfxOffset,             // Location of transferred data
				-1, false, false, false, false, false);
		} else
			oamClearSprite(&oamMain, battStartId+i);
		i++;
	}
}
void drawSelection(int x, int y, s16 scale, u8 priority)
{
	int matrix_id = 0;
	oamSet(&oamSub, 
		SELECTION_SPRITE_ID,               // Sprite ID
		x-16, y-8,            // Display coordinates
		priority,               // Priority
		id_selection,               // Palette index
		SpriteSize_32x16, 
		SpriteColorFormat_16Color, 
		selectionGfxSub,             // Location of transferred data
		matrix_id, true, false, false, false, false);
	// Calculation of the affine matrix (If the scale is 2.0, then 1/2.0 = 0.5. Set 0.5 * 256 = 128.)
	s16 fixed_scale = (s16)(256.0f / scale);
	// Write to matrix register
	oamAffineTransformation(&oamSub, 
							matrix_id, 
							fixed_scale, 0, 
							0, fixed_scale);
}

void searchNearAlmSend(void)
{
	bool anyAlarm = false;
	// search nearest alarm
	u8 nearestInd = 255;	// exception:alarm isnt set
	int now = h*60 + m;
	int nearest = MIN_OF_DAY+1;
	int snoozeM = SNOOZE_MAX;
	for (int i=0; i<ELEM_MAX; i++) {
		if (alm[i].isOn) {
			int comp = alm[i].hour*60 + alm[i].min - now;
			if (comp <= 0) comp += MIN_OF_DAY;
			if (comp < nearest) {nearest = comp; nearestInd = i; snoozeM = alm[i].snoozeOn ? alm[i].snoozeMin : SNOOZE_MAX;}
			else if ((comp == nearest) && alm[i].snoozeOn && (alm[i].snoozeMin < snoozeM)) {nearest = comp; nearestInd = i; snoozeM = alm[i].snoozeMin;}
			anyAlarm = true;
		}
	}
	// alarm set/unset
	u32 data;
	if (anyAlarm) {	// set
		u8 hour = alm[nearestInd].hour;
		u8 min = alm[nearestInd].min;
		u8 ampm = hour/12;
		data = ((alm[nearestInd].isOn & 1U)<< MODE_SHIFT_NUM) | encodeBcd(min)<<16 | encodeBcd(hour)<<8 | ((ampm & 1U)<< AMPM_BIT_NUM) | ALARM_COMPARE_ENABLE;
	} else {	// unset
		data = 0;
		prev_snoozeOn = almSel==255 ? false : !alm[almSel].snoozeOn;
		prev_snoozeMin = 255;
	}
	u32 ret = (u32)(0xfffffff);
	if (!(data == 0 && almIsOn == 0)) {		// except for no alarm and not set alarm now
		pxiSendAndReceive(PxiChannel_User0, 0);		// important for write 0 to status2 register to reset IF flag(maybe)
		ret = pxiSendAndReceive(PxiChannel_User0, data);
	}
	if (ret != data) {
		// not succeeded
	} else {
		// decode and preserve data
		almIsOn = (ret >> MODE_SHIFT_NUM) & 1U;
		almHour = decodeBcd((ret >> 8) & 0x3f);
		almMin = decodeBcd((ret >> 16) & 0x7f);
		almSel = nearestInd;
	}
}
bool checkValidSnooze(int min)	// for snooze
{
	// check snooze valid
	int now = h*60 + m;
	int tempAlm = almHour*60 + almMin + min;
	int diff = (tempAlm - now + MIN_OF_DAY) % MIN_OF_DAY;	// calc difference clockwise
	if (diff > 0 && diff <= SNOOZE_MAX) {return true;}
	return false;
}
void addMinAndSend(int min)	// for snooze
{
	// add minute
	u8 carry = 0;
	almMin += min;
	if (almMin >= 60) {almMin -= 60; carry = 1;}
	almHour += carry;
	if (almHour >= 24) {almHour -= 24;}
	// set snooze
	pxiSendAndReceive(PxiChannel_User0, 0);		// important for write 0 to status2 register to reset IF flag(maybe)
	u8 ampm = almHour/12;
	u32 data = (ALM_SET<< MODE_SHIFT_NUM) | encodeBcd(almMin)<<16 | encodeBcd(almHour)<<8 | ((ampm & 1U)<< AMPM_BIT_NUM) | ALARM_COMPARE_ENABLE;
	u32 ret = pxiSendAndReceive(PxiChannel_User0, data);
	if (ret != data) {
		// not succeeded
	} else {
		// decode and preserve data
		almHour = decodeBcd((ret >> 8) & 0x3f);
		almMin = decodeBcd((ret >> 16) & 0x7f);
	}
}
inline bool my_pmMainLoop() {
	return !noAlarmSet ? pmMainLoop() : true;
}
//---------------------------------------------------------------------------------
int main(void) {
//---------------------------------------------------------------------------------
	touchPosition touch;

	consoleDemoInit();  //setup the sub screen for printing

	// IPC init
	mIPC.counter = 0;
	mIPC.data = 0;
	mIPC.almState = None;
	IPC = &mIPC;

	// send
	pxiWaitRemote(PxiChannel_User0);	// need to call this once.
	DC_FlushRange(&mIPC, sizeof(mIPC));
	pxiSendAndReceive(PxiChannel_User0, (u32)IPC);

	// initialize needed
	mmInitDefaultMem((mm_addr)soundbank_bin);

	// load sound effects
	for (int i=0;i<seNumLen;i++) {
		mmLoadEffect(i);
	}
	// sound effect handle (for cancelling it later)
	mm_sfxhand amb = 0;

#ifndef EMU
	if (fatInitDefault()) {
		iprintf("\x1b[14;0HfatInitDefault OK\n");
		// alarm elements initialize
		loadSave(saveFileName, (void *)alm, sizeof(alm));
	} else {
		iprintf("\x1b[14;0HfatInitDefault failure\n");
	}
#else
	almInit();
#endif
	getTime();	// because searchNearAlmSend uses current time internally
	searchNearAlmSend();	// initialize alm var
	prev_h = 255;	// to display first time
	prev_m = 255;	// to display first time
	prev_day = 255;	// to display first time
	prev_almIsOn = false;	// to display first time if set
	prev_almHour = almHour;
	prev_almMin = almMin;
	prev_snoozeOn = almSel==255 ? false : alm[almSel].snoozeOn;
	prev_snoozeMin = 255;

	// draw upper screen
	videoSetMode(MODE_0_2D | DISPLAY_SPR_ACTIVE);
	// Set VRAM for sprites
	vramSetBankA(VRAM_A_MAIN_SPRITE);
	// Initialization of sprite memory
	oamInit(&oamMain, SpriteMapping_1D_128, false);
	// Sub-engine (lower screen) settings
	videoSetModeSub(MODE_0_2D | DISPLAY_SPR_ACTIVE);
	vramSetBankC(VRAM_C_SUB_BG);
	vramSetBankD(VRAM_D_SUB_SPRITE);
	vramSetBankI(VRAM_I_SUB_SPRITE_EXT_PALETTE);
	oamInit(&oamSub, SpriteMapping_1D_128, false);
	// Transferring image data to VRAM: It's actually not a good idea to just run `allocate` without any memory allocation (because it can result in isolated memory locations depending on the previous memory layout) (it's better to store the addresses using an array or similar method).
	clockGfx = oamAllocateGfx(&oamMain, SpriteSize_64x64, SpriteColorFormat_16Color);	// Reserve 11 areas of 64x64 (first area)
	for (int i=0;i<10;i++){		// Reserve 10 more 64x64 areas.
		oamAllocateGfx(&oamMain, SpriteSize_64x64, SpriteColorFormat_16Color);
	}
	dmaCopy(n09_bkTiles, clockGfx, n09_bkTilesLen);
	dmaCopy(n09_bkPal, SPRITE_PAL(id_n09), n09_bkPalLen);	
	smallGfx = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_16Color);	// Reserve 43 areas of 16x16 (first area)
	for (int i=0;i<42;i++){		// Reserve 42 more 16x16 areas.
		oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_16Color);
	}
	dmaCopy(ascii_small_bkTiles, smallGfx, ascii_small_bkTilesLen);
	dmaCopy(ascii_small_bkPal, SPRITE_PAL(id_ascii_small), ascii_small_bkPalLen);	

	// battery mark prepare and draw batt_bg
	battBgGfx = oamAllocateGfx(&oamMain, SpriteSize_64x32, SpriteColorFormat_16Color);
	dmaCopy(batt_bg_bkTiles, battBgGfx, batt_bg_bkTilesLen);
	dmaCopy(batt_bg_bkPal, SPRITE_PAL(id_batt_bg), batt_bg_bkPalLen);	
	battStatGfx = oamAllocateGfx(&oamMain, SpriteSize_16x32, SpriteColorFormat_16Color);
	oamAllocateGfx(&oamMain, SpriteSize_16x32, SpriteColorFormat_16Color);	// Since there are two tiles in one image.
	dmaCopy(batt_stat_bkTiles, battStatGfx, batt_stat_bkTilesLen);
	dmaCopy(batt_stat_bkPal, SPRITE_PAL(id_batt_stat), batt_stat_bkPalLen);	

	int clockX = 15, clockY = 70;
	u16 wid = 40, colonWid = 30;
	battX = 200, battY = 155;
	battStartId = 46;
	drawBattStat(0);	// draw batt bg
	// get battery level and draw
	u8 battLevel = PM_BATT_LEVEL(pmGetBatteryState());
	drawBattStat(battLevel);

	// Preparing sprites and backgrounds for the lower screen

	// Transferring image data to VRAM: It's actually not a good idea to just run `allocate` without any memory allocation (because it can result in isolated memory locations depending on the previous memory layout) (it's better to store the addresses using an array or similar method).
	smallGfxSub = oamAllocateGfx(&oamSub, SpriteSize_16x16, SpriteColorFormat_16Color);	// Reserve 43 areas of 16x16 (first area)
	for (int i=0;i<42;i++)		// Reserve 42 more 16x16 areas.
		oamAllocateGfx(&oamSub, SpriteSize_16x16, SpriteColorFormat_16Color);
	dmaCopy(ascii_small_bkTiles, smallGfxSub, ascii_small_bkTilesLen);
	dmaCopy(ascii_small_bkPal, SPRITE_PAL_SUB(id_ascii_small_sub), ascii_small_bkPalLen);
	selectionGfxSub = oamAllocateGfx(&oamSub, SpriteSize_32x16, SpriteColorFormat_16Color);	// For cursor image
	dmaCopy(selectionTiles, selectionGfxSub, selectionTilesLen);
	dmaCopy(selectionPal, SPRITE_PAL_SUB(id_selection), selectionPalLen);
	// Display the background of the lower screen (Layer 0 (not depth))
	int bg0sub = bgInitSub(0, BgType_Text4bpp, BgSize_T_256x256, 16, 0);	// mapBase and tileBase should not collide.
	dmaCopy(lowerBg_bkTiles, bgGetGfxPtr(bg0sub), lowerBg_bkTilesLen);
	dmaCopy(lowerBg_bkMap, bgGetMapPtr(bg0sub), lowerBg_bkMapLen);
	dmaCopy(lowerBg_bkPal, BG_PAL_SUB(id_lowerBg), lowerBg_bkPalLen);
	bgSetPriority(bg0sub, 2);	// Set depth to 2
	// ringing(Layer 1 (not depth))
	int bgRinging = bgInitSub(1, BgType_Text4bpp, BgSize_T_256x256, 17, 1);	// mapBase and tileBase should not collide.
	mapPtr = bgGetMapPtr(bgRinging);
	dmaCopy(ringing_window_bkTiles, bgGetGfxPtr(bgRinging), ringing_window_bkTilesLen);
	dmaCopy(ringing_window_bkMap, mapPtr, ringing_window_bkMapLen);
	dmaCopy(ringing_window_bkPal, BG_PAL_SUB(id_ringing_window), ringing_window_bkPalLen);
	for (int i = 0; i < 32 * 32; i++)
		mapPtr[i] = (mapPtr[i] & 0x0FFF) | (id_ringing_window << 12); 	// Assign a palette slot to each tile in the modal map.
	bgSetPriority(bgRinging, 0);	// Set depth to 0
	bgHide(bgRinging);
	// snooze(Layer 2 (not depth))
	int bgSnooze = bgInitSub(2, BgType_Text4bpp, BgSize_T_256x256, 18, 5);	// mapBase and tileBase should not collide.
	mapPtr = bgGetMapPtr(bgSnooze);
	dmaCopy(snooze_window_bkTiles, bgGetGfxPtr(bgSnooze), snooze_window_bkTilesLen);
	dmaCopy(snooze_window_bkMap, mapPtr, snooze_window_bkMapLen);
	dmaCopy(snooze_window_bkPal, BG_PAL_SUB(id_snooze_window), snooze_window_bkPalLen);
	for (int i = 0; i < 32 * 32; i++)
		mapPtr[i] = (mapPtr[i] & 0x0FFF) | (id_snooze_window << 12); 	// Assign a palette slot to each tile in the modal map.
	bgSetPriority(bgSnooze, 0);	// Set depth to 0
	bgHide(bgSnooze);

	// Touch area settings
	u8 ind = 0; u8 mar = 2;
	// alm ON/OFF, hour inc, hour dec, min inc, min dec, snz ON/OFF, snz min inc, snz min dec
	u8 touch8X[] = {8,61,61,95,95,135,203,203}, touch8Y[] = {17,8,40,8,40,26,8,40};
	u8 touch8W[] = {45,15,15,15,15,45,15,15}, touch8H[] = {22,8,8,8,8,22,8,8};
	for (int i=0;i<ELEM_MAX;i++) {
		int offsetLowerY = colSpanY*i;	// Calculate which row
		for (int j=0;j<8;j++) {
			touchArea[ind].x=touch8X[j]-mar; touchArea[ind].y=touch8Y[j]+offsetLowerY-mar; touchArea[ind].w=touch8W[j]+2*mar; touchArea[ind].h=touch8H[j]+2*mar; ind++;
		}
	}
	touchArea[ind].x=70-mar; touchArea[ind].y=173-mar; touchArea[ind].w=15+2*mar; touchArea[ind].h=15+2*mar;	// sound

	int keyWait = 0;
	int touchWait = 0;
	sel = -1;	// deselect

	while(my_pmMainLoop()) {

		swiWaitForVBlank();
		scanKeys();
		DC_InvalidateRange(&mIPC, sizeof(mIPC));
		// get current time
		getTime();
		// draw clock
		if ((prev_h != h)) {
			if (h>=10) drawClockNum(0,h/10,clockX,clockY);
			else oamClearSprite(&oamMain, 0);
			drawClockNum(1,h%10,clockX+wid,clockY);
		}
		if ((prev_m != m)) {
			drawClockNum(3,m/10,clockX+2*wid+colonWid,clockY);
			drawClockNum(4,m%10,clockX+3*wid+colonWid,clockY);
		}
		// colon blinking
		if (colonCounter == 0) {	// on
			drawClockNum(2,10,clockX+2*wid,clockY);	// colon
			// get battery level and draw
			u8 battLevel = PM_BATT_LEVEL(pmGetBatteryState());
			drawBattStat(battLevel);
		} else if (colonCounter == COLON_ON_TIME) {	// off
			oamClearSprite(&oamMain, 2);
		}
		// display alarm
		if ((prev_almIsOn != almIsOn)||(prev_almHour != almHour)||(prev_almMin != almMin)) {
			startId = 5;
			if (almIsOn) {
				char almStr[15] = "";
				sprintf(almStr, "ALARM  %02i:%02i", almHour, almMin);
				drawCharSmall(startId,115,5,almStr,strlen(almStr));
			} else {
				for (int i=0;i<12;i++) {oamClearSprite(&oamMain, startId+i);}
				for (int i=0;i<12;i++) {oamClearSprite(&oamMain, startId2+i);}
			}
		}
		if ((almSel != 255) && ((prev_snoozeOn != alm[almSel].snoozeOn)||(prev_snoozeMin != alm[almSel].snoozeMin))) {
			startId2 = 17;
			if (alm[almSel].snoozeOn) {
				char snzStr[11] = "";
				sprintf(snzStr, "SNOOZE %2i", alm[almSel].snoozeMin);
				drawCharSmall(startId2,115,25,snzStr,strlen(snzStr));
				drawCharSmall(startId2+9,218,25,"MIN",3);
			} else {
				for (int i=0;i<12;i++) {oamClearSprite(&oamMain, startId2+i);}
			}
		}
		// now date
		if ((prev_day != day)) {
			char date[18] = "";
			sprintf(date, "%4i/%i/%i(%s)  ", year, month, day, weekStrArr[week]);
			int startId3 = 29;
			drawCharSmall(startId3,10,170,date,strlen(date));
		}
		// save previous value for display trigger
		prev_h = h;
		prev_m = m;
		prev_day = day;
		prev_almIsOn = almIsOn;
		prev_almHour = almHour;
		prev_almMin = almMin;
		prev_snoozeOn = almSel==255 ? false : alm[almSel].snoozeOn;
		prev_snoozeMin = almSel==255 ? 255 : alm[almSel].snoozeMin;

		// alarm watchdog
		if (IPC->almState == RingOn) {
			if (isDSiMode()) {
				prev_vol = 0xFF & pxiSendAndReceive(PxiChannel_User0, READ_VOL << MODE_SHIFT_NUM);
				pxiSendAndReceive(PxiChannel_User0, (WRITE_VOL << MODE_SHIFT_NUM) | VOL_MAX);	// set sound volume max
			}
			amb = mmEffect(seNum);
			IPC->almState = Ringing;
			DC_FlushRange(&mIPC, sizeof(mIPC));
			bgShow(bgRinging);
			sel = -1;	// deselect
		}

		touchRead(&touch);	// read the touchscreen coordinates
		int down = keysDown();
		int held = keysHeld();
		int up = keysUp();
		char tempDisp[8] = "";	// for display

		// stop snooze and reset alarm
		if ((IPC->almState == None) && (waitSnooze)) {
			if ((held&KEY_A)) {
				unsnoozeCounter++;
				sprintf(tempDisp, "%3i/%3i", unsnoozeCounter, UNSNOOZE_FRAME);
				drawCharSmallSub(88,140,134,tempDisp,strlen(tempDisp),0);
				snoozeHold = true;
			} else {
				unsnoozeCounter = 0;
				drawCharSmallSub(88,140,134,"       ",7,0);
				snoozeHold = false;
			}
			if ((unsnoozeCounter >= UNSNOOZE_FRAME)) {
				searchNearAlmSend();	// reset alarm
				waitSnooze = false;
				unsnoozeCounter = 0;
				bgHide(bgSnooze);
				drawCharSmallSub(88,140,134,"       ",7,0);
			}
		}
		if ((up&KEY_A)) {snoozeHold = false;}	// not to move selection
		// stop alarm
		if ((down&KEY_B) || (down&KEY_LID)) {
			if (IPC->almState == Ringing) {
				bgHide(bgSnooze);
				bgHide(bgRinging);
				// stop SFX
				mmEffectCancel(amb);
				if (isDSiMode()) {
					u8 temp = 0xFF & pxiSendAndReceive(PxiChannel_User0, READ_VOL << MODE_SHIFT_NUM);
					if (temp != VOL_MAX) prev_vol = temp;
					pxiSendAndReceive(PxiChannel_User0, (WRITE_VOL << MODE_SHIFT_NUM) | prev_vol);	// restore sound volume
				}
				// Whether the alarm has expired (always false if snooze is OFF)
				bool validSnooze = alm[almSel].snoozeOn ? checkValidSnooze(alm[almSel].snoozeMin) : false;
				if (alm[almSel].snoozeOn && validSnooze) {
					waitSnooze = true;
					addMinAndSend(alm[almSel].snoozeMin);
					bgShow(bgSnooze);
				} else {
					waitSnooze = false;
					searchNearAlmSend();	// reset alarm
				}
				IPC->almState = None;
				DC_FlushRange(&mIPC, sizeof(mIPC));
			}
		}
		// Sound an alarm and flag the alarm if no alarms are set.
		bool noAlmTemp = true;
		for (int i=0;i<ELEM_MAX;i++) 
			if (alm[i].isOn) noAlmTemp = false;
		if (noAlmTemp && (down&KEY_LID)) {
			noAlarmSet = true;
			if (isDSiMode()) {
				prev_vol = 0xFF & pxiSendAndReceive(PxiChannel_User0, READ_VOL << MODE_SHIFT_NUM);
				pxiSendAndReceive(PxiChannel_User0, (WRITE_VOL << MODE_SHIFT_NUM) | VOL_MAX);	// set sound volume max
			}
			amb = mmEffect(seNum);
		} else if (noAlarmSet && (up&KEY_LID)) {
			noAlarmSet = false;
			mmEffectCancel(amb);
			if (isDSiMode()) {
				u8 temp = 0xFF & pxiSendAndReceive(PxiChannel_User0, READ_VOL << MODE_SHIFT_NUM);
				if (temp != VOL_MAX) prev_vol = temp;
				pxiSendAndReceive(PxiChannel_User0, (WRITE_VOL << MODE_SHIFT_NUM) | prev_vol);	// restore sound volume
			}
			swiWaitForVBlank();
			swiWaitForVBlank();
			swiWaitForVBlank();		// Avoid screen glitches
		}

		if (keyWait <= 0) {	// Move key processing
			if (!snoozeHold && !waitSnooze && !(IPC->almState == Ringing)) {
				if ((down&KEY_RIGHT) || (held&KEY_RIGHT) || (down&KEY_A) || (held&KEY_A)) {
					sel++;
					if (sel >= SEL_MAX) sel = 0;
					keyWait = keyWaitNum;
				}
				if ((down&KEY_LEFT) || (held&KEY_LEFT) || (down&KEY_Y) || (held&KEY_Y)) {
					if (sel <= 0) sel = SEL_MAX;
					sel--;
					keyWait = keyWaitNum;
				}
				if ((down&KEY_UP) || (held&KEY_UP) || (down&KEY_X) || (held&KEY_X)) {
					u8 selMod = sel%5;
					u8 i = sel/5;
					if (sel == 15) seNum = ++seNum >= seNumLen ? 0 : seNum;
					else if (selMod == 0) {alm[i].isOn = alm[i].isOn ? 0 : 1; searchNearAlmSend();}
					else if (selMod == 1) {alm[i].hour = hourCalc(alm[i].hour, 1); searchNearAlmSend();}
					else if (selMod == 2) {alm[i].min = minCalc(alm[i].min, 1); searchNearAlmSend();}
					else if (selMod == 3) {alm[i].snoozeOn = alm[i].snoozeOn ? 0 : 1; searchNearAlmSend();}
					else if (selMod == 4) {alm[i].snoozeMin++;if(alm[i].snoozeMin > SNOOZE_MAX) alm[i].snoozeMin = 1; searchNearAlmSend();}
	#ifndef EMU
					writeSave(saveFileName, (void *)alm, sizeof(alm));
	#endif
					keyWait = (selMod == 0) || (selMod == 3) ? keyWaitNum : numModWait;
				}
				if ((down&KEY_DOWN) || (held&KEY_DOWN) || (down&KEY_B) || (held&KEY_B)) {
					u8 selMod = sel%5;
					u8 i = sel/5;
					if (sel == 15) seNum = --seNum < 0 ? seNumLen-1 : seNum;
					else if (selMod == 0) {alm[i].isOn = alm[i].isOn ? 0 : 1; searchNearAlmSend();}
					else if (selMod == 1) {alm[i].hour = hourCalc(alm[i].hour, -1); searchNearAlmSend();}
					else if (selMod == 2) {alm[i].min = minCalc(alm[i].min, -1); searchNearAlmSend();}
					else if (selMod == 3) {alm[i].snoozeOn = alm[i].snoozeOn ? 0 : 1; searchNearAlmSend();}
					else if (selMod == 4) {if (alm[i].snoozeMin <= 1) alm[i].snoozeMin = SNOOZE_MAX+1;alm[i].snoozeMin--; searchNearAlmSend();}
	#ifndef EMU
					writeSave(saveFileName, (void *)alm, sizeof(alm));
	#endif
					keyWait = (selMod == 0) || (selMod == 3) ? keyWaitNum : numModWait;
				}
				if ((down&KEY_SELECT)) {sel = -1; keyWait = keyWaitNum;}	// deselect
			}
			if ((down&KEY_START)) {REG_POWERCNT ^= POWER_SWAP_LCDS; keyWait = keyWaitNum;}	// Swap the upper and lower displays.
		}
		// Handling touch events
		if ((down & KEY_TOUCH) && !waitSnooze && (IPC->almState != Ringing))
			for (int i=0;i<TOUCH_AREA_MAX;i++) {
				if ((touch.px >= touchArea[i].x)&&(touch.px <= touchArea[i].x+touchArea[i].w)&&
				(touch.py >= touchArea[i].y)&&(touch.py <= touchArea[i].y+touchArea[i].h)){
					u8 row = i/8, col = i%8;
					if (row == 3) seNum = ++seNum >= seNumLen ? 0 : seNum;
					else if (col == 0) {alm[row].isOn = alm[row].isOn ? 0 : 1; searchNearAlmSend();}
					else if (col == 1) {alm[row].hour = hourCalc(alm[row].hour, 1); searchNearAlmSend();}
					else if (col == 2) {alm[row].hour = hourCalc(alm[row].hour, -1); searchNearAlmSend();}
					else if (col == 3) {alm[row].min = minCalc(alm[row].min, 1); searchNearAlmSend();}
					else if (col == 4) {alm[row].min = minCalc(alm[row].min, -1); searchNearAlmSend();}
					else if (col == 5) {alm[row].snoozeOn = alm[row].snoozeOn ? 0 : 1; searchNearAlmSend();}
					else if (col == 6) {alm[row].snoozeMin++;if(alm[row].snoozeMin > SNOOZE_MAX) alm[row].snoozeMin = 1; searchNearAlmSend();}
					else if (col == 7) {if (alm[row].snoozeMin <= 1) alm[row].snoozeMin = SNOOZE_MAX+1;alm[row].snoozeMin--; searchNearAlmSend();}
					touchWait = 2*numModWait;	// wait after touch
#ifndef EMU
					writeSave(saveFileName, (void *)alm, sizeof(alm));
#endif
				}
			}
		// touch and hold
		if ((touchWait <= 0) && (held & KEY_TOUCH) && !waitSnooze && (IPC->almState != Ringing))
			for (int i=0;i<TOUCH_AREA_MAX;i++) {
				if ((touch.px >= touchArea[i].x)&&(touch.px <= touchArea[i].x+touchArea[i].w)&&
				(touch.py >= touchArea[i].y)&&(touch.py <= touchArea[i].y+touchArea[i].h)){
					u8 row = i/8, col = i%8;
					// if (row == 3) seNum = ++seNum >= seNumLen ? 0 : seNum;
					// else if (col == 0) {alm[row].isOn = alm[row].isOn ? 0 : 1; searchNearAlmSend();}
					if (col == 1) {alm[row].hour = hourCalc(alm[row].hour, 1); searchNearAlmSend();}
					else if (col == 2) {alm[row].hour = hourCalc(alm[row].hour, -1); searchNearAlmSend();}
					else if (col == 3) {alm[row].min = minCalc(alm[row].min, 1); searchNearAlmSend();}
					else if (col == 4) {alm[row].min = minCalc(alm[row].min, -1); searchNearAlmSend();}
					// else if (col == 5) {alm[row].snoozeOn = alm[row].snoozeOn ? 0 : 1; searchNearAlmSend();}
					else if (col == 6) {alm[row].snoozeMin++;if(alm[row].snoozeMin > SNOOZE_MAX) alm[row].snoozeMin = 1; searchNearAlmSend();}
					else if (col == 7) {if (alm[row].snoozeMin <= 1) alm[row].snoozeMin = SNOOZE_MAX+1;alm[row].snoozeMin--; searchNearAlmSend();}
					touchWait = numModWait;
				}
			}

		// Alarm settings display on the lower screen
		int startLowerId = 51;
		// (alm ON/OFF, hour, min, snz ON/OFF, snz min)*ELEM_MAX, sound
		u8 settingX[] = {12,56,90,139,198,70}, settingY[] = {20,20,20,29,20,173};
		for (int i=0;i<ELEM_MAX;i++) {
			int offsetLowerY = colSpanY*i;	// Calculate which row
			sprintf(tempDisp, "%s", alm[i].isOn ? "ON " : "OFF");
			drawCharSmallSub(startLowerId,settingX[0],settingY[0]+offsetLowerY,tempDisp,strlen(tempDisp),1);	// Alarm ON/OFF text
			sprintf(tempDisp, "%02i", alm[i].hour);
			drawCharSmallSub(startLowerId+3,settingX[1],settingY[1]+offsetLowerY,tempDisp,strlen(tempDisp),1);	// Alarm hour text
			sprintf(tempDisp, "%02i", alm[i].min);
			drawCharSmallSub(startLowerId+5,settingX[2],settingY[2]+offsetLowerY,tempDisp,strlen(tempDisp),1);	// Alarm minute text
			sprintf(tempDisp, "%s", alm[i].snoozeOn ? "ON " : "OFF");
			drawCharSmallSub(startLowerId+7,settingX[3],settingY[3]+offsetLowerY,tempDisp,strlen(tempDisp),1);	// Snooze ON/OFF text
			sprintf(tempDisp, "%2i", alm[i].snoozeMin);
			drawCharSmallSub(startLowerId+10,settingX[4],settingY[4]+offsetLowerY,tempDisp,strlen(tempDisp),1);	// Snooze min text
			startLowerId += 12;
		}
		sprintf(tempDisp, "%i", seNum);
		drawCharSmallSub(startLowerId+36,settingX[5],settingY[5],tempDisp,strlen(tempDisp),1);

		// draw selection
		if (sel >= 0) {
			int selButtonOffsetX = 10, selButtonOffsetY = 0;
			u8 selRow = sel/5, selCol = sel%5;
			if (selRow == 3) drawSelection(settingX[5]-6,settingY[5],1,1);	// sound
			else {
				int offsetLowerY = colSpanY*selRow;	// Calculate which row
				if ((selCol == 0) || (selCol == 3))
					drawSelection(settingX[selCol]+selButtonOffsetX,settingY[selCol]+selButtonOffsetY+offsetLowerY,2,1);	// alm ON/OFF, snz ON/OFF
				if ((selCol == 1) || (selCol == 2) || (selCol == 4))
					drawSelection(settingX[selCol],settingY[selCol]+offsetLowerY,1,1);	// hour, min, snz min
			}
		} else if (sel == -1) oamClearSprite(&oamSub, SELECTION_SPRITE_ID);

		oamUpdate(&oamMain); // Update drawing information during VBlank.
		oamUpdate(&oamSub); // Update drawing information during VBlank.

		colonCounter++;
		if (colonCounter >= COLON_PERIOD) colonCounter = 0;

		if (keyWait > 0) keyWait--;
		if (touchWait > 0) touchWait--;

	}

	return 0;
}
