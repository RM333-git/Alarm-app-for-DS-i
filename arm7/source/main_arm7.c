/*---------------------------------------------------------------------------------

	default ARM7 core

		Copyright (C) 2005 - 2010
		Michael Noland (joat)
		Jason Rogers (dovoto)
		Dave Murphy (WinterMute)

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any
	damages arising from the use of this software.

	Permission is granted to anyone to use this software for any
	purpose, including commercial applications, and to alter it and
	redistribute it freely, subject to the following restrictions:

	1.	The origin of this software must not be misrepresented; you
		must not claim that you wrote the original software. If you use
		this software in a product, an acknowledgment in the product
		documentation would be appreciated but is not required.

	2.	Altered source versions must be plainly marked as such, and
		must not be misrepresented as being the original software.

	3.	This notice may not be removed or altered from any source
		distribution.

---------------------------------------------------------------------------------*/
#include <calico.h>
#include <nds.h>
#include <maxmod7.h>
#include <stdio.h>
#include <calico/nds/arm7/rtc.h>
#include <calico/nds/arm7/gpio.h>
#include <../../ipc.h>

static volatile IpcStruct* IPC;
static volatile int frame = 0;
static volatile bool rtcFlag = false;

void RtcCallback()
{
	// 割込み処理は最小限に
	rtcFlag = true;
}

void setRtcReg(u32 mode, u32 data)
{
	u8 status2 = rtcReadRegister8(RtcReg_Status2);
	status2 &= ~(0x0F);
	if (mode == 0) {	// alarm reset (INT1 Disable)
//		rtcWriteRegister8(RtcReg_Status2, status2 & 0xF0);
		rtcWriteRegister8(RtcReg_Status2, 0);
	} else if (mode == 1) {	// alarm set (Alarm 1 interrupt)
		rtcWriteRegister8(RtcReg_Status2, status2 | RtcInt1Mode_Alarm1);
		status2 = rtcReadRegister8(RtcReg_Status2);
		u32 dataSet = data & 0x00FFFFFF;		// 3 bytes
		rtcWriteRegister(RtcReg_Alarm1Time, &dataSet, 3);	// 3 bytes
		u32 temp = 0;
		rtcReadRegister(RtcReg_Alarm1Time, &temp, 3);	// 3 bytes
	// } else if (mode == 2) {	// alarm test (Per-minute steady interrupt 2 (duty 0.0079 seconds))
	// 	rtcWriteRegister8(RtcReg_Status2, status2 | RtcInt1Mode_MinSteadyPulse);
	}
}

//---------------------------------------------------------------------------------
int main() {
//---------------------------------------------------------------------------------

	// Read settings from NVRAM
	envReadNvramSettings();

	// Set up extended keypad server (X/Y/hinge)
	keypadStartExtServer();

	// Configure and enable VBlank interrupt
	lcdSetIrqMask(DISPSTAT_IE_ALL, DISPSTAT_IE_VBLANK);
	irqEnable(IRQ_VBLANK);

	// Set up RTC
	rtcInit();
	rtcSyncTime();

	// Initialize power management
	pmInit();

	// Set up block device peripherals
	blkInit();

	// Set up touch screen driver
	touchInit();
	touchStartServer(80, MAIN_THREAD_PRIO);

	// Set up sound and mic driver
	soundStartServer(MAIN_THREAD_PRIO-0x10);
	micStartServer(MAIN_THREAD_PRIO-0x18);

	// Set up wireless manager
	wlmgrStartServer(MAIN_THREAD_PRIO-8);

	// Set up Maxmod
	mmInstall(MAIN_THREAD_PRIO+1);


	// receive
	Mailbox mb;
	u32 mb_slots[4];
	mailboxPrepare(&mb, mb_slots, sizeof(mb_slots)/4);
	pxiSetMailbox(PxiChannel_User0, &mb);

	// Receive a message
	u32 msg = mailboxRecv(&mb);
	u32 data = 0;
	rtcReadRegister(RtcReg_Time, &data, 3);
	pxiReply(PxiChannel_User0, data);

	// determine IPC address
	IPC = (volatile IpcStruct*)msg;


	// RTC interrupt setup
	// REG_RCNT = RCNT_MODE_GPIO | RCNT_SI_IRQ_ENABLE | RCNT_SI_DIR_OUT | RCNT_SI;	// 0x8144; (SI input mode is recommended)
	REG_RCNT = RCNT_MODE_GPIO | RCNT_SI_IRQ_ENABLE;	// 0x8100;(recommended) more safe? or battery is low?
	
	// set & enable IRQ
	irqSet(IRQ_RTC, RtcCallback);
	irqEnable(IRQ_RTC);

	// Keep the ARM7 mostly idle
	while (pmMainLoop()) {

		if (rtcFlag) {
			u8 status2 = rtcReadRegister8(RtcReg_Status2);
			rtcWriteRegister8(RtcReg_Status2, status2 & 0xF0);	// reset
			IPC->counter++;
			IPC->almState = RingOn;		// start alarm
			rtcFlag = false;
		}

		// Receive a message
		if (mailboxTryRecv(&mb, &msg)) {
			// something received
			u8 mode = (msg >> MODE_SHIFT_NUM) & 0xFF;
			if ((mode == ALM_UNSET) || (mode == ALM_SET)) {	// alarm reset or set
				setRtcReg(mode, msg);
			} else if (mode == READ_VOL) {		// read system sound volume
				msg = (u32)i2cReadRegister(I2cDev_MCU, I2CREGPM_VOL);
			} else if (mode == WRITE_VOL) {		// write system sound volume(0~31)
				i2cWriteRegister(I2cDev_MCU, I2CREGPM_VOL, msg & 0x3F);		// 0x1F is correct but test
			}
			pxiReply(PxiChannel_User0, msg);
		}

		threadWaitForVBlank();
		frame++;
	}

	return 0;
}

