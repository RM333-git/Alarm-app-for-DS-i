#ifndef IPC_INCLUDE
#define IPC_INCLUDE

//////////////////////////////////////////////////////////////////////

#include <nds.h>
#include <calico/nds/mm_env.h>

//////////////////////////////////////////////////////////////////////


#define AMPM_BIT_NUM 14	// needed even in 24h mode
#define MODE_SHIFT_NUM 24		// mode 0:alarm unset, 1:alarm set, 2:read sound volume, 3:write sound volume (no more mode because pxiSendAndReceive allows 26bit)
#define ALARM_COMPARE_ENABLE (1U<<15) | (1U<<23)
#define ALM_UNSET 0U
#define ALM_SET 1U
#define READ_VOL 2U
#define WRITE_VOL 3U
inline u8 encodeBcd(u8 num)
{
	return (num/10)<<4 | num%10;
}

inline u8 decodeBcd(u8 num)
{
	return (num>>4)*10 + (num&0xf);
}

// in fact, below is IPC definition
enum alarmState {
  None,
  RingOn,
  Ringing
};
 
typedef struct ipcStruct {	// be careful with alignment
	u32 data;	// no need maybe?
	int counter;	// for debug (actually no need)
	u8 almState;	// using alarmState enum 0:none, 1: alarm ring on(when wakeup IRQ_RTC), 2: alarm is ringing
	u8 padding[16];
} __attribute__((aligned(32))) IpcStruct;

#endif


