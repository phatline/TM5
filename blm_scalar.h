#ifndef _BLM_SCALAR_H
#define _BLM_SCALAR_H

#include <mios32_config.h>



#define TM_VERSION 4	// HIER 4 oder 5 eintragen je nach verwendeter Hardware, hauptsächlich werden hier DINDOUT und BLM Pins vertauscht



/////////////////////////////////////////////////////////////////////////////
// Prototypes
/////////////////////////////////////////////////////////////////////////////
extern s32 BLM_SCALAR_Init(u32 mode);
extern s32 BLM_SCALAR_PrepareCol(void);
extern s32 BLM_SCALAR_GetRow(void);
extern s32 BLM_SCALAR_ButtonHandler(void *notify_hook);


// for direct access (bypasses BLM_SCALAR_DOUT_SR* functions)
extern u8 blm_scalar_led[5][8][2][2]; // [Module-Nr] [8] [Color-Nr] [blink or not the blink]

extern u8 BLM_BLINK_ACT;

extern u8 blinky;

#endif /* _BLM_SCALAR_H */
