#ifndef _APP_H
#define _APP_H

extern s32 SEQ_Init(u32 mode);
extern s32 SEQ_Reset(void);
extern s32 SEQ_Handler(void);

extern u8 Trigger_Matrix(u8 port, u8 note, u8 velocity, u16 step, u16 tic);
//    #define PRIORITY_Trigger_Matrix	      ( tskIDLE_PRIORITY + 3 ) // 4: high //303 is not a task!!! no effect?

extern u8 Trigger_REC(u16 step, u16 tic);
//    #define PRIORITY_Trigger_REC ( tskIDLE_PRIORITY + 3 ) // 4: high //303 is not a task!!! no effect?


extern void APP_Init(void);
extern void APP_Background(void);
extern void APP_Tick(void);
extern void APP_MIDI_NotifyPackage(mios32_midi_port_t port, mios32_midi_package_t midi_package);
extern void APP_SRIO_ServicePrepare(void);
extern void APP_SRIO_ServiceFinish(void);
extern void APP_DIN_NotifyToggle(u32 pin, u32 pin_value);
extern void APP_ENC_NotifyChange(u32 encoder, s32 incrementer);
extern void APP_AIN_NotifyChange(u32 pin, u32 pin_value);
extern void Router(u16 ID, u16 H, u16 V, s16 value);
extern void Song(u16 stp, s16 value);




extern void BLM_LED(u32 element_id, u32 v, u32 h, u32 color, u32 blink);

	s8 MatrixUse;   // SQ-Steps, SQ-Swing, TM-Router, TM-CC,

	u16 Last[4]; // calculated Last Step
	u16 First[4]; // calculated First Step
	u8	Sync_LED;
	u16 SONG_STEP;
	u8	SONG_SYNC_Loop;
	u8  Bar1;	// 0 1 2 3   0 1 2 3
	u8  Bar4;	// 0 1 2 3 4 5 6 7 ...16
	u8  Bar8;
	u8  Bar16;
	u16 Bar32;
	u16 Bar64;
	u16 Ticker;
	u8  SONG_Running_LED;
	u16	SEQ_Running_LED;
	u16 Letzter_Anfangs_Schritt;
	u16 Bar_Schritt_Zahl;
	u8  Anzeige[18];	// OLED - Display - Update
	s8  Menue; 	// LCD Menue Page = depending of MtxNr
	s16 SongProgress;		// show in a LED-Bar where in the Song we approximatly are

	u8 Bar_Ende_erreicht;

	s16 NavJump;

	u8 REC_Seq;	// 0: Not in Recmode, 1: Overdub, 2: Clear and Replace
    static u8 const getan = 0;
    static u8 const erledigt = 0;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Patch Variables >>> Saved on SDCard
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

 typedef struct storage {   // to optimize SD-Load-Time, each clips data is stored in this struct

        char	name[4][8];
        char	label[16][16];	// 16 tracks, 16 Chars (for 2 lcd lines)
        char	symb[16][2][2];		// Track Symbols
        char	launch_cc_name[16][8];
        u8		vari;
        float   bpm;
        u8      rythm;          // 4x4=16, 5x3=15 usw... to calculate maximal duration per UI-matrix page
        u8      PAGE_Steps;     // calculate from 0 on....so 0-15 are 16 Steps that are visible on the BLM with a 4/4Beat
        u8      PAGE_loop[4][2];   // >Loop Length - END
        s8      PAGE_actual;    // Step PAGE, in 4/4 Mode one PAGE has 16 Step
        u8		FLAM_act;		// Activate the Flame Feature
        u8		FLAM_val;		// Ammount of Flame
        s8		FLAM_vel;		// FLAM Velocity spreading - negative Values reduce Velocity of the first hit, positive reduce the Velocity of the second hit +- 24

        u8      MainLoop;       // max Length of the mainloop, if you always play shorter then 256 then you should set this to shorter values!!! (sync...)
        u8      direction;      // > 303 PLACE-HOLDER - <      0: <, 1>, 2<>   SEQUENCER STEP-COUNTER-DIRECTION
        u8      direct_start;   // > 303 PLACE-HOLDER - <      0: in beat.direction-mode:2 > start on start, 1: -//- > start on end

        s8      SWING_32th;
        s8      SWING_64th;
        u8      SWING_Switch;

        //      These are new values------------need for update script
        u8		BPM_Slide;		// Activate the BPM_slide for this Patch...
        u16		BPM_Slide_Value;// Time for BPM_Slide when loading a new beat variation
        float	BPM[4];

} store_t;
  store_t beat;     // Data for  beat.clips
  store_t beat_cpy;


 typedef struct songmode {   // to optimize SD-Load-Time, each clips data is stored in this struct

        char		name[16][8][8];
        char		label[16][8];

        u8			DECAY_erweitern;	// new!  init 0   Erweitere den Decay
        u16			Ende;				// in Bars

        unsigned	AutoRepeat:		1;	// new!  Init 0
        unsigned    RUN:			1;	// also this means its AUTOSTART


        //404 update FILE:  load name and label, then init the new Variables...

} song_t;
  song_t song;     // Data for  song informations


  typedef struct system {	// SYSTEM SETTINGS  -  Saved on SD-Card

						char name[8];
						char Folder;			// where to save/load the patches
						s16	TrigChn[16];
						u8 OUTPUT_Clock[4]; 	// should the sequencer Put out Clocks on the DIN-Ports  //1: thru, 0: blocked
						u8 Port_OUT_Clock;
						u8 Port_IN_Clock;
						u8 Port_OUT_Poly;
						u8 Port_OUT_Mono;
						u8 Port_OUT_Trig;
						u8 Port_LoopA;
						u8 Port_CC;
						u8 Port_IN_Poly;
						u8 Port_IN_PAD;

						u8 CH_LoopA;				// Remote off the LoopA  (Melody Variation aka Scene, + Song switch...) PORT-LOOPA
						u8 CH_CC_Melo;				// Melody Variation Output (launch and perofamnce page) PORT-CC
						u8 CH_CC_XY;				// Launchpage - XY Grid  PORT-CC
						u8 CH_CC_Drum;				// Performance-Page - automated Switching PORT-CC
						u8 CH_CC_DrumLaunch;		// Launch-Page - automated Switching  -   not so sure at the moment - have to dive into the code PORT-CC


						u8 Screensaver_time_sec;
						u8 TrigNote[16];
						s8 BeatRealtimeLoadVAR;				// Load the Beat-Notes   NOW   and indipendent from the Loopsection		For   This Song only
						u8 DecayExBtns;						// the left one of 2 buttons over the Decay-Fader --- to expand the Decay time to extreme long!
						u8 SONG_Sync;

						u8 TAP_CC_Start;
						u8 TAP_CC_Stop;
						u8 TAP_CC_Tic;
						u8 TAP_Port;
						u8 TAP_CH;
						s16 BeatVarTime;

						// Random Sequence
						u8 SEQrandom_Mode;		// init 1
						u8 SEQrandom_Page;  	// init 0
						u8 SEQrandom_Amout;		// 64
						u8 SEQrandom_TrackA; 	// init 0
						u8 SEQrandom_TrackB; 	// init 15
						u8 SEQrandom_Velo;		// init 100
						u8 SEQrandom_Grid;		// init 1

						// Random Router-Matrices
						u8 ROUTrandom_Mode;		// init 1
						u8 ROUTrandom_Act;		// init 2
						u8 ROUTrandom_Page;  	// init 0
						u8 ROUTrandom_Amout;	// 64
						u8 ROUTrandom_TrackA; 	// init 0
						u8 ROUTrandom_TrackB; 	// init 15
						u8 ROUTrandom_Grid;		// init 0

						u8 SEQrandom_Act;

						// Random Sequence Rules
						u8 SEQranThresh1;
						u8 SEQranThresh2;
						u8 SEQranRule_ExTrk;
						u8 SEQranChoke1_stay;
						u8 SEQranChoke1_clear;
						u8 SEQranChoke2_stay;
						u8 SEQranChoke2_clear;

						u8 TrigNoteIN[16];
						u8 SEQrandom_ModeVEL;

						// Recording Sequence
						s8 REC_TIC_Offset;
						u8 REC_Quant; //32th, 16th, 8th
						u8 CC_REC;
						u8 CH_PAD;
						u8 CC_NTErpd4off;
						u8 CC_NTErpd4;
						u8 CC_NTErpd8;
						u8 CC_NTErpd16;
						u8 CC_RecDelete;

						// J5 Trigger Scan
						u8 ZeitSchlagMaximum[16];
						u8 ZeitAmpMaximum[8];
						s16 Schwellwert[8];

						// Send CCs
						s8 Port_OUT_CC;
						s8 CH_CC_FaderButton;

						u8 BPM_Auto_Slide;		// dont make unsigned here!
						u8 StepView;			// dont make unsigned here!					// 0: 32th Step View,        1: 16th Step View
						u8 Port_THRU_Clock1: 1;	// dont make unsigned here!
						u8 Port_THRU_Clock2: 1;	// dont make unsigned here!
						u8 Port_THRU_Clock3: 1;	// dont make unsigned here!
						u8 Port_THRU_Clock4: 1;	// dont make unsigned here!

						// save some RAM... with One-Bit-Values
						unsigned NTErpd_Mode: 1;		// 0: NoteOnOFF, 1: PolyPressure	Recording-Sequence
						unsigned LIVE_PAGE_CYCLE: 1;
						unsigned LIVE_SAVE: 1;
						unsigned LIVE_CpyPaste: 1;
						unsigned LIVE_MODE: 1;
						unsigned LIVE_SONG_CLEAR: 1;
						unsigned CC_Mode: 1;
						unsigned CC_Tx_Beat: 1;
						unsigned CC_Tx_Bank: 1;
						unsigned BeatRealtimeLoadBetweenSongs: 1;	//Load the Beat-Notes   NOW   and indipendent from the Loopsection		For   Other Songs
						unsigned BPM_Auto_Slide_PC: 1;
						unsigned RouterCycle: 1;					// if 0: Normal behavior with shift functions...1: toggle thru modes
						unsigned LIVE_PAGE_CYCLE_Include0: 1;
						unsigned LIVE_PAGE_CYCLE_Include1: 1;
						unsigned LIVE_PAGE_CYCLE_Include2: 1;
						unsigned LIVE_PAGE_CYCLE_Include3: 1;
						unsigned NORMAL_PAGE_CYCLE: 1;				// if 0 pagecycle is also active in Non-Live-Mode, 1: shift will return you to Performcepage 3


} sys_t;
  sys_t sys;   // Data for  Systemsettings




 // Struct for different flags
 typedef struct Aufgaben {     // to optimize Memory usage and data, i fill all 1bit flags into a struct

	u8 SendShift;

	u8 SDmessage;					// some SD-Load Messages in Background tasks...
	u8 MtrNrStored4Sync;
	u8 Overdub[16];
	u8 BeatNrStored4Sync;

	unsigned PCLoad : 1;     	// init 0   // flag.PCLoad      // ProgramChange via Midi received!
	unsigned shift: 1;       	// init 0   // flag.shift       // calculate time between press and release, on order to do different things
	unsigned tm_blank_calc: 1;	// init 1 // calculate that a routing preset is filled with information or not.
	unsigned stop: 			1;		// will stop the seq on next cycle
	unsigned play: 			1;  	// will stop the seq on next cycle
	unsigned calc_decay:	1;		// calculate Decay when Fader has moved...
	unsigned upd_menue:		1;		// if a Encoder has moved - and we want to update
	unsigned roll_led_poly:	1;		// if a Encoder has moved - and we want to update
	unsigned roll_led_mono:	1;		// if a Encoder has moved - and we want to update
	unsigned roll_led_trig:	1;		// if a Encoder has moved - and we want to update
	unsigned energy_save:	1;		// after a while of no UI-Action send LCD and Matrix into sleep....
	unsigned energy_reset:	1;		// to reset the counter
	unsigned energy_lcd:	1;
	unsigned Im_SYS_Menue:	1;
	unsigned get_page_names:1;
	unsigned TM_was_loadet:	1;		// was the TM loadet in last time?
	unsigned BEAT_LOAD:		1;		// Need To load
	unsigned BEAT_TRANSFAIR:1;		// Do it now ---Transfair the new beat into sequencer
	unsigned SongClear:		1;
	unsigned Setze_Song_Ende:	1;		// init 0 // calculate that a routing preset is filled with information or not.
	unsigned SongRestart:	1;
	unsigned SongNavi:		1;
	unsigned CCMtx_Load:	1;		// init 0 // calculate that a routing preset is filled with information or not.
	unsigned TM_Load:		1;		// init 0  Load the Actual Trigger-Routing into the load matrix
	unsigned MATRIX_Copy:	1;
	unsigned MATRIX_Paste:	1;
	unsigned MATRIX_Clear:	1;
	unsigned SyncedSwitch_Taster:	1;
	unsigned SyncedBeat_Taster:		1;
	unsigned MatrixUse_Switch:		1;
	unsigned Sync_LED_neu:			1;
	unsigned TM_Route_View:		1;
	unsigned Berechne_Navi:			1;
	unsigned VELO_KILL_calc:		1;
	unsigned Roll_Variation_calc:	1;
	unsigned Anzeige_UPD:			1;
	unsigned upd_BLM:				1;
	unsigned upd_EXT:				1;
	unsigned LIVE_Toggle:			1;
	unsigned copy:			1;
	unsigned paste:			1;
	unsigned clear:			1;
	unsigned swap_names:	1;
	unsigned swap_name2:	1;
	unsigned swap_name3:	1;
	unsigned get_swap_name1:	1;
	unsigned get_swap_name2:	1;
	unsigned PC_Slide: 			1;
	unsigned BPMLOAD:			1;
	unsigned song_end_indicate: 1;
	unsigned Delayed_LCD_UPD: 1;
	unsigned TM_Random: 1;
	unsigned SEQ_Random: 1;
	unsigned BLMkeyboard: 1;
	unsigned BLMshift: 1;
	unsigned EncSet: 1;
}Auftrag_t;

 Auftrag_t Auftrag;





 // Struct for different flags
 typedef struct statuses {     // to optimize Memory usage and data, i fill all 1bit flags into a struct

	unsigned Shift_Taste: 	1;  	// init 0   // flag.shiftvalue  // pressed or not pressed = value
	unsigned SongAnsicht:	1;
	unsigned SongRecFirst:	1;
	unsigned Lauft:			1;
	unsigned LAUNCH_Taste:	1;

	u8 SongRec;

}status_t;

 status_t Status;
#endif /* _APP_H */
