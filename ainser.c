#include <mios32.h>
#include "ainser.h"

/////////////////////////////////////////////////////////////////////////////
//! Initializes AINSER driver
//! Should be called from Init() during startup
/////////////////////////////////////////////////////////////////////////////
s32 AINSER_Init(u32 mode){

	// pins in push-poll mode (3.3V output voltage)
	MIOS32_SPI_IO_Init(2, MIOS32_SPI_PIN_DRIVER_STRONG);	// 2: AINSER_SPI J19      Push-poll:  better for Potentiometers
	// MIOS32_SPI_IO_Init(AINSER_SPI, MIOS32_SPI_PIN_DRIVER_STRONG_OD);	// pins in open drain mode (to pull-up the outputs to 5V)

	// SPI Port will be initialized in AINSER_Update()

	// ensure that CS is deactivated
	MIOS32_SPI_RC_PinSet(2, 0, 1); // Module 0....... 2:	AINSER_SPIspi, rc_pin J19, pin_value
	MIOS32_SPI_RC_PinSet(2, 1, 1); // Module 1....... 2:	AINSER_SPIspi, rc_pin J19, pin_value
	return 0;
}




/////////////////////////////////////////////////////////////////////////////
//! This function should be periodically called to scan AIN pin changes.
//!
//! A scan of a single multiplexer selection takes ca. 50 uS on a LPC1769 with MIOS32_SPI_PRESCALER_8
//!
//! Whenever a pin has changed, the given callback function will be called.\n
//! Example:
//! \code
//!   void AINSER_NotifyChanged(u32 pin, u16 value);
//! \endcode
//! \param[in] _callback pointer to callback function
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 AINSER_Handler(void (*_callback)(u32 module, u32 pin, u32 value)) {


	static int previous_ain_pin_value = 0;
	static int ain_pin_values[2][8] ={	{0,0,0,0, 0,0,0,0},
										{0,0,0,0, 0,0,0,0}	};

	static u8 mux_ctr = 0; // will be incremented on each update to select the next AIN pin
	static u8 first_scan_done = 0;


	// init SPI port for fast frequency access
	// we will do this here, so that other handlers (e.g. AOUT) could use SPI in different modes
	// Maxmimum allowed SCLK is 2 MHz according to datasheet
	// We select prescaler 64 @120 MHz (-> ca. 500 nS period)
	MIOS32_SPI_TransferModeInit(2, MIOS32_SPI_MODE_CLK0_PHASE0, MIOS32_SPI_PRESCALER_64);	//2: AINSER_SPI J19

	// determine next MUX selection
	int next_mux_ctr = (mux_ctr + 1) % 8;


	//	Initalize the enable mask:
	static u8 ainser_enable_mask = (1 << 0) | (1 << 1);

	// loop over connected modules
	int module;
	u32 module_mask = 1;
	for(module=0; module<2; ++module, module_mask <<= 1) {

													    if( !(ainser_enable_mask & module_mask) )
													      continue;

													    // loop over channels
													    int chn;
													    for(chn=0; chn<8; ++chn) {


																			      // CS=0
																			      if(module){ MIOS32_SPI_RC_PinSet(2, 1, 0); }// 2:	AINSER_SPIspi, rc_pin J19, pin_value
																			      else 		{ MIOS32_SPI_RC_PinSet(2, 0, 0); }// 2:	AINSER_SPIspi, rc_pin J19, pin_value

																			      // retrieve conversion values
																			      // shift in start bit + SGL + MSB of channel selection, shift out dummy byte
																			      MIOS32_SPI_TransferByte(2, 0x06 | (chn>>2));	// (2,...: AINSER_SPI
																			      // shift in remaining 2 bits of channel selection, shift out MSBs of conversion value
																			      u8 b1 = MIOS32_SPI_TransferByte(2, chn << 6);	// (2,...: AINSER_SPI
																			      // shift in mux_ctr , shift out LSBs of conversion value
																			      u8 b2 = MIOS32_SPI_TransferByte(2, (mux_ctr << 5));	// (2,...: AINSER_SPI


																			      // CS=1 (the rising edge will update the 74HC595)
																			      if(module){ MIOS32_SPI_RC_PinSet(2, 1, 1); }// 2:	AINSER_SPIspi, rc_pin J19, pin_value
																			      else 		{ MIOS32_SPI_RC_PinSet(2, 0, 1); }// 2:	AINSER_SPIspi, rc_pin J19, pin_value


																			      // store conversion value if difference to old value is outside the deadband
																			      u16 pin = 7-chn; // the mux/chn -> pin mapping is layout dependend
																			      u16 value = (b2 | (b1 << 8)) & 0xfff;

																			      previous_ain_pin_value = ain_pin_values[module][pin];
																			      int diff = value - previous_ain_pin_value;
																			      int abs_diff = (diff > 0 ) ? diff : -diff;

																			      if( !first_scan_done || abs_diff > MIOS32_AIN_DEADBAND ){
																																			ain_pin_values[module][pin] = value ;

																																			// notify callback function
																																			if( first_scan_done )	_callback(module, pin, value);
																																		    }
																			    }
													  }
  // select MUX input
  mux_ctr = next_mux_ctr;

  // one complete scan done?
  if( next_mux_ctr == 0 )	first_scan_done = 1;

  return 0; // no error
}
