#include <mios32.h>
#include "seq_bpm.h"
#include "app.h"

static s32 SEQ_Tick(u32 bpm_tick);

static s16 loop_step_pos = -1; //  straight step position bevore swing

static s16 StepSend = 0; // Memorized StepNr

static u8 seq_pause = 0;



s32 SEQ_Init(u32 mode){
					    SEQ_Reset();  		//  reset sequencer
					    SEQ_BPM_Init(0);  	//  init BPM generator
					    SEQ_BPM_PPQN_Set(384);
					    SEQ_BPM_Set(beat.bpm);
					    SEQ_BPM_TickSet(0);
					    SONG_STEP = 0;
						SONG_SYNC_Loop = 0;

						return 0; //  no error
}

s32 SEQ_Handler(void){ //  this sequencer handler is called periodically to check for new requests from BPM generator
  //  handle requests

  u8 num_loops = 0;
  u8 again = 0;
  do {
    ++num_loops;

    //  note: don't remove any request check - clocks won't be propagated
    //  so long any Stop/Cont/Start/SongPos event hasn't been flagged to the sequencer
    if( SEQ_BPM_ChkReqStop() ) {
     //  SEQ_PlayOffEvents();
    }

    if( SEQ_BPM_ChkReqCont() ) {
      //  release pause mode
      seq_pause = 0;
    }

    if( SEQ_BPM_ChkReqStart() ) {
      SEQ_Reset();
    }

    u16 new_song_pos;
    if( SEQ_BPM_ChkReqSongPos(&new_song_pos) ) {
   //    SEQ_PlayOffEvents();
    }

    u32 bpm_tick;
    if( SEQ_BPM_ChkReqClk(&bpm_tick) > 0 ) {
      again = 1; //  check all requests again after execution of this part

      SEQ_Tick(bpm_tick);
    }
  } while( again && num_loops < 10 );

  return 0; //  no error
}

s32 SEQ_Reset(void){
  seq_pause = 0;   		//  release pause mode
  SEQ_BPM_TickSet(0);   //  reset BPM tick
  return 0; //  no error
}

s32 SEQ_Loop(void){return 0;}


static s32 SEQ_Tick(u32 bpm_tick){  //  performs a single bpm tick


	static u8 master = 0;
	static s16 ppqn_got = 0;
	static u8 TRANSFAIR = 0;

	// Syncronisied Loop Points
	static u16 act_FIRST = 0;
	static u16 act_LAST  = 31;



  // MAIN RESET ON FIRST TIC EVER... (PLAY-Button)
  if( bpm_tick == 0 ) {
						SEQ_BPM_CheckAutoMaster();

						// Midi-Clock-MASTER: 		send Midi Start
						if( SEQ_BPM_IsMaster() ) { 	if(sys.OUTPUT_Clock[0]) {  MIOS32_MIDI_SendStart(sys.Port_OUT_Poly);	}
													if(sys.OUTPUT_Clock[1]) {  MIOS32_MIDI_SendStart(sys.Port_OUT_Mono);	}
													if(sys.OUTPUT_Clock[2]) {  MIOS32_MIDI_SendStart(sys.Port_OUT_Trig);	}
													if(sys.OUTPUT_Clock[3]  &&  sys.Port_LoopA != 15) {  MIOS32_MIDI_SendStart(sys.Port_LoopA);	}

													// TAP Start
													if(sys.TAP_Port != 15) {  MIOS32_MIDI_SendCC(sys.TAP_Port, sys.TAP_CH,	sys.TAP_CC_Start, 127); }

													master= 1;}
						// Midi-Clock-SLAVE:
						else { master = 0;}

						ppqn_got = SEQ_BPM_PPQN_Get();


						// Berechne Loop - Start
						First[beat.vari] = (beat.PAGE_loop[beat.vari][0] * beat.PAGE_Steps);	// First[beat.vari] step in loop =  0beat.PAGE_actual*16Perpage=0...
						act_FIRST = First[beat.vari];	// synced ANFANG

						Last[beat.vari]  = (beat.PAGE_loop[beat.vari][1] * beat.PAGE_Steps) + beat.PAGE_Steps;  // Last[beat.vari]  step in loop = (0beat.PAGE_actual*16Perpage)+16=16
						act_LAST = Last[beat.vari];		// synced ENDE

						// Berechne neue Loop-Position
						loop_step_pos = act_FIRST - 1;

			            SONG_STEP = 0;
						SONG_SYNC_Loop = 0;
			            }
	// TIC GENERATION
	static u16 TIC4 = 0;
	static u8 TIC16 = 0;
	static u8 TIC32 = 0;
	static u8 TICclock = 0;
	static int TIC384 = 0;

	TIC4	 = (bpm_tick % (ppqn_got));	// for Song-Sequencer

	TIC16	 = (bpm_tick % (ppqn_got/4));	// for Song-Sequencer

	TIC32 	 = (bpm_tick % (ppqn_got/8));	// for Stepsequencer

	TICclock = (bpm_tick % (ppqn_got/24));	// for midiclock-Tic - output

	TIC384 =   (bpm_tick % (ppqn_got/256));	// for midiclock-Tic - output

	Ticker = TIC32;	// to use for example in random generators


	if(TICclock == 0) { if(sys.OUTPUT_Clock[0]) {  MIOS32_MIDI_SendClock(sys.Port_OUT_Poly);	}	// 305 Ticsend was 1!   maybe there happens to much on "0"   and "1" less > so less change to loose a tic!
						if(sys.OUTPUT_Clock[1]) {  MIOS32_MIDI_SendClock(sys.Port_OUT_Mono);	}
						if(sys.OUTPUT_Clock[2]) {  MIOS32_MIDI_SendClock(sys.Port_OUT_Trig);	}
						if(sys.OUTPUT_Clock[3]  &&  sys.Port_LoopA != 15) {  MIOS32_MIDI_SendClock(sys.Port_LoopA);	}
						}

	// TAP 24th Tic
	if(TIC4 == 0) {	if(sys.TAP_Port != 15) {  MIOS32_MIDI_SendCC(sys.TAP_Port, sys.TAP_CH,	sys.TAP_CC_Tic, 127); } }

	//MIOS32_MIDI_SendDebugMessage("TIC4: %d    TIC 16: %d", TIC4, TIC16);

	// Auto BPM-Slide		// Auto Slide BPM on Beat-Variation Change   - Globaly ON
	if(TIC384 == 0 && SEQ_BPM_IsMaster() ) {


											// count
											static int slCount = 0;
											slCount++;

											if(slCount > beat.BPM_Slide_Value){	slCount = 0;	// reset Counter

																				u16 beatss = SEQ_BPM_Get();	// make an integer --- becasue BPM is Float


																				// if Programchange Slide    or   (GlobalAutoSlide  AND   Slide on off the next beat)
																				if( Auftrag.PC_Slide || (sys.BPM_Auto_Slide && beat.BPM_Slide) ) {


																										// Actual BPM is lower then it should be	ENCREASE BPM!
																										if(beatss < beat.BPM[beat.vari]) { 	beatss++;
																																	if(beatss > 999)	{ beatss = 999; }	// clip the Value
																																	SEQ_BPM_Set(beatss);
																																	if(Menue == 4 || Menue == 5) { Anzeige[0] = 1; }	// update LCD if on BPM-Screen
																																	}

																										if(beatss > beat.BPM[beat.vari]) { 	beatss--;
																																	if(beatss <= 0)		{ beatss = 1; }	// Clip the Values because 0 is not possible
																																	SEQ_BPM_Set(beatss);
																																	if(Menue == 4 || Menue == 5) { Anzeige[0] = 1; }	// update LCD if on BPM-Screen
																																	}

																										if(beatss == beat.BPM[beat.vari])	{ Auftrag.PC_Slide = 0;	}// Kill Flag
																										}
																			}
											}



	//  Running LED   (done in BACKGROUND.TASK)
	if( TIC32 == 12 )	{ SEQ_Running_LED = StepSend; }


	/// ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Song SEQUENCER    jede 16th Note //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			// jede 16th NOTE
			if(TIC16 == 0) {	// Auftrag.Sync_Step_Done  benutzen um das ganze in einen backgroundtast zu verbaren... der step soll einen change zeigen.

								// Schritt Zahl
								u8  Bar1_Schrittzahl  = beat.rythm;
								u8  Bar4_Schrittzahl  = beat.rythm * 4;
								u8  Bar8_Schrittzahl  = beat.rythm * 8;
								u8  Bar16_Schrittzahl = beat.rythm * 16;
								u16 Bar32_Schrittzahl = beat.rythm * 32;
								u16 Bar64_Schrittzahl = beat.rythm * 64;


								// Welchen Schritt im Bar haben wir gerade
								Bar1  = SONG_STEP % Bar1_Schrittzahl;	// 0 1 2 3   0 1 2 3
								Bar4  = SONG_STEP % Bar4_Schrittzahl;	// 16 Schritte	// als Beispiel ein 4/4 Takt
								Bar8  = SONG_STEP % Bar8_Schrittzahl;	// 32 Schritte
								Bar16 = SONG_STEP % Bar16_Schrittzahl;	// 64 Schritte
								Bar32 = SONG_STEP % Bar32_Schrittzahl;	// 128 Schritte
								Bar64 = SONG_STEP % Bar64_Schrittzahl;	// 256 Schritte



								// Bei jedem Schritt, erstelle ein BlinkMuster ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

								Auftrag.Sync_LED_neu = 1;	// Informiere das die LED neu aktiviert werden muss

								// Finde heraus:  Ist der Erste Schritt im Sync-Loop erreicht?
								u8 Bar_Anfang_erreicht = 0;
								Bar_Ende_erreicht = 0;



								// Sync
								switch ( sys.SONG_Sync) {	// 1 Bar
															case 0:	if(Bar1 == 0)	{	Sync_LED = 3;							// Am Schritt jedes BARS >> Schalte LED auf CYAN
																						Bar_Anfang_erreicht = 1;
																						Bar_Schritt_Zahl = Bar1_Schrittzahl;	// aktuelle Schritt Zahl
																						break; }

																	else	 		{	Sync_LED = 0;	}	// Sonstige Schritte	 >> Schalte -aus-

																	// Bar Ende berechnen
																	if( (Bar1+1) >= Bar1_Schrittzahl ) {	Bar_Ende_erreicht = 1; }

																	break;


															// 4 Bar
															case 1: if(Bar1 == 0)										{	Sync_LED = 1; 	}									// Jeder 4 schritte    (bei 4/4 Takt)

																	if(Bar4 == 0)										{	Sync_LED = 3;										// Jeden 16 Schritt
																															Bar_Anfang_erreicht = 1;
																															Bar_Schritt_Zahl = Bar4_Schrittzahl;				// 16 Schritte
																															}
																	if(Bar4 != 0  &&  Bar1 != 0)						{	Sync_LED = 0;	}

																	// Bar Ende berechnen
																	if( (Bar4+1) >= Bar4_Schrittzahl ) {	Bar_Ende_erreicht = 1; }

																	break;


															// 8 Bar					// als VORLAGE
															case 2: if(Bar1 == 0)										{	Sync_LED = 1;	}
																	if(Bar8 == 0)										{	Sync_LED = 3;										// Jeden 32 Schritt
																															Bar_Anfang_erreicht = 1;
																															Bar_Schritt_Zahl = Bar8_Schrittzahl;
																															}
																	if(Bar4 == 0  &&  Bar8 !=0)							{	Sync_LED = 2;	}	// Jeden 16ten Schritt   --- jedoch nicht wenn er auf 32 fällt...
																	if(Bar4 != 0  &&  Bar8 != 0  &&  Bar1 != 0)			{	Sync_LED = 0;	}	// Sonstige Schritte	 >> Schalte -aus-

																	// Bar Ende berechnen
																	if( (Bar8+1) >= Bar8_Schrittzahl ) {	Bar_Ende_erreicht = 1; }

																	SONG_Running_LED = (Bar8/2)%beat.PAGE_Steps;

																	break;


															// 16 Bar
															case 3:	if(Bar1 == 0)												{	Sync_LED = 1;	}
																	if(Bar16== 0)												{	Sync_LED = 3;								// Jeden 64 Schritt
																																	Bar_Anfang_erreicht = 1;
																																	Bar_Schritt_Zahl = Bar16_Schrittzahl;
																																	}
																	if( (Bar4==0  &&  Bar8==0) &&  Bar16!=0)					{	Sync_LED = 2;	}
																	if(Bar4 != 0  &&  Bar8!=0  &&  Bar1!= 0  && Bar16!=0)		{	Sync_LED = 0;	}

																	// Bar Ende berechnen
																	if( (Bar16+1)  >= Bar16_Schrittzahl ) {	Bar_Ende_erreicht = 1; }

																	SONG_Running_LED = (Bar16/4)%beat.PAGE_Steps;

																	break;


															// 32 Bar
															case 4:	if(Bar1 == 0)														{	Sync_LED = 1;	}
																	if(Bar32== 0)														{	Sync_LED = 3;						// Jeden 128 Schritt
																																			Bar_Anfang_erreicht = 1;
																																			Bar_Schritt_Zahl = Bar32_Schrittzahl;
																																			}
																	if( (Bar4==0  &&  Bar8==0 ) && Bar32 != 0)							{	Sync_LED = 2;	}
																	if(Bar4 != 0  &&  Bar8!=0 && Bar1 !=0  && Bar16 != 0 && Bar32 !=0)	{	Sync_LED = 0;	}

																	// Bar Ende berechnen
																	if( (Bar32+1) >= Bar32_Schrittzahl ) {	Bar_Ende_erreicht = 1; }

																	SONG_Running_LED = (Bar32/8)%beat.PAGE_Steps;

																	break;


															// 64 Bar
															case 5:	if(Bar1 == 0)																{	Sync_LED = 1;	}
																	if(Bar64== 0)																{	Sync_LED = 3;				// Jeden 256 Schritt
																																					Bar_Anfang_erreicht = 1;
																																					Bar_Schritt_Zahl = Bar64_Schrittzahl;
																																					}
																	if( (Bar4==0  &&  Bar8==0 ) && Bar64!=0)									{	Sync_LED = 2;	}
																	if(Bar4 != 0  &&  Bar8!=0 && Bar1!= 0 && Bar16!=0  && Bar32!=0 && Bar64!=0)	{	Sync_LED = 0;	}

																	// Bar Ende berechnen
																	if( (Bar64+1) >= Bar64_Schrittzahl ) {	Bar_Ende_erreicht = 1; }

																	SONG_Running_LED = (Bar64/16)%beat.PAGE_Steps;

																	break;
															break;
															}

								// Merke 0.Schritt
								if( Bar_Anfang_erreicht )	{ Letzter_Anfangs_Schritt = SONG_STEP; }


								// MIOS32_MIDI_SendDebugMessage("Step: %d   1: %d:   4: %d   8: %d   16:  %d   32: %d   64: %d Song_Running_LED %d", Bar1,Bar4,Bar8,Bar16,Bar32,Bar64, SONG_Running_LED);

								// MIOS32_MIDI_SendDebugMessage("B1-Step: %d   B1-Schritt: %d:   Anfangs-Schritt: %d     Anfang-Er: %d  Ende-err: %d", Bar1, Bar1_Schrittzahl, Letzter_Anfangs_Schritt, Bar_Anfang_erreicht, Bar_Ende_erreicht);
								// MIOS32_MIDI_SendDebugMessage("B8-Step: %d   B8-Schritt: %d:   Anfangs-Schritt: %d     Anfang-Er: %d  Ende-err: %d", Bar8, Bar8_Schrittzahl, Letzter_Anfangs_Schritt, Bar_Anfang_erreicht, Bar_Ende_erreicht);


								// Song Restart
								if( Auftrag.SongRestart ){
															// in Sync
															if( SONG_SYNC_Loop == 0 )	{	Auftrag.SongRestart = 0;

																							SONG_STEP = 0;

																							Auftrag.upd_BLM = 1;
																							}
															}


								/// Send Actual Song Step ///////////////////////////////////////////////////////////////////// z.B. 511

								Song(SONG_STEP, 1);			// ...dort passieren die Automatisierunge, die Aufnahem usw...

								/// ///////////////////////////////////////////////////////////////////////////////////////////


								/// Neuer Song-Ende	///////////	z.B. 511

								if( Auftrag.Setze_Song_Ende ) {
																//  Nur in Sync
																if( Bar_Ende_erreicht )	 {	if( Status.Shift_Taste ) {	song.Ende = 3071;  				// Maximale Schrittlänge
																														Auftrag.Delayed_LCD_UPD = 1;	// Alle Anzeige updaten

																														Auftrag.Setze_Song_Ende = getan; }

																							else 					{ 	song.Ende = SONG_STEP;			// Ende wird aktueller Schritt
																														Auftrag.Delayed_LCD_UPD = 1;	// Alle Anzeige updaten
																														Auftrag.Setze_Song_Ende = getan;

																														// MIOS32_MIDI_SendDebugMessage("Auftrag setze Song-Ende > erledigt    EndSTP: %d    LastStartSTP: %d    Start-Er: %d  END-err: %d", song.Ende, Letzter_Anfangs_Schritt, Bar_Anfang_erreicht, Bar_Ende_erreicht);
																														}
													}							}




								/// Schreite fort ////////// z.B. 512
								SONG_STEP++;




								// Berechne Songübersichts bar  - Die Bar hat 8 Schritte
								// SongProgress    = (song.Ende - SONG_STEP) % (song.Ende/8);
								s16 songending = song.Ende - SONG_STEP;
								if(songending <= 0)	{ songending = 0; }

								SongProgress    = (  songending  / (song.Ende/8));
								if(SongProgress <= 0)	{ SongProgress = 0; }	// clip

								// Are we on the last 16 steps?
								if(songending <= 16)	{ Auftrag.song_end_indicate = 1; }	// then blink
								else { Auftrag.song_end_indicate = 0; }	// not blink


								// MIOS32_MIDI_SendDebugMessage("songende %d    -    songStep %d   =  %d     calc process %d", song.Ende, SONG_STEP, (song.Ende-SONG_STEP), SongProgress);



								/// Springe zum Anfang zurück
								//  > Max Schritt Zahl  |  ( Schritt = Song Ende	    &  NICHT im Song-END-Setzen)
								// z.B.                       510     >=  511+1 (512)				// Normaler Loop
								// z.B.                       512	  >=  512						// Setze neues Song Ende....

								if( SONG_STEP >= 3072  ||  (SONG_STEP >= (song.Ende+1)  &&  !Auftrag.Setze_Song_Ende) )	{
																															SONG_STEP = 0;

																															// Turn off Song Modus
																															if( !song.AutoRepeat ) {	song.RUN = 0;				// End Song Modus
																																						Status.SongRec = 0;		// End Record Modus
																																						Auftrag.upd_BLM = 1;
																																						}

																															else  {	if( Status.SongRec) {	Status.SongRec = 1;	// Go into Overdub- Mode
																																							Auftrag.upd_BLM = 1;
																																							}
																															}
																												}



								// Loop a Section
								if( Auftrag.SongNavi )	{
															//NAV Jump
															if(NavJump != 0){	if( Bar_Ende_erreicht )	 {	// Calculate Jump-Step
																											static s16 Jump = 0;

																											// jump in Song-Sync-Loop - Times
																											switch ( sys.SONG_Sync) {	case 0: Jump = beat.rythm * NavJump; 			break;
																																		case 1: Jump = Bar4_Schrittzahl  *	NavJump; 	break;
																																		case 2: Jump = Bar8_Schrittzahl  *	NavJump; 	break;
																																		case 3: Jump = Bar16_Schrittzahl *	NavJump; 	break;
																																		case 4: Jump = Bar32_Schrittzahl *	NavJump; 	break;
																																		default:Jump = Bar64_Schrittzahl *	NavJump; 	break;
																																		break;
																																		}

																											SONG_STEP = Letzter_Anfangs_Schritt + Jump;	// Jump back or forward

																											if(SONG_STEP <= 0)		{ SONG_STEP = 0;}	// dont allow negative values
																											if(SONG_STEP >= 3071)	{ SONG_STEP = Letzter_Anfangs_Schritt;}


																											NavJump = 0;	// Kill Flag
																											Anzeige[0] = 1;
																											}
																				}


															// Springe zum Anfang zurück
															else  			{	if( Bar_Ende_erreicht )	 {	SONG_STEP = Letzter_Anfangs_Schritt; } }


															}


								// 1BAR  Sync Loop  -   läuft ewig....
								if( ++SONG_SYNC_Loop >= (beat.rythm * 4) ) {	SONG_SYNC_Loop = 0; }


	}	// ENDE    Song  Sequencer



	/// ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Beat SEQUENCER    jede 32th Note //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			// Am Anfang jeder NOTE
			if(TIC32 == 0) {	// Send Actual Song Step

								// Noten-Schritt-Zähler++   			WENN loop-ende erreicht ist   -   beginne von vorne:
								if( ++loop_step_pos >= act_LAST) {
																	// Berechne Loop - Start
																	First[beat.vari] = (beat.PAGE_loop[beat.vari][0] * beat.PAGE_Steps);	// First[beat.vari] step in loop =  0beat.PAGE_actual*16Perpage=0...
																	act_FIRST = First[beat.vari];	// synced ANFANG

																	Last[beat.vari]  = (beat.PAGE_loop[beat.vari][1] * beat.PAGE_Steps) + beat.PAGE_Steps;  // Last[beat.vari]  step in loop = (0beat.PAGE_actual*16Perpage)+16=16
																	act_LAST = Last[beat.vari];		// synced ENDE

																	// Berechne neue Loop-Position
																	loop_step_pos = act_FIRST;	// das schaltet das INTRO AUS!!! Normall sollte hier 0 sein, dann beginnts von vorne
																}

								// Letzter Schritt
								if( loop_step_pos == act_LAST-1) {	// Transfair Beat
																	if( Auftrag.BEAT_LOAD == 1 ) {	Auftrag.BEAT_LOAD = 0;	// Kill Flag
																									TRANSFAIR = 1;		// Send The Transfair command on Last Tic of this Step     SIEHE   TIC NR 47!
																								}
																}

								// make a global VARiable   of   the Step-Position
								StepSend = loop_step_pos;
						}



			// Am Ende jeder NOTE              Start=0    Ende= 47
			if(TIC32 == 47) {
								// Syncronisierte Überschreibung des Step-Sequencer INHALTS
								if(TRANSFAIR == 1)	{	TRANSFAIR = 0;				// Kill Flag
														Auftrag.BEAT_TRANSFAIR = 1;	// update currently played Beat
														Auftrag.BEAT_LOAD = 0;		// Kill Flag
													}
								}



//MIOS32_MIDI_SendDebugMessage("tic:%d", TIC32);

//send 32th Tics to Record
if(REC_Seq) {Trigger_REC(StepSend, TIC32);} // (u8 port, u8 note, u16 velocity, u8 step, u8 tic) Back to APP.c

// send 32th Tics to Triggermatrix to trigger Notes between 16th notes (human feel)
Trigger_Matrix(0, 127, 0, StepSend, TIC32); // (u8 port, u8 note, u16 velocity, u8 step, u8 tic) Back to APP.c




// End-drum-trigger
  return 0; //  no error,
}
