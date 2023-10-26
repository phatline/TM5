/////////////////////	Header File for GLCD Fonts	/////////////////////////

#ifndef _GLCD_FONT_H
#define _GLCD_FONT_H

/////////////////////////////////////////////////////////////////////////////
// Defines array pointers to all available fonts
/////////////////////////////////////////////////////////////////////////////

extern const u8 GLCD_FONT_NORMAL[];
extern const u8 GLCD_FONT_NORMAL_INV[];	//	inverted in sense of Background
extern const u8 GLCD_FONT_BIG[];
extern const u8 fnt_BIG[]; 		// 	custom for CV1
extern const u8 fnt_SYM[]; 		// 	custom for CV1
extern const u8 fnt_SMALL[]; 	// 	custom for Tracker
extern const u8 fnt_TINY[]; 	// 	custom for Tracker
#endif /* _GLCD_FONT_H */
