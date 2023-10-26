/*
Die SD-Karte wird über SPI1 (J16) oder alternativ über SPI2 (J8/9) angesprochen. Eine Lösung für alternative Ports wird später bereitgestellt.

Die Karte muss mit 3,3V versorgt werden.

Das SDIO-Peripheriegerät wird nicht verwendet, um die Kompatibilität mit "mid density" Geräten der STM32-Familie und zukünftigen Derivaten zu gewährleisten.

MIOS32_SDCARD_Init(0) muss nur einmal aufgerufen werden, um den Treiber zu initialisieren.

MIOS32_SDCARD_CheckAvailable() sollte aufgerufen werden, um eine Verbindung mit der SD-Karte herzustellen.
* Wenn 0 zurückgegeben wird, kann davon ausgegangen werden, dass keine SD-Karte angeschlossen ist. Die Funktion kann periodisch von einer Task mit niedriger Priorität aufgerufen werden,
* um eine Verbindung erneut zu versuchen oder eine automatische Erkennung während der Laufzeit durchzuführen.


MIOS32_SDCARD_SectorRead/SectorWrite ermöglichen das Lesen/Schreiben eines 512-Byte-Sektors.

* Wenn ein solcher Zugriff einen Fehler zurückgibt, kann davon ausgegangen werden, dass die SD-Karte während des Transfers getrennt wurde.

* Da DMA für serielle Übertragungen verwendet wird, dauert das Lesen/Schreiben eines Sektors in der Regel ca. 500 µs. Daher beträgt die erreichbare Übertragungsrate ca. 1 MByte/s (8 MBit/s).

Am 08. Januar 2009 wurde eine Änderung von philetaylor vorgenommen, um auch MMC- und HCSD-Unterstützung hinzuzufügen.
* Es wurde auch ein erster Versuch unternommen, einen Mutex zu verwenden, um die gemeinsame Nutzung von SPI0 zwischen SD-Karten und dem ENC28J60-Ethernet zu ermöglichen.
* Wenn dies auf diese Weise verwendet wird, muss die SD-Karte beim Einschalten eingesteckt sein und wird nicht erkannt, wenn sie danach eingesteckt wird. Hoffentlich wird dafür eine Lösung gefunden!

*/



/////////////////////////////////////////////////////////////////////////////
// Include files
/////////////////////////////////////////////////////////////////////////////

#include <mios32.h>


/////////////////////////////////////////////////////////////////////////////
// Local definitions
/////////////////////////////////////////////////////////////////////////////

#if !defined(MIOS32_SDCARD_MUTEX_TAKE)
#define MIOS32_SDCARD_MUTEX_TAKE {}
#define MIOS32_SDCARD_MUTEX_GIVE {}
#endif


// SPI prescaler used for fast transfers
# define MIOS32_SDCARD_SPI_PRESCALER MIOS32_SPI_PRESCALER_4



/* Definitions for MMC/SDC command */
#define SDCMD_GO_IDLE_STATE		(0x40+0)
#define SDCMD_GO_IDLE_STATE_CRC	0x95

#define SDCMD_SEND_OP_COND		(0x40+1)
#define SDCMD_SEND_OP_COND_CRC	0xf9

#define SDCMD_SEND_OP_COND_SDC	(0xC0+41)
#define SDCMD_SEND_OP_COND_SDC_CRC	0xff

#define SDCMD_READ_OCR			(0x40+58)
#define SDCMD_READ_OCR_CRC		0xff

#define SDCMD_APP_CMD			(0x40+55)
#define SDCMD_APP_CMD_CRC		0xff

#define SDCMD_SEND_IF_COND		(0x40+8)
#define SDCMD_SEND_IF_COND_CRC	0x87

#define SDCMD_SEND_CSD			(0x40+9)
#define SDCMD_SEND_CSD_CRC      0xff

#define SDCMD_SEND_CID	        (0x40+10)
#define SDCMD_SEND_CID_CRC      0xff

#define SDCMD_SEND_STATUS		(0x40+13)
#define SDCMD_SEND_STATUS_CRC	0xaf

#define SDCMD_READ_SINGLE_BLOCK	(0x40+17)
#define SDCMD_READ_SINGLE_BLOCK_CRC 0xff

#define SDCMD_SET_BLOCKLEN		(0x40+16)
#define SDCMD_SET_BLOCKLEN_CRC 	0xff

#define SDCMD_WRITE_SINGLE_BLOCK (0x40+24)
#define SDCMD_WRITE_SINGLE_BLOCK_CRC 0xff


/* Card type flags (CardType) */
#define CT_MMC				0x01
#define CT_SD1				0x02
#define CT_SD2				0x04
#define CT_SDC				(CT_SD1|CT_SD2)
#define CT_BLOCK			0x08
static u8 CardType;




/////////////////////////////////////////////////////////////////////////////
//! Initializes SPI pins and peripheral to access MMC/SD Card
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_SDCARD_Init(u32 mode){

  MIOS32_SDCARD_MUTEX_TAKE;

		  // ensure that fast pin drivers are activated
		  MIOS32_SPI_IO_Init(MIOS32_SDCARD_SPI, MIOS32_SPI_PIN_DRIVER_STRONG);

		  // init SPI port for slow frequency access (ca. 0.3 MBit/s)
		  MIOS32_SPI_TransferModeInit(MIOS32_SDCARD_SPI, MIOS32_SPI_MODE_CLK1_PHASE1, MIOS32_SPI_PRESCALER_256);	// 303 warum langsam?

  MIOS32_SDCARD_MUTEX_GIVE;

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! Connects to SD Card
//! \return < 0 if initialisation sequence failed
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_SDCARD_PowerOn(void) {

  s32 status = 0;
  int i;

  MIOS32_SDCARD_MUTEX_TAKE;

		  // ensure that chip select line deactivated
		  MIOS32_SPI_RC_PinSet(MIOS32_SDCARD_SPI, MIOS32_SDCARD_SPI_RC_PIN, 1); // spi, rc_pin, pin_value

		  // init SPI port for slow frequency access (ca. 0.3 MBit/s)
		  MIOS32_SPI_TransferModeInit(MIOS32_SDCARD_SPI, MIOS32_SPI_MODE_CLK1_PHASE1, MIOS32_SPI_PRESCALER_256);	// 303 warum langsam

		  // send 80 clock cycles to start up
		  for(i=0; i<10; ++i)
		    MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);

		  // activate chip select
		  MIOS32_SPI_RC_PinSet(MIOS32_SDCARD_SPI, MIOS32_SDCARD_SPI_RC_PIN, 0); // spi, rc_pin, pin_value

		  // wait for 1 mS
		  MIOS32_DELAY_Wait_uS(1000);

		  // send CMD0 to reset the media
		  if( (status=MIOS32_SDCARD_SendSDCCmd(SDCMD_GO_IDLE_STATE, 0, SDCMD_GO_IDLE_STATE_CRC)) < 0 )
		    goto error;

		  CardType=0;


		  // A card is detected, what type is it? Use SEND_IF_COND (CMD8) to find out
		  if ((status=MIOS32_SDCARD_SendSDCCmd(SDCMD_SEND_IF_COND, 0x1AA, SDCMD_SEND_IF_COND_CRC)) == 0x01aa ) {

						// SDHC Card Detected.

						// We now check to see if we should use block mode or byte mode. Command is SEND_OP_COND_SDC (ACMD41) with HCS (bit 30) set
						for (i=0;i<16384;i++)
						  if((status=MIOS32_SDCARD_SendSDCCmd(SDCMD_SEND_OP_COND_SDC, 0x01<<30, SDCMD_SEND_OP_COND_SDC_CRC)) == 0 )
							break;

						if(i < 16384) {
						  status=MIOS32_SDCARD_SendSDCCmd(SDCMD_READ_OCR, 0, SDCMD_READ_OCR_CRC);
						  CardType=((status>>24) & 0x40)  ? CT_SD2 | CT_BLOCK : CT_SD2;
						  status=0;
						} else {
							// We waited long enough!
						    status = -2;
					    }
					  }



		  else { // Card is SDv1 or MMC.

								//MMC will accept ACMD41 (SDv1 won't and requires CMD1) so send ACMD41 first and then CMD1 if that fails
								if ((status=MIOS32_SDCARD_SendSDCCmd(SDCMD_SEND_OP_COND_SDC, 0, SDCMD_SEND_OP_COND_SDC_CRC)) <= 1 )
								  CardType=CT_SD1;
								else
							      CardType=CT_MMC;

								for (i=0;i<16384;i++) {

								  if (CardType==CT_SD1)
									status=MIOS32_SDCARD_SendSDCCmd(SDCMD_SEND_OP_COND_SDC, 0, SDCMD_SEND_OP_COND_SDC_CRC);
								  else
								    status=MIOS32_SDCARD_SendSDCCmd(SDCMD_SEND_OP_COND, 0, SDCMD_SEND_OP_COND_CRC);

								  if (status<0){
									break;
								  }
								  if (status==0)
									break;
								}

								// The block size should already be 512 bytes but re-initialize just in case (ignore if it fails)
							    MIOS32_SDCARD_SendSDCCmd(SDCMD_SET_BLOCKLEN, 512, SDCMD_SEND_OP_COND_CRC);
							  }


		  if( i == 16384 || CardType==0 )
		    status=-2; // the last loop timed out or the cardtype was not detected...
		error:
		  // deactivate chip select
		  MIOS32_SPI_RC_PinSet(MIOS32_SDCARD_SPI, MIOS32_SDCARD_SPI_RC_PIN, 1); // spi, rc_pin, pin_value

		  // Send dummy byte once deactivated to drop cards DO
		  MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);
  MIOS32_SDCARD_MUTEX_GIVE;
  return status; // Status should be 0 if nothing went wrong!
}





////////////////////////////////////////////////////////////////////////////
//! Wenn die SD-Karte zuvor verfügbar war: Überprüft, ob die SD-Karte immer noch
//! verfügbar ist, indem der STATUS-Befehl gesendet wird.<BR>
//! Dies dauert ca. 10 µs.
//!
//! Wenn die SD-Karte zuvor nicht verfügbar war: Überprüft, ob die SD-Karte
//! verfügbar ist, indem der IDLE-Befehl mit niedriger Geschwindigkeit gesendet wird.<BR>
//! Dies dauert ca. 500 µs!<BR>
//! Sobald eine positive Antwort erhalten wird, wird MIOS32_SDCARD_PowerOn()
//! von dieser Funktion aufgerufen, um die Karte vollständig zu initialisieren.
//!
//! Beispiel für die Erkennung von Verbindung/Unterbrechung:
//! \code
//! // Diese Funktion wird alle Sekunde von einer Low-Priority-Task aufgerufen
//! // Wenn mehrere Tasks auf die SD-Karte zugreifen, fügen Sie einen Semaphore/Mutex hinzu,
//! // um IO-Zugriffskollisionen mit anderen Tasks zu vermeiden!
//
//! u8 sdcard_available;
//! s32 SEQ_FILE_CheckSDCard(void)
//! {
//! // Überprüfen, ob die SD-Karte verfügbar ist
//! // Hochgeschwindigkeitszugriff, wenn die Karte zuvor verfügbar war
//! u8 prev_sdcard_available = sdcard_available;
//! sdcard_available = MIOS32_SDCARD_CheckAvailable(prev_sdcard_available);
//!
//! if( sdcard_available && !prev_sdcard_available ) {
//! // SD-Karte wurde angeschlossen
//!
//! // Jetzt ist es möglich, Sektoren zu lesen/schreiben
//!
//! } else if( !sdcard_available && prev_sdcard_available ) {
//! // SD-Karte wurde getrennt
//!
//! // Hier können Sie Ihre Anwendung über diesen Zustand informieren
//! }
//!
//! return 0; // kein Fehler
//! }
//! \endcode
//! \param[in] was_available sollte nur gesetzt werden, wenn die SD-Karte zuvor verfügbar war
//! \return 0, wenn keine Antwort von der SD-Karte erhalten wurde
//! \return 1, wenn auf die SD-Karte zugegriffen werden kann
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_SDCARD_CheckAvailable(u8 was_available){

  s32 ret;
  MIOS32_SDCARD_MUTEX_TAKE;

			if( was_available ){
								// init SPI port for fast frequency access (ca. 18 MBit/s)
								// this is required for the case that the SPI port is shared with other devices
								MIOS32_SPI_TransferModeInit(MIOS32_SDCARD_SPI, MIOS32_SPI_MODE_CLK1_PHASE1, MIOS32_SDCARD_SPI_PRESCALER);
								// activate chip select
								MIOS32_SPI_RC_PinSet(MIOS32_SDCARD_SPI, MIOS32_SDCARD_SPI_RC_PIN, 0); // spi, rc_pin, pin_value

								// send STATUS command to check if media is available
								ret=MIOS32_SDCARD_SendSDCCmd(SDCMD_SEND_STATUS, 0, SDCMD_SEND_STATUS_CRC);
								}
			else {

							    // ensure that SPI interface is clocked at low speed
							    MIOS32_SPI_TransferModeInit(MIOS32_SDCARD_SPI, MIOS32_SPI_MODE_CLK1_PHASE1, MIOS32_SPI_PRESCALER_256);

							    // deactivate chip select
							    MIOS32_SPI_RC_PinSet(MIOS32_SDCARD_SPI, MIOS32_SDCARD_SPI_RC_PIN, 1); // spi, rc_pin, pin_value
							    // send 80 clock cycles to start up
								u8 i;
							    for(i=0; i<10; ++i)
							       MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);
							    // activate chip select
							    MIOS32_SPI_RC_PinSet(MIOS32_SDCARD_SPI, MIOS32_SDCARD_SPI_RC_PIN, 0); // spi, rc_pin, pin_value

							    // send CMD0 to reset the media
							    if( (ret=(MIOS32_SDCARD_SendSDCCmd(SDCMD_GO_IDLE_STATE, 0, SDCMD_GO_IDLE_STATE_CRC))) < 0 )
								  goto not_available;

							    // deactivate chip select
							    MIOS32_SPI_RC_PinSet(MIOS32_SDCARD_SPI, MIOS32_SDCARD_SPI_RC_PIN, 1); // spi, rc_pin, pin_value

							    MIOS32_SDCARD_MUTEX_GIVE;
							    // run power-on sequence (negative return = not available)
							    ret=MIOS32_SDCARD_PowerOn();
								}


			not_available:
			// deactivate chip select
			MIOS32_SPI_RC_PinSet(MIOS32_SDCARD_SPI, MIOS32_SDCARD_SPI_RC_PIN, 1); // spi, rc_pin, pin_value

			// Send dummy byte once deactivated to drop cards DO
			MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);


  MIOS32_SDCARD_MUTEX_GIVE;


  return (ret == 0) ? 1 : 0; // 1=available, 0=not available.
}


/////////////////////////////////////////////////////////////////////////////
//! Sends command to SD card
//! \param[in] cmd SD card command
//! \param[in] addr 32bit address
//! \param[in] crc precalculated CRC
//! \return >= 0x00 if command has been sent successfully (contains received byte)
//! \return -1 if no response from SD Card (timeout)
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_SDCARD_SendSDCCmd(u8 cmd, u32 addr, u8 crc) {

  int i;
  s32 ret;

  if (cmd & 0x80) {	/* ACMD<n> is the command sequence of CMD55-CMD<n> */
	cmd &= 0x7F;
	ret = MIOS32_SDCARD_SendSDCCmd(SDCMD_APP_CMD, 0,SDCMD_APP_CMD_CRC);
	if (ret > 1) return ret;
  }
  // activate chip select
  MIOS32_SPI_RC_PinSet(MIOS32_SDCARD_SPI, MIOS32_SDCARD_SPI_RC_PIN, 0); // spi, rc_pin, pin_value

  // transfer to card
  MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, (u8)cmd);
  MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, (addr >> 24) & 0xff);
  MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, (addr >> 16) & 0xff);
  MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, (addr >>  8) & 0xff);
  MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, (addr >>  0) & 0xff);
  MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, crc);

  u8 timeout = 0;

  if( cmd == SDCMD_SEND_STATUS ) {

  // one dummy read
    MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);

    // read two bytes (only last value will be returned)
    ret=MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);
    ret=MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);

    // all-one read: we expect that SD card is not connected: notify timeout!
    if( ret == 0xff )
      timeout = 1;

  } else {
    // wait for standard R1 response
    for(i=0; i<8; ++i) {
      if( (ret=MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff)) != 0xff )
	    break;
    }
    if( i == 8 )
      timeout = 1;
  }

  if ((cmd == SDCMD_SEND_IF_COND || cmd == SDCMD_READ_OCR) && timeout!=1){

	// This is a 4 byte R3 or R7 response.
    ret=(MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff)<<24);
    ret|=(MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff)<<16);
    ret|=(MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff)<<8);
    ret|=(MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff)<<0);

  }

  // required clocking (see spec)
  MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);

  // deactivate chip-select on timeout, and return error code
  if( timeout ) {
    MIOS32_SPI_RC_PinSet(MIOS32_SDCARD_SPI, MIOS32_SDCARD_SPI_RC_PIN, 1); // spi, rc_pin, pin_value

	// Send dummy byte once deactivated to drop cards DO
    MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);
    return -1;
  }

  // else return received value - don't deactivate chip select or mutex (for continuous access)
  return ret;
}


/////////////////////////////////////////////////////////////////////////////
//! Reads 512 bytes from selected sector
//! \param[in] sector 32bit sector
//! \param[in] *buffer pointer to 512 byte buffer
//! \return 0 if whole sector has been successfully read
//! \return -error if error occured during read operation:<BR>
//! <UL>
//!   <LI>Bit 0              - In idle state if 1
//!   <LI>Bit 1              - Erase Reset if 1
//!   <LI>Bit 2              - Illgal Command if 1
//!   <LI>Bit 3              - Com CRC Error if 1
//!   <LI>Bit 4              - Erase Sequence Error if 1
//!   <LI>Bit 5              - Address Error if 1
//!   <LI>Bit 6              - Parameter Error if 1
//!   <LI>Bit 7              - Not used, always '0'
//! </UL>
//! \return -256 if timeout during command has been sent
//! \return -257 if timeout while waiting for start token
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_SDCARD_SectorRead(u32 sector, u8 *buffer)
{
  s32 status = 0;
  int i;
  if (!(CardType & CT_BLOCK))
	sector *= 512;

  MIOS32_SDCARD_MUTEX_TAKE;

  // init SPI port for fast frequency access (ca. 18 MBit/s)
  // this is required for the case that the SPI port is shared with other devices
  MIOS32_SPI_TransferModeInit(MIOS32_SDCARD_SPI, MIOS32_SPI_MODE_CLK1_PHASE1, MIOS32_SDCARD_SPI_PRESCALER);

  if( (status=MIOS32_SDCARD_SendSDCCmd(SDCMD_READ_SINGLE_BLOCK, sector, SDCMD_READ_SINGLE_BLOCK_CRC)) ) {
    status=(status < 0) ? -256 : status; // return timeout indicator or error flags
    goto error;
  }

  // wait for start token of the data block
  for(i=0; i<65536; ++i) { // TODO: check if sufficient
    u8 ret = MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);
    if( ret != 0xff )
      break;
  }
  if( i == 65536 ) {
    status= -257;
    goto error;
  }

  // read 512 bytes via DMA
#ifdef MIOS32_SDCARD_TASK_SUSPEND_HOOK
  MIOS32_SPI_TransferBlock(MIOS32_SDCARD_SPI, NULL, buffer, 512, MIOS32_SDCARD_TASK_RESUME_HOOK);
  MIOS32_SDCARD_TASK_SUSPEND_HOOK();
#else
  MIOS32_SPI_TransferBlock(MIOS32_SDCARD_SPI, NULL, buffer, 512, NULL);
#endif

  // read (and ignore) CRC
  MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);
  MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);

  // required for clocking (see spec)
  MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);

error:
  // deactivate chip select
  MIOS32_SPI_RC_PinSet(MIOS32_SDCARD_SPI, MIOS32_SDCARD_SPI_RC_PIN, 1); // spi, rc_pin, pin_value

  // Send dummy byte once deactivated to drop cards DO
  MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);
  MIOS32_SDCARD_MUTEX_GIVE;
  return status;
}


/////////////////////////////////////////////////////////////////////////////
//! Writes 512 bytes into selected sector
//! \param[in] sector 32bit sector
//! \param[in] *buffer pointer to 512 byte buffer
//! \return 0 if whole sector has been successfully read
//! \return -error if error occured during read operation:<BR>
//! <UL>
//!   <LI>Bit 0              - In idle state if 1
//!   <LI>Bit 1              - Erase Reset if 1
//!   <LI>Bit 2              - Illgal Command if 1
//!   <LI>Bit 3              - Com CRC Error if 1
//!   <LI>Bit 4              - Erase Sequence Error if 1
//!   <LI>Bit 5              - Address Error if 1
//!   <LI>Bit 6              - Parameter Error if 1
//!   <LI>Bit 7              - Not used, always '0'
//! </UL>
//! \return -256 if timeout during command has been sent
//! \return -257 if write operation not accepted
//! \return -258 if timeout during write operation
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_SDCARD_SectorWrite(u32 sector, u8 *buffer)
{
  s32 status = 0;
  int i;

  MIOS32_SDCARD_MUTEX_TAKE;

  if (!(CardType & CT_BLOCK))
	sector *= 512;

  // init SPI port for fast frequency access (ca. 18 MBit/s)
  // this is required for the case that the SPI port is shared with other devices
  MIOS32_SPI_TransferModeInit(MIOS32_SDCARD_SPI, MIOS32_SPI_MODE_CLK1_PHASE1, MIOS32_SDCARD_SPI_PRESCALER);

  if( (status=MIOS32_SDCARD_SendSDCCmd(SDCMD_WRITE_SINGLE_BLOCK, sector, SDCMD_WRITE_SINGLE_BLOCK_CRC)) ) {
    status=(status < 0) ? -256 : status; // return timeout indicator or error flags
    goto error;
  }

  // send start token
  MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xfe);

  // send 512 bytes of data via DMA
#ifdef MIOS32_SDCARD_TASK_SUSPEND_HOOK
  MIOS32_SPI_TransferBlock(MIOS32_SDCARD_SPI, buffer, NULL, 512, MIOS32_SDCARD_TASK_RESUME_HOOK);
  MIOS32_SDCARD_TASK_SUSPEND_HOOK();
#else
  MIOS32_SPI_TransferBlock(MIOS32_SDCARD_SPI, buffer, NULL, 512, NULL);
#endif

  // send CRC
  MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);
  MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);

  // read response
  u8 response = MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);
  if( (response & 0x0f) != 0x5 ) {
    status= -257;
    goto error;
  }

  // wait for write completion
  for(i=0; i<32*65536; ++i) { // TODO: check if sufficient
    u8 ret = MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);
    if( ret != 0x00 )
      break;
  }
  if( i == 32*65536 ) {

    status= -258;
    goto error;
  }

  // required for clocking (see spec)
  MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);

error:
  // deactivate chip select
  MIOS32_SPI_RC_PinSet(MIOS32_SDCARD_SPI, MIOS32_SDCARD_SPI_RC_PIN, 1); // spi, rc_pin, pin_value
  // Send dummy byte once deactivated to drop cards DO
  MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);

  MIOS32_SDCARD_MUTEX_GIVE;

  return status;
}


/////////////////////////////////////////////////////////////////////////////
//! Reads the CID informations from SD Card
//! \param[in] *cid pointer to buffer which holds the CID informations
//! \return 0 if the informations haven been successfully read
//! \return -error if error occured during read operation
//! \return -256 if timeout during command has been sent
//! \return -257 if timeout while waiting for start token
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_SDCARD_CIDRead(mios32_sdcard_cid_t *cid)
{
  s32 status = 0;
  int i;

  // init SPI port for fast frequency access (ca. 18 MBit/s)
  // this is required for the case that the SPI port is shared with other devices
  MIOS32_SDCARD_MUTEX_TAKE;

  MIOS32_SPI_TransferModeInit(MIOS32_SDCARD_SPI, MIOS32_SPI_MODE_CLK1_PHASE1, MIOS32_SDCARD_SPI_PRESCALER);

  if( (status=MIOS32_SDCARD_SendSDCCmd(SDCMD_SEND_CID, 0, SDCMD_SEND_CID_CRC)) ) {
    status=(status < 0) ? -256 : status; // return timeout indicator or error flags
    goto error;
  }

  // wait for start token of the data block
  for(i=0; i<65536; ++i) { // TODO: check if sufficient
    u8 ret = MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);
    if( ret != 0xff )
      break;
  }
  if( i == 65536 )
    status= -257;


  // read 16 bytes via DMA
  u8 cid_buffer[16];
  MIOS32_SPI_TransferBlock(MIOS32_SDCARD_SPI, NULL, cid_buffer, 16, NULL);

  // read (and ignore) CRC
  MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);
  MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);

  // required for clocking (see spec)
  MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);


  // sort returned informations into CID structure
  // from STM Mass Storage example
  /* Byte 0 */
  cid->ManufacturerID = cid_buffer[0];
  /* Byte 1 */
  cid->OEM_AppliID = cid_buffer[1] << 8;
  /* Byte 2 */
  cid->OEM_AppliID |= cid_buffer[2];
  /* Byte 3..7 */
  for(i=0; i<5; ++i)
    cid->ProdName[i] = cid_buffer[3+i];
  cid->ProdName[5] = 0; // string terminator
  /* Byte 8 */
  cid->ProdRev = cid_buffer[8];
  /* Byte 9 */
  cid->ProdSN = cid_buffer[9] << 24;
  /* Byte 10 */
  cid->ProdSN |= cid_buffer[10] << 16;
  /* Byte 11 */
  cid->ProdSN |= cid_buffer[11] << 8;
  /* Byte 12 */
  cid->ProdSN |= cid_buffer[12];
  /* Byte 13 */
  cid->Reserved1 |= (cid_buffer[13] & 0xF0) >> 4;
  /* Byte 14 */
  cid->ManufactDate = (cid_buffer[13] & 0x0F) << 8;
  /* Byte 15 */
  cid->ManufactDate |= cid_buffer[14];
  /* Byte 16 */
  cid->msd_CRC = (cid_buffer[15] & 0xFE) >> 1;
  cid->Reserved2 = 1;

error:
  // deactivate chip select
  MIOS32_SPI_RC_PinSet(MIOS32_SDCARD_SPI, MIOS32_SDCARD_SPI_RC_PIN, 1); // spi, rc_pin, pin_value
  // Send dummy byte once deactivated to drop cards DO
  MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);
  MIOS32_SDCARD_MUTEX_GIVE;

  return status;
}

/////////////////////////////////////////////////////////////////////////////
//! Reads the CSD informations from SD Card
//! \param[in] *csd pointer to buffer which holds the CSD informations
//! \return 0 if the informations haven been successfully read
//! \return -error if error occured during read operation
//! \return -256 if timeout during command has been sent
//! \return -257 if timeout while waiting for start token
/////////////////////////////////////////////////////////////////////////////
s32 MIOS32_SDCARD_CSDRead(mios32_sdcard_csd_t *csd)
{
  s32 status = 0;
  int i;

  MIOS32_SDCARD_MUTEX_TAKE;

  // init SPI port for fast frequency access (ca. 18 MBit/s)
  // this is required for the case that the SPI port is shared with other devices
  MIOS32_SPI_TransferModeInit(MIOS32_SDCARD_SPI, MIOS32_SPI_MODE_CLK1_PHASE1, MIOS32_SDCARD_SPI_PRESCALER);

  if( (status=MIOS32_SDCARD_SendSDCCmd(SDCMD_SEND_CSD, 0, SDCMD_SEND_CSD_CRC)) ) {
    status=(status < 0) ? -256 : status; // return timeout indicator or error flags
    goto error;
  }
  // wait for start token of the data block
  for(i=0; i<65536; ++i) { // TODO: check if sufficient
    u8 ret = MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);
    if( ret != 0xff )
      break;
  }
  if( i == 65536 ) {
    status= -257;
    goto error;
  }

  // read 16 bytes via DMA
  u8 csd_buffer[16];
  MIOS32_SPI_TransferBlock(MIOS32_SDCARD_SPI, NULL, csd_buffer, 16, NULL);

  // read (and ignore) CRC
  MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);
  MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);

  // required for clocking (see spec)
  MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);

  // sort returned informations into CSD structure
  // from STM Mass Storage example
  /* Byte 0 */

  csd->CSDStruct = (csd_buffer[0] & 0xC0) >> 6;
  csd->SysSpecVersion = (csd_buffer[0] & 0x3C) >> 2;
  csd->Reserved1 = csd_buffer[0] & 0x03;
  /* Byte 1 */
  csd->TAAC = csd_buffer[1] ;
  /* Byte 2 */
  csd->NSAC = csd_buffer[2];
  /* Byte 3 */
  csd->MaxBusClkFrec = csd_buffer[3];
  /* Byte 4 */
  csd->CardComdClasses = csd_buffer[4] << 4;
  /* Byte 5 */
  csd->CardComdClasses |= (csd_buffer[5] & 0xF0) >> 4;
  csd->RdBlockLen = csd_buffer[5] & 0x0F;
  /* Byte 6 */
  csd->PartBlockRead = (csd_buffer[6] & 0x80) >> 7;
  csd->WrBlockMisalign = (csd_buffer[6] & 0x40) >> 6;
  csd->RdBlockMisalign = (csd_buffer[6] & 0x20) >> 5;
  csd->DSRImpl = (csd_buffer[6] & 0x10) >> 4;
  csd->Reserved2 = 0; /* Reserved */

  if (csd->CSDStruct != 1) { //SD V1
	csd->DeviceSize = (csd_buffer[6] & 0x03) << 10;
	/* Byte 7 */
	csd->DeviceSize |= (csd_buffer[7]) << 2;
	/* Byte 8 */
	csd->DeviceSize |= (csd_buffer[8] & 0xC0) >> 6;
	csd->MaxRdCurrentVDDMin = (csd_buffer[8] & 0x38) >> 3;
	csd->MaxRdCurrentVDDMax = (csd_buffer[8] & 0x07);
    /* Byte 9 */
    csd->MaxWrCurrentVDDMin = (csd_buffer[9] & 0xE0) >> 5;
    csd->MaxWrCurrentVDDMax = (csd_buffer[9] & 0x1C) >> 2;
    csd->DeviceSizeMul = (csd_buffer[9] & 0x03) << 1;
    /* Byte 10 */
    csd->DeviceSizeMul |= (csd_buffer[10] & 0x80) >> 7;
  } else { // SD V2
	/* Byte 7 */
    csd->DeviceSize = (csd_buffer[7] & 0x03)<<16;
	/* Byte 8 */
    csd->DeviceSize |= (csd_buffer[8] & 0xff)<<8;
    /* Byte 9 */
    csd->DeviceSize |= csd_buffer[9] & 0xff;
	// These are not supported/required on v2 cards.
	csd->MaxRdCurrentVDDMin=0;
	csd->MaxRdCurrentVDDMax=0;
	csd->MaxWrCurrentVDDMin=0;
	csd->MaxWrCurrentVDDMax=0;
	csd->DeviceSizeMul= (csd_buffer[10]) << 4 ;
  }

  /* Byte 10 continued */
  csd->EraseGrSize = (csd_buffer[10] & 0x7C) >> 2;
  csd->EraseGrMul = (csd_buffer[10] & 0x03) << 3;
  /* Byte 11 */
  csd->EraseGrMul |= (csd_buffer[11] & 0xE0) >> 5;
  csd->WrProtectGrSize = (csd_buffer[11] & 0x1F);
  /* Byte 12 */
  csd->WrProtectGrEnable = (csd_buffer[12] & 0x80) >> 7;
  csd->ManDeflECC = (csd_buffer[12] & 0x60) >> 5;
  csd->WrSpeedFact = (csd_buffer[12] & 0x1C) >> 2;
  csd->MaxWrBlockLen = (csd_buffer[12] & 0x03) << 2;
  /* Byte 13 */
  csd->MaxWrBlockLen |= (csd_buffer[13] & 0xc0) >> 6;
  csd->WriteBlockPaPartial = (csd_buffer[13] & 0x20) >> 5;
  csd->Reserved3 = 0;
  csd->ContentProtectAppli = (csd_buffer[13] & 0x01);
  /* Byte 14 */
  csd->FileFormatGrouop = (csd_buffer[14] & 0x80) >> 7;
  csd->CopyFlag = (csd_buffer[14] & 0x40) >> 6;
  csd->PermWrProtect = (csd_buffer[14] & 0x20) >> 5;
  csd->TempWrProtect = (csd_buffer[14] & 0x10) >> 4;
  csd->FileFormat = (csd_buffer[14] & 0x0C) >> 2;
  csd->ECC = (csd_buffer[14] & 0x03);
  /* Byte 15 */
  csd->msd_CRC = (csd_buffer[15] & 0xFE) >> 1;
  csd->Reserved4 = 1;

error:
  // deactivate chip select
  MIOS32_SPI_RC_PinSet(MIOS32_SDCARD_SPI, MIOS32_SDCARD_SPI_RC_PIN, 1); // spi, rc_pin, pin_value
  // Send dummy byte once deactivated to drop cards DO
  MIOS32_SPI_TransferByte(MIOS32_SDCARD_SPI, 0xff);
  MIOS32_SDCARD_MUTEX_GIVE;

  return status;
}
