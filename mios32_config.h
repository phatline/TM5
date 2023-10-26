//this file allows to re-configure default functions of MIOS32
//available switches are listed in $MIOS32_PATH/modules/mios32/MIOS32_CONFIG.txt

#ifndef _MIOS32_CONFIG_H
#define _MIOS32_CONFIG_H

// map MIDI mutex to BLM_SCALAR master
// located in tasks.c to access MIDI IN/OUT mutex from external
//extern void TASKS_MUTEX_MIDIOUT_Take(void);
//extern void TASKS_MUTEX_MIDIOUT_Give(void);
//#define BLM_SCALAR_MASTER_MUTEX_MIDIOUT_TAKE { TASKS_MUTEX_MIDIOUT_Take(); }
//#define BLM_SCALAR_MASTER_MUTEX_MIDIOUT_GIVE { TASKS_MUTEX_MIDIOUT_Give(); }

#define MIOS32_USB_MIDI_NUM_PORTS 4           // we provide 4 USB ports

#define MIOS32_USE_MIDI
#define MIOS32_USE_UART
#define MIOS32_USE_UART_MIDI
#define MIOS32_UART_NUM 4
#define MIOS32_UART0_ASSIGNMENT 1 //1=Midi, 0= Disabled, 2= COM
#define MIOS32_UART1_ASSIGNMENT 1
#define MIOS32_UART2_ASSIGNMENT 1
#define MIOS32_UART3_ASSIGNMENT 1

// SHift Registers
		// DOUTX4  SR: 0 1 2 3
		//
		// BLM:::::::::::::::::::::::::::::
		// MOD 0  SR: 4 Common	Cathode
		// MOD 0  SR: 5 Green	Color
		// MOD 0  SR: 6 Blue	Color
		//
		// MOD 1  SR: 7 Common	Cathode
		// MOD 1  SR: 8 Green	Color
		// MOD 1  SR: 9 Blue	Color
		//
		// MOD 2  SR: 10 Common	Cathode
		// MOD 2  SR: 11 Green	Color
		// MOD 2  SR: 12 Blue	Color
		//
		// MOD 3  SR: 13 Common	Cathode
		// MOD 3  SR: 14 Green	Color
		// MOD 3  SR: 15 Blue	Color
		//
		// MOD 4  SR: 16 Common	Cathode		// Extra H
		// MOD 4  SR: 17 Green	Color		// Extra H-V Color
		// MOD 4  SR: 18 Blue	Color		// Extra H-V Color

#define MIOS32_SRIO_NUM_SR 32 //  BLMD-DIN: 5, DINx3: 3  =  summed 8   +++ DOUT Pages 19 = 27
//#define MIOS32_SRIO_NUM_DOUT_PAGES 19 // BLM-DOUT: 5*3=15,  DOUTx4: 4  summed: 19






#define MIOS32_ENC_NUM_MAX 3

#define MIOS32_DONT_USE_MF 1
#define MIOS32_DONT_USE_OSC 1
#define MIOS32_DONT_USE_COM 1
// #define MIOS32_DONT_USE_IIC
// #define MIOS32_DONT_USE_IIC_MIDI
//#define MIOS32_USE_I2S

#define MIOS32_DONT_USE_ENC28J60

#define MIOS32_LCD_BOOT_MSG_LINE1 "TM           "
//                                "             "
#define MIOS32_LCD_BOOT_MSG_LINE2 "             "


// AIN define the deadband (min. difference to report a change to the application hook)
#define MIOS32_AIN_DEADBAND 32 //7Bit
#define MIOS32_AIN_OVERSAMPLING_RATE  1
#define MIOS32_AIN_IDLE_CTR 1500 //1,5sec goes into idle ---standart 3s
#define MIOS32_DONT_USE_AIN 1



#endif /* _MIOS32_CONFIG_H */
