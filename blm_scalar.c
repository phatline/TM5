#include <mios32.h>
#include "blm_scalar.h"

// for direct access (bypasses BLM_SCALAR_DOUT_SR* functions)
u8 BLM_BLINK_ACT;
u8 blm_scalar_led[5][8][2][2];	//[num modules][8][num colour] [blink  or  not the blink]

u8 blinky;	// in order to use the Blinking colour, it has to be toggled with this variable (toggle this var from 0 to 1, with a sequencer or a simple milliseconds counter...)

static u8 blm_scalar_selected_row;

static u8 blm_scalar_button_debounce_ctr;

static u8 blm_scalar_button_row_values[5][8]; // [num modules][8]
static u8 blm_scalar_button_row_changed[5][8];// [num modules][8]
static u8 cathodes_inv_mask;

		// SR CHAIN   EXPLAIN ///////////////////////////////////////////////

		// BLM:::::::::::::::::::::::::::::
		// MOD 0  SR: 0 Common	Cathode
		// MOD 0  SR: 1 Green	Color
		// MOD 0  SR: 2 Blue	Color
		//
		// MOD 1  SR: 3 Common	Cathode
		// MOD 1  SR: 4 Green	Color
		// MOD 1  SR: 5 Blue	Color
		//
		// MOD 2  SR: 6 Common	Cathode
		// MOD 2  SR: 7 Green	Color
		// MOD 2  SR: 8 Blue	Color
		//
		// MOD 3  SR: 9 Common	Cathode
		// MOD 3  SR: 10 Green	Color
		// MOD 3  SR: 11 Blue	Color
		//
		// MOD 4  SR: 12 Common	Cathode		// Extra H
		// MOD 4  SR: 13 Green	Color		// Extra H-V Color
		// MOD 4  SR: 14 Blue	Color		// Extra H-V Color

		// DINX4  SR: 15 16 17 18




/////////////////////////////////////////////////////////////////////////////
//! Initializes the BLM_SCALAR driver
//! Should be called from Init() during startup
/////////////////////////////////////////////////////////////////////////////
s32 BLM_SCALAR_Init(u32 mode)	{

	  cathodes_inv_mask = 0xff; // sink drivers connected


	  // set button value to initial value (1) and clear "changed" status of all 64 buttons
	  // clear all LEDs
	  int mod;
	  for(mod=0; mod<5; ++mod) { // 5: Module-Nr
								int row;

								for(row=0; row<8; ++row) {	blm_scalar_button_row_values[mod][row]  = 0xff;
															blm_scalar_button_row_changed[mod][row] = 0x00;

															int colour;
															for(colour=0; colour<2; ++colour)	{ 	blm_scalar_led[mod][row][colour][0] = 0x00;	// 2: Colour-Nr		[0]: Not blinking
																									blm_scalar_led[mod][row][colour][1] = 0x00;}// 2: Colour-Nr		[1]: Not blinking
								}
								}

		// clear debounce counter
		blm_scalar_button_debounce_ctr = 0;

#if TM_VERSION == 5
		// Common Catodes
		MIOS32_DOUT_SRSet(0,	(1 << 0) ^ cathodes_inv_mask);
		MIOS32_DOUT_SRSet(3,	(1 << 0) ^ cathodes_inv_mask);
		MIOS32_DOUT_SRSet(6,	(1 << 0) ^ cathodes_inv_mask);
		MIOS32_DOUT_SRSet(9,	(1 << 0) ^ cathodes_inv_mask);
		MIOS32_DOUT_SRSet(12,	(1 << 0) ^ cathodes_inv_mask);

#elif TM_VERSION == 4
		// Common Catodes
		MIOS32_DOUT_SRSet(4,	(1 << 0) ^ cathodes_inv_mask);
		MIOS32_DOUT_SRSet(7,	(1 << 0) ^ cathodes_inv_mask);
		MIOS32_DOUT_SRSet(10,	(1 << 0) ^ cathodes_inv_mask);
		MIOS32_DOUT_SRSet(13,	(1 << 0) ^ cathodes_inv_mask);
		MIOS32_DOUT_SRSet(16,	(1 << 0) ^ cathodes_inv_mask);
#endif


		// remember that this row has been selected
		blm_scalar_selected_row = 0;

	  return 0;
}




/////////////////////////////////////////////////////////////////////////////
//! This function prepares the DOUT register to drive a row						DOUT-SR-OFFSET HERE!!!!!!!!!!!!!!!
//! should be called from   APP_SRIO_ServicePrepare()
/////////////////////////////////////////////////////////////////////////////
s32 BLM_SCALAR_PrepareCol(void)	{

		// increment row, wrap at 8
		if( ++blm_scalar_selected_row >= 8 )	{ blm_scalar_selected_row = 0; }

		// select next DOUT/DIN row (selected cathode line = 0, all others 1)
		u8 dout_value = ~(1 << blm_scalar_selected_row);

		// apply inversion mask
		dout_value ^= 0xff;


/////////////////////// H A R D W A R E    Versions ////////////////////////////////////77
	#if TM_VERSION == 5
		// Common Cathodes ///////////////////////////////////////////////////////////////////
		MIOS32_DOUT_SRSet(0,	dout_value);
		MIOS32_DOUT_SRSet(3,	dout_value);
		MIOS32_DOUT_SRSet(6,	dout_value);
		MIOS32_DOUT_SRSet(9,	dout_value);
		MIOS32_DOUT_SRSet(12,	dout_value);

	#elif TM_VERSION == 4
		// Common Cathodes ///////////////////////////////////////////////////////////////////
		MIOS32_DOUT_SRSet(4,	dout_value);
		MIOS32_DOUT_SRSet(7,	dout_value);
		MIOS32_DOUT_SRSet(10,	dout_value);
		MIOS32_DOUT_SRSet(13,	dout_value);
		MIOS32_DOUT_SRSet(16,	dout_value);
	#endif



		// Colors  ////////////////////////////////////////////////////////////////////////////
		int val;
		int mod;
		for(mod=0;mod<5;++mod){



								// BLUE			// Module        8 Rows            Color   Blink
								u8 STEPs= blm_scalar_led[mod][blm_scalar_selected_row][0][0];

								// BLINKING
								// "blinky" is a variable that turns ON/OFF in a Interval  BLM_BLIN_ACT  activates this Feature
								if(blinky && BLM_BLINK_ACT){	u8 FLAM = blm_scalar_led[mod][blm_scalar_selected_row][0][1];

																val = FLAM ^ STEPs;	// compare FLAM and STEPs    with   Bitwise XOR!
																}
								// No BLINKING
								else { val = STEPs; }

								// Update Hardware
								#if TM_VERSION == 5

											switch(mod) {	// Update Hardware
															case 0:	MIOS32_DOUT_SRSet(1,val);	break;
															case 1:	MIOS32_DOUT_SRSet(4,val);	break;
															case 2:	MIOS32_DOUT_SRSet(7,val);	break;
															case 3:	MIOS32_DOUT_SRSet(10,val);	break;
															default:MIOS32_DOUT_SRSet(13,val);	break;
															break;
															}
								#elif TM_VERSION == 4
											switch(mod) {	// Update Hardware
															case 0:	MIOS32_DOUT_SRSet(5,val);	break;
															case 1:	MIOS32_DOUT_SRSet(8,val);	break;
															case 2:	MIOS32_DOUT_SRSet(11,val);	break;
															case 3:	MIOS32_DOUT_SRSet(14,val);	break;
															default:MIOS32_DOUT_SRSet(17,val);	break;
															break;
															}
								#endif






								// GREEN	// 			Module        8 Rows            Color   Blink
								STEPs= blm_scalar_led[mod][blm_scalar_selected_row][1][0];

								// BLINKING
								// "blinky" is a variable that turns ON/OFF in a Interval  BLM_BLIN_ACT  activates this Feature
								if(blinky && BLM_BLINK_ACT){	u8 FLAM = blm_scalar_led[mod][blm_scalar_selected_row][1][1];

																val = FLAM ^ STEPs;	// compare FLAM and STEPs    with   Bitwise XOR!
																}
								// No BLINKING
								else { val = STEPs; }

								// Update Hardware
								#if TM_VERSION == 5

											switch(mod) {	// Update Hardware
															case 0:	MIOS32_DOUT_SRSet(2,val);	break;
															case 1:	MIOS32_DOUT_SRSet(5,val);	break;
															case 2:	MIOS32_DOUT_SRSet(8,val);	break;
															case 3:	MIOS32_DOUT_SRSet(11,val);	break;
															default:MIOS32_DOUT_SRSet(14,val);	break;
															break;
															}
								#elif TM_VERSION == 4
											switch(mod) {	// Update Hardware
															case 0:	MIOS32_DOUT_SRSet(6,val);	break;
															case 1:	MIOS32_DOUT_SRSet(9,val);	break;
															case 2:	MIOS32_DOUT_SRSet(12,val);	break;
															case 3:	MIOS32_DOUT_SRSet(15,val);	break;
															default:MIOS32_DOUT_SRSet(18,val);	break;
															break;
															}
								#endif
								}
		return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! This function gets the DIN values of the selected row.						DIN  SHIFTREGISTER OFFSET HERE!!!!
//! call it from APP_SRIO_ServiceFinish() hook!
/////////////////////////////////////////////////////////////////////////////
s32 BLM_SCALAR_GetRow(void) {

		// decrememt debounce counter if > 0
		if( blm_scalar_button_debounce_ctr )	{ --blm_scalar_button_debounce_ctr; }

		// since the row line of the buttons is   identical to   the row line of the LEDs,
		// we can derive the button row offset from blm_scalar_selected_row
		int selected_row = (blm_scalar_selected_row-1) & 0x7; // &: bitwise AND     0x7: 7

		// check DINs
		int mod;
		for(mod=0; mod<5; ++mod) {	// 5: Module-Nr

								// Shift Register
								u8 sr;


/////////////////////// H A R D W A R E    Versions ////////////////////////////////////77
						#if TM_VERSION == 5
								switch(mod) {	// Chain BLM > DinX3
												case 0: sr = 0; break;
												case 1: sr = 1; break;
												case 2: sr = 2; break;
												case 3: sr = 3; break;
												default:sr = 4; break;
												break;
												}
						#elif TM_VERSION == 4
								switch(mod) {	// Chain BLM > DinX3
												case 0: sr = 3; break;
												case 1: sr = 4; break;
												case 2: sr = 5; break;
												case 3: sr = 6; break;
												default:sr = 7; break;
												break;
												}
						#endif

									// Deactivate normal DIN-Handler  (Encoders Buttons)
									MIOS32_DIN_SRChangedGetAndClear(sr, 0xff);

									// Get Value
									u8 sr_value = MIOS32_DIN_SRGet(sr);

									// ignore as long as debounce counter != 0
									if( !blm_scalar_button_debounce_ctr ) {

																			// determine pin changes
																			u8 changed = sr_value ^ blm_scalar_button_row_values[mod][selected_row];

																			if( changed ) {
																							// add them to existing notifications
																							blm_scalar_button_row_changed[mod][selected_row] |= changed;

																							// store new value
																							blm_scalar_button_row_values[mod][selected_row] = sr_value;

																							// reload debounce counter if any pin has changed
																							blm_scalar_button_debounce_ctr = 10;
																							}
																			}

						}
		return 0;
}


/////////////////////////////////////////////////////////////////////////////
//! This function should be called from a task to check for button changes
//! periodically. Events (change from 0->1 or from 1->0) will be notified
//! via the given callback function <notify_hook> with following parameters:
//!   <notifcation-hook>(s32 pin, s32 value)
/////////////////////////////////////////////////////////////////////////////
s32 BLM_SCALAR_ButtonHandler(void *_notify_hook)	{

		void (*notify_hook)(u32 pin, u32 value) = _notify_hook;

		// no hook?
		if( _notify_hook == NULL ) { return -2; }

		// check all shift registers for DIN pin changes
		int mod;
		for(mod=0; mod<5; ++mod) {	// 5: Module-Nr
									int row;
									for(row=0; row<8; ++row) {
																// check if there are pin changes - must be atomic!
																MIOS32_IRQ_Disable();
																u8 changed = blm_scalar_button_row_changed[mod][row];
																blm_scalar_button_row_changed[mod][row] = 0;
																MIOS32_IRQ_Enable();

																// any pin change at this SR?
																if( !changed )	{ continue; }

																// check all 8 pins of the SR
																int sr_pin;
																for(sr_pin=0; sr_pin<8; ++sr_pin)	{

																		if( changed & (1 << sr_pin) )	{notify_hook(mod*64 + 8*row + sr_pin, (blm_scalar_button_row_values[mod][row] & (1 << sr_pin)) ? 1 : 0); }
																}}
		}

	return 0;
}
