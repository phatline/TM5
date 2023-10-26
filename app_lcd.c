//	LCD driver for 26x SSD1306
// 		MIOS32_LCD environment variable set   to    "universal"
//		reduced by Michael Sigl

#include <mios32.h>
#include <glcd_font.h>
#include <string.h>
#include "app_lcd.h"


static u8 prev_glcd_selection = 0xfe; // the previous mios32_lcd_device, 0xff: all CS were activated, 0xfe: will force the update


// pin initialisation >> Additonal CS-Line @ J10B
inline static s32 APP_LCD_ExtPort_Init(void) {

	int pin;
	for(pin=0; pin<8; ++pin) {	MIOS32_BOARD_J10_PinInit(pin + 8, MIOS32_BOARD_PIN_MODE_OUTPUT_PP); }

	return 0;
}

// set pin directly
inline static s32 APP_LCD_ExtPort_PinSet(u8 pin, u8 value) {

			return MIOS32_BOARD_J10_PinSet(pin + 8, value);
}



// serial data shift
inline static s32 APP_LCD_ExtPort_SerDataShift(u8 data, u8 lsb_first) {		// 202 einiges stretches vom orginal entfernt, bei fehlern die vom orginal wieder dazugeben!

  int i;
  if( lsb_first ) {
    for(i=0; i<8; ++i, data >>= 1) {
								      MIOS32_SYS_STM_PINSET(GPIOC, GPIO_Pin_13, data & 1); // J10B.D8 = ser
								      MIOS32_SYS_STM_PINSET_0(GPIOC, GPIO_Pin_14); // J10B.D9 = 0 (Clk)

								      MIOS32_SYS_STM_PINSET_1(GPIOC, GPIO_Pin_14); // J10B.D9 = 1 (Clk)
								    }
  } else {
    for(i=0; i<8; ++i, data <<= 1) {
								      MIOS32_SYS_STM_PINSET(GPIOC, GPIO_Pin_13, data & 0x80); // J10B.D8 = ser
								      MIOS32_SYS_STM_PINSET_0(GPIOC, GPIO_Pin_14); // J10B.D9 = 0 (Clk)

								      MIOS32_SYS_STM_PINSET_1(GPIOC, GPIO_Pin_14); // J10B.D9 = 1 (Clk)
								    }
  }
  return 0; // no error

}




// pulse the RC line after a serial data shift
inline static s32 APP_LCD_ExtPort_UpdateSRs(void) {

  APP_LCD_ExtPort_PinSet(2, 0); // J10B.D10
  APP_LCD_ExtPort_PinSet(2, 1); // J10B.D10
  return 0; // no error
}



/////////////////////////////////////////////////////////////////////////////
// Initializes the CS pins for GLCDs with serial port
// - 8 CS lines are available at J15
// - additional lines are available J10
/////////////////////////////////////////////////////////////////////////////
static s32 APP_LCD_SERGLCD_CS_Init(void)	{

    APP_LCD_ExtPort_Init();

  return 0; // no error
}





/////////////////////////////////////////////////////////////////////////////
// Sets the CS line of a serial GLCDs depending on mios32_lcd_device		202 optimized
// if "all" flag is set, commands are sent to all segments
/////////////////////////////////////////////////////////////////////////////
static s32 APP_LCD_SERGLCD_CS_Set(u8 value, u8 all){


    // Note: assume that CS lines are low-active!
	if( all ) {
				if( prev_glcd_selection != 0xff ) {
													prev_glcd_selection = 0xff;
													MIOS32_BOARD_J15_DataSet(value ? 0x00 : 0xff);

													// shift data
													int i;
													for(i=2; i>=0; --i) {	APP_LCD_ExtPort_SerDataShift(value ? 0x00 : 0xff, 1);	}

													// update serial shift registers
													APP_LCD_ExtPort_UpdateSRs();
													}
				}
	else{
		if( prev_glcd_selection != mios32_lcd_device ) {
													prev_glcd_selection = mios32_lcd_device;
													u32 mask = value ? ~(1 << mios32_lcd_device) : 0xffffffff;

													MIOS32_BOARD_J15_DataSet(mask);

													int selected_lcd = mios32_lcd_device - 8;
													int selected_lcd_sr = selected_lcd / 8;
													u8 selected_lcd_mask = value ? ~(1 << (selected_lcd % 8)) : 0xff;

													// shift data
													int i;
													for(i=2; i>=0; --i) {	u8 data = (i == selected_lcd_sr) ? selected_lcd_mask : 0xff;
																			APP_LCD_ExtPort_SerDataShift(data, 1);
																			}

													// update serial shift registers
													APP_LCD_ExtPort_UpdateSRs();
													}
		}


  return 0; // no error
}





/////////////////////////////////////////////////////////////////////////////
// Sets the E (enable) line depending on mios32_lcd_device	202 optimized
/////////////////////////////////////////////////////////////////////////////
static s32 APP_LCD_E_Set(u8 value) {

	    u8 selected_lcd = mios32_lcd_device - 2;
	    u8 selected_lcd_sr = selected_lcd / 8;
	    u8 selected_lcd_mask = (value != 0) * (1 << (selected_lcd % 8));
	    int i;

	    // shift data
	    for (i = 2; i >= 0; --i) {
	        APP_LCD_ExtPort_SerDataShift((i == selected_lcd_sr) ? selected_lcd_mask : 0, 1);
	    }

	    // update serial shift registers
	    APP_LCD_ExtPort_UpdateSRs();

	    return 0; // no error
}



/////////////////////////////////////////////////////////////////////////////
// Initializes application specific LCD driver	202 optimized
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_Init(u32 mode)	{

		MIOS32_BOARD_J15_PortInit(0);

		APP_LCD_SERGLCD_CS_Init();

		// wait 500 mS to ensure that the reset is released
		{
		int i;
		for(i=0; i<500; ++i)
		MIOS32_DELAY_Wait_uS(1000);
		}



		// initialize LCDs
		APP_LCD_Cmd(0xa8); // Set MUX Ratio
		APP_LCD_Cmd(0x3f);

		APP_LCD_Cmd(0xd3); // Set Display Offset
		APP_LCD_Cmd(0x00);

		APP_LCD_Cmd(0x40); // Set Display Start Line

		APP_LCD_Cmd(0xa1); // Set Segment re-map: rotated
		APP_LCD_Cmd(0xc8); // Set COM Output Scan Direction: rotated


		APP_LCD_Cmd(0xda); // Set COM Pins hardware configuration
		APP_LCD_Cmd(0x12);

		APP_LCD_Cmd(0x81); // Set Contrast Control
		APP_LCD_Cmd(0x7f); // middle

		APP_LCD_Cmd(0xa4); // Disable Entiere Display On

		APP_LCD_Cmd(0xa6); // Set Normal Display

		APP_LCD_Cmd(0xd5); // Set OSC Frequency
		APP_LCD_Cmd(0x80);

		APP_LCD_Cmd(0x8d); // Enable charge pump regulator
		APP_LCD_Cmd(0x14);

		APP_LCD_Cmd(0xaf); // Display On

		APP_LCD_Cmd(0x20); // Enable Page mode
		APP_LCD_Cmd(0x02);


		APP_LCD_ExtPort_Init();


  return 0;
}


/////////////////////////////////////////////////////////////////////////////
// Sends data byte to LCD	202	optimized
// IN: data byte in <data>
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_Data(u8 data) {

		// chip select and DC
		APP_LCD_SERGLCD_CS_Set(1, 0);

		MIOS32_BOARD_J15_RS_Set(1); // RS pin used to control DC

		// send data
		MIOS32_BOARD_J15_SerDataShift(data);


		// increment graphical cursor
		++mios32_lcd_x;

		// if end of display segment reached: set X position of all segments to 0
		if( (mios32_lcd_x % 128) == 0 ) {	APP_LCD_Cmd(0x00); // set X=0
											APP_LCD_Cmd(0x10);	}
		return 0; // no error
		}


/////////////////////////////////////////////////////////////////////////////
// Sends command byte to LCD
// IN: command byte in <cmd>
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_Cmd(u8 cmd)	{


		// select all LCDs
		APP_LCD_SERGLCD_CS_Set(1, 1);

		MIOS32_BOARD_J15_RS_Set(0); // RS pin used to control DC

		MIOS32_BOARD_J15_SerDataShift(cmd);
		return 0;
}


/////////////////////////////////////////////////////////////////////////////
// Clear ALL Screenś
s32 APP_LCD_Clear_ALL(void)	{

    u8 x, y;

    // use default font
    MIOS32_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);

    // send data
    for(y=0; y<8; ++y){
						MIOS32_LCD_CursorSet(0, y);

						// select all LCDs
						APP_LCD_SERGLCD_CS_Set(1, 1);

						MIOS32_BOARD_J15_RS_Set(1); // RS pin used to control DC

						for(x=0; x<128; ++x) MIOS32_BOARD_J15_SerDataShift(0x00);
						}
    return 0;
 }

/////////////////////////////////////////////////////////////////////////////
// Clear specific screen
s32 APP_LCD_Clear_NR(u8 nr){

    // use default font
    MIOS32_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);

    u8 x, y;

    // send data
    for(y=0; y<8; ++y){
						MIOS32_LCD_CursorSet(0, y);

						// select specific LCD
						APP_LCD_SERGLCD_CS_Set(nr, 0); // if x, 1 set then all --- if x, 0 then a specific display

						MIOS32_BOARD_J15_RS_Set(1); // RS pin used to control DC

						for(x=0; x<128; ++x) MIOS32_BOARD_J15_SerDataShift(0x00);
						}

	return 0;
 }

/////////////////////////////////////////////////////////////////////////////
// Clear Screen  non modified orginal
// IN: -
// OUT: returns < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_Clear(void)	{

    s32 error = 0;
    u8 x, y;

    // use default font
    MIOS32_LCD_FontInit((u8 *)GLCD_FONT_NORMAL);

    // send data
    for(y=0; y<mios32_lcd_parameters.height/8; ++y) {
      error |= MIOS32_LCD_CursorSet(0, y);

      // select all LCDs
      APP_LCD_SERGLCD_CS_Set(1, 1);

	MIOS32_BOARD_J15_RS_Set(1); // RS pin used to control DC

	for(x=0; x<mios32_lcd_parameters.width; ++x)
	  MIOS32_BOARD_J15_SerDataShift(0x00);

    }

    // set X=0, Y=0
    error |= MIOS32_LCD_CursorSet(0, 0);

    return error;
  }



/////////////////////////////////////////////////////////////////////////////
// Sets cursor to given position
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_CursorSet(u16 column, u16 line)	{	return APP_LCD_GCursorSet(mios32_lcd_x, mios32_lcd_y);	}


/////////////////////////////////////////////////////////////////////////////
// Sets graphical cursor to given position
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_GCursorSet(u16 x, u16 y)	{

    s32 error = 0;

    // set X position
    error |= APP_LCD_Cmd(0x00 | (x & 0xf));
    error |= APP_LCD_Cmd(0x10 | ((x>>4) & 0xf));

    // set Y position
    error |= APP_LCD_Cmd(0xb0 | ((y>>3) & 7));

    return error;
}


/////////////////////////////////////////////////////////////////////////////
// Initializes a single special character  // only for Character-LCDs
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_SpecialCharInit(u8 num, u8 table[8])	{	return -3; }// not supported


/////////////////////////////////////////////////////////////////////////////
// Sets the background colour
// Only relevant for colour GLCDs
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_BColourSet(u32 rgb) { return -3; }  // not supported


/////////////////////////////////////////////////////////////////////////////
// Sets the foreground colour
// Only relevant for colour GLCDs
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_FColourSet(u32 rgb)	{ return -3; } // not supported




/////////////////////////////////////////////////////////////////////////////
// Sets a pixel in the bitmap
// IN: bitmap, x/y position and colour value (value range depends on APP_LCD_COLOUR_DEPTH)
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_BitmapPixelSet(mios32_lcd_bitmap_t bitmap, u16 x, u16 y, u32 colour)	{
  if( x >= bitmap.width || y >= bitmap.height )
    return -1; // pixel is outside bitmap

  // all GLCDs support the same bitmap scrambling
  u8 *pixel = (u8 *)&bitmap.memory[bitmap.line_offset*(y / 8) + x];
  u8 mask = 1 << (y % 8);

  *pixel &= ~mask;
  if( colour )
    *pixel |= mask;

  return -3; // not supported
}



/////////////////////////////////////////////////////////////////////////////
// Transfers a Bitmap within given boundaries to the LCD
// IN: bitmap
/////////////////////////////////////////////////////////////////////////////
s32 APP_LCD_BitmapPrint(mios32_lcd_bitmap_t bitmap) {

  // abort if max. width reached
  if( mios32_lcd_x >= mios32_lcd_parameters.width )	{ return -2; }

  // all GLCDs support the same bitmap scrambling
  int line;
  int y_lines = (bitmap.height >> 3);

  u16 initial_x = mios32_lcd_x;
  u16 initial_y = mios32_lcd_y;
  for(line=0; line<y_lines; ++line) {

    // calculate pointer to bitmap line
    u8 *memory_ptr = bitmap.memory + line * bitmap.line_offset;

    // set graphical cursor after second line has reached
    if( line > 0 ) {
      mios32_lcd_x = initial_x;
      mios32_lcd_y += 8;
      APP_LCD_GCursorSet(mios32_lcd_x, mios32_lcd_y);
    }

    // transfer character
    int x;
    for(x=0; x<bitmap.width; ++x)
      APP_LCD_Data(*memory_ptr++);
  }

  // fix graphical cursor if more than one line has been print
  if( y_lines >= 1 ) {
    mios32_lcd_y = initial_y;
    APP_LCD_GCursorSet(mios32_lcd_x, mios32_lcd_y);
  }

  return 0; // no error
}
