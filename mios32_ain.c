//! AIN-Treiber für MIOS32
//!
//! ADC-Kanäle, die konvertiert werden sollen, müssen mit einer Maske (MIOS32_AIN_CHANNEL_MASK) angegeben werden, die in der anwendungsspezifischen mios32_config.h-Datei hinzugefügt werden muss.
//!
//! Die Konvertierungsergebnisse werden durch DMA1 Channel 1 in das Array adc_conversion_values[] übertragen, um die CPU zu entlasten.
//!
//! Nach Abschluss des Scans wird der DMA-Kanalinterrupt aufgerufen, um die endgültigen (optional überabgetasteten) Werte zu berechnen und in das Array ain_pin_values[] zu übertragen, wenn die Wertänderung größer als der definierte MIOS32_AIN_DEADBAND ist (kann während der Laufzeit mit MIOS32_AIN_DeadbandSet() geändert werden).
//!
//! Wertänderungen (innerhalb des Totbandes) werden an den MIOS32_AIN_Handler() gemeldet.
//! Diese Funktion wird nicht direkt von der Anwendung aufgerufen, sondern ist Teil des Programmiermodell-Frameworks.
//! Wenn beispielsweise das "traditionelle" Framework verwendet wird, wird der AIN-Handler alle ms aufgerufen und ruft den Hook 'APP_AIN_NotifyChange(u32 pin, u32 pin_value)' bei Pin-Änderungen auf.<BR>
//! Der AIN-Handler löst nach Überprüfung aller Pins einen neuen Scan aus.
//!
//! Analogkanäle können über MBHP_AINX4-Module multiplexiert werden. Die Auswahl-Pins können mit einem beliebigen freien GPIO-Pin verbunden werden, die Zuordnungen müssen in der mios32_config.h-Datei hinzugefügt werden.<BR>
//! Normalerweise sind die 3 Auswahllinien mit J5C.A0/1/2 des Kernmoduls verbunden.
//! Zusammen mit den 8 Analogkanälen an J5A/B ergibt sich dies zu 64 Analogpins.
//!
//! Der AIN-Treiber ist flexibel genug, um die Anzahl der ADC-Kanäle auf mindestens 16 (verbunden mit J5A/B/C und J16) zu erhöhen. Zusammen mit 4 AINX4-Multiplexern ergeben sich dadurch 128 Analogkanäle.
//!
//! Es ist möglich, eine Übersampling-Rate festzulegen, die zu einer Akkumulation der Konvertierungsergebnisse führt, um die Auflösung zu erhöhen und die Genauigkeit zu verbessern.
//!
//! Es wurde ein spezieller Leerlaufmechanismus integriert, der sporadisches Flattern der AIN-Pins vermeidet, das aufgrund von EMI-Problemen auftreten könnte.<BR>
//! MIOS32_AIN_IDLE_CTR definiert die Anzahl der Konvertierungen, nach denen der Pin in den Leerlaufzustand wechselt, wenn keine Konvertierung das MIOS32_AIN_DEADBAND überschritten hat.
//! Im Leerlaufzustand wird stattdessen MIOS32_AIN_DEADBAND_IDLE verwendet, das größer ist (entsprechend ist der Pin weniger empfindlich). Der Pin verwendet wieder das ursprüngliche MIOS32_AIN_DEADBAND, sobald MIOS32_AIN_DEADBAND_IDLE überschritten wurde.<BR>
//! Diese Funktion kann deaktiviert werden, indem MIOS32_AIN_DEADBAND_IDLE in der Datei mios32_config.h auf 0 gesetzt wird.


#include <mios32.h>
#include "app.h"
#include "mios32_ain.h"


/////////////////////////////////////////////////////////////////////////////
// Lokale Variablen
/////////////////////////////////////////////////////////////////////////////

// Array für die ADC-Konvertierungswerte (wortausgerichtet für DMA-Übertragung)
static u16 adc_conversion_values[8] __attribute__((aligned(4)));


// Array für die AIN-Pin-Werte
static u16 ain_pin_values[8];

// Array für den Zustand der geänderten AIN-Pins
static u32 ain_pin_changed[8];



/////////////////////////////////////////////////////////////////////////////
// Lokale Konstanten
/////////////////////////////////////////////////////////////////////////////

// Tabelle zur Zuordnung der ADC-Kanäle zu J5.Ax-Pins
typedef struct {
  u8            chn;
  GPIO_TypeDef *port;
  u16           pin_mask;
} adc_chn_map_t;

static const adc_chn_map_t adc_chn_map[8] = {
  { ADC_Channel_11, GPIOC, GPIO_Pin_1 }, // J5A.A0
  { ADC_Channel_12, GPIOC, GPIO_Pin_2 }, // J5A.A1
  { ADC_Channel_1,  GPIOA, GPIO_Pin_1 }, // J5A.A2
  { ADC_Channel_4,  GPIOA, GPIO_Pin_4 }, // J5A.A3
  { ADC_Channel_14, GPIOC, GPIO_Pin_4 }, // J5B.A4
  { ADC_Channel_15, GPIOC, GPIO_Pin_5 }, // J5B.A5
  { ADC_Channel_8,  GPIOB, GPIO_Pin_0 }, // J5B.A6
  { ADC_Channel_9,  GPIOB, GPIO_Pin_1 }, // J5B.A7
};






//////////////////////////////////////////////////////////////////////////////////////////////////
// Initalisiere AIN
///////////////////
s32 MIOS32_AIN_Init(u32 mode) {

	#define MULTIPLIER (10) // for trigger dedection


  int i;


  // Arrays und Variablen löschen
  for (i=0;i<8;++i) {	adc_conversion_values[i] = 0; }

  for (i=0;i<8;++i) {	ain_pin_values[i] = 0; }

  for (i=0;i<8;++i) {	ain_pin_changed[i] = 0;	}

  // Analoge Pins konfigurieren
  GPIO_InitTypeDef GPIO_InitStructure;
  GPIO_StructInit(&GPIO_InitStructure);
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;

for(i=0;i<8;++i) {
				    GPIO_InitStructure.GPIO_Pin = adc_chn_map[i].pin_mask;
				    GPIO_Init(adc_chn_map[i].port, &GPIO_InitStructure);
					}

  // ADC1/2-Takt aktivieren
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_ADC2, ENABLE);

  // Kanäle den Konvertierungsslots zuordnen, abhängig von der Kanalauswahlmaske
  // Dies wird auf die beiden ADCs verteilt, um Kanäle parallel zu konvertieren
u8 num_channels = 0;

for (i=0; i<8;++i) {
				    ADC_RegularChannelConfig(
				        (num_channels & 1) ? ADC2 : ADC1,
				        adc_chn_map[i].chn,
				        (num_channels >> 1) + 1,
				        ADC_SampleTime_144Cycles);
				    ++num_channels;
					}

  // Stellen sicher, dass num_used_channels eine gerade Anzahl ist, um ADC2 mit ADC1 synchron zu halten


  // ADCs konfigurieren
  ADC_CommonInitTypeDef ADC_CommonInitStructure;
  ADC_CommonStructInit(&ADC_CommonInitStructure);
  ADC_CommonInitStructure.ADC_Mode = ADC_DualMode_RegSimult;
  ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div8;	// orginal Div2
  ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_2;
  ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
  ADC_CommonInit(&ADC_CommonInitStructure);

  ADC_InitTypeDef ADC_InitStructure;
  ADC_StructInit(&ADC_InitStructure);
  ADC_InitStructure.ADC_ScanConvMode = ENABLE;
  ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
  ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
  ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
  ADC_InitStructure.ADC_NbrOfConversion = 4;
  ADC_Init(ADC1, &ADC_InitStructure);
  ADC_Init(ADC2, &ADC_InitStructure);

  // ADC1->DMA-Anforderung aktivieren
  ADC_DMACmd(ADC1, ENABLE);
  ADC_MultiModeDMARequestAfterLastTransferCmd(ENABLE);

  // ADCs aktivieren
  ADC_Cmd(ADC1, ENABLE);
  ADC_Cmd(ADC2, ENABLE);

  // DMA2-Takt aktivieren
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);

  // DMA1-Kanal 1 konfigurieren, um Daten aus dem ADC-Ergebnisregister abzurufen
  DMA_Cmd(DMA2_Stream0, DISABLE);
  DMA_InitTypeDef DMA_InitStructure;
  DMA_StructInit(&DMA_InitStructure);
  DMA_InitStructure.DMA_Channel = DMA_Channel_0;
  DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)&ADC->CDR;
  DMA_InitStructure.DMA_Memory0BaseAddr = (u32)&adc_conversion_values;
  DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
  DMA_InitStructure.DMA_BufferSize = 4; // Anzahl der Konvertierungen abhängig von der Anzahl der verwendeten Kanäle
  DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
  DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
  DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
  DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
  DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;		// orginal: DMA_Mode_Circular
  DMA_InitStructure.DMA_Priority = DMA_Priority_High;
  DMA_Init(DMA2_Stream0, &DMA_InitStructure);
  DMA_Cmd(DMA2_Stream0, ENABLE);

  // Interrupt auslösen, wenn alle Konvertierungswerte abgerufen wurden
  DMA_ITConfig(DMA2_Stream0, DMA_IT_TC, ENABLE);

  // DMA-Interrupt konfigurieren und aktivieren
  MIOS32_IRQ_Install(DMA2_Stream0_IRQn, MIOS32_IRQ_AIN_DMA_PRIORITY);

  // Starte die erste Konvertierung
  MIOS32_AIN_StartConversions();

  // Timer für die periodische Auslösung der AIN-Scans initialisieren
	//MIOS32_TIMER_Init(0, 1, &MIOS32_AIN_StartConversions, 2);


  return 0;
}



/////////////////////////////////////////////////////////////////////////////
//! Starts an ADC conversion
//! In this case, the MIOS32_AIN_StartConversions() function has to be called
//! periodically from the application (e.g. from a timer), and conversion values
//! can be retrieved with MIOS32_AIN_PinGet()
//! \return < 0 on errors
/////////////////////////////////////////////////////////////////////////////
void MIOS32_AIN_StartConversions(void) {

  ADC_SoftwareStartConv(ADC1);


// Multiplikator für die Umrechnung von Benutzereingabe in Millisekunden

	static int Trigger[8] = {0};
	static int Max_AIN_Value[8] = {0};
	static s32 MaxTimerCounter[8] = {0};
	static s32 CooldownTimerCounter[8] = {0};


	int pin;
  // Überprüfe geänderte AIN-Wandlungswerte
  for(pin=0; pin<8; ++pin){

							// Ein neuer Trigger wird erkannt																// der Timer wird auf seinen maximum wert initialisiert
							if(ain_pin_values[pin] > sys.Schwellwert[pin] && Max_AIN_Value[pin] < sys.Schwellwert[pin]) {	MaxTimerCounter[pin] = sys.ZeitAmpMaximum[pin] * MULTIPLIER;

																															// Wir setzen eine Flag - damit wissen wir, wir analysieren gerade einen Trigger
																															Trigger[pin] = 1;

																															// Setze den ersten Maximalwert
																															Max_AIN_Value[pin] = ain_pin_values[pin];
																															}
							// Trigger wird analysiert - er befindet sich in der ersten phase
							if(Trigger[pin]==1){

												// Timer zählt runter bis auf 0
												MaxTimerCounter[pin]--;

												// Wenn ein neuer AIN-Wert den aktuellen maximalen Wert ÜBER-schreitet, wird der maximale Wert aktualisiert.
												if(ain_pin_values[pin] > Max_AIN_Value[pin]) { Max_AIN_Value[pin] = ain_pin_values[pin]; }

												// Wenn ein neuer AIN-Wert den aktuellen maximalen Wert UNTER-schreitet, haben wir den Peak erreicht  ||  Die analysezeit wurde überschritten
												if(ain_pin_values[pin] < Max_AIN_Value[pin] || MaxTimerCounter[pin]<=0) {
																															APP_AIN_NotifyChange(pin, Max_AIN_Value[pin]);
																															MaxTimerCounter[pin] = 0;
																															CooldownTimerCounter[pin] = sys.ZeitSchlagMaximum[pin] * MULTIPLIER;
																															Trigger[pin] = 2;		// wir gehen in die nächste phase
																															}
												}

							// Trigger wurde ausgeführt, nun verhindern wir ein zufrühes Erneutes Trigger - Cooldown
							if(Trigger[pin]==2){
												// Timer zählt runter auf 0
												CooldownTimerCounter[pin]--;

												if(CooldownTimerCounter[pin]<=0) {

																					// Reset Counter
																					CooldownTimerCounter[pin] = 0;

																					Max_AIN_Value[pin] = 0;	//Zurücksetzen für nächste Triggererkennung

																					// Trigger zurücksetzen
																					Trigger[pin] = 0;
																				}




												}
							}
}





void DMA2_Stream0_IRQHandler(void) {

    int i;
    uint16_t *src_ptr, *dst_ptr;

    // Flag(s) löschen
    DMA_ClearFlag(DMA2_Stream0, DMA_FLAG_TCIF0 | DMA_FLAG_TEIF0 | DMA_FLAG_HTIF0 | DMA_FLAG_FEIF0);

    src_ptr = (uint16_t *)adc_conversion_values;
    dst_ptr = (uint16_t *)ain_pin_values;

    // Kopiere die konvertierten ADC-Werte in das Array ain_pin_values
    for(i=0; i<8;++i){
						*dst_ptr = *src_ptr;
						++dst_ptr;
						++src_ptr;
						}
MIOS32_AIN_StartConversions();
}
