//! File access layer for FatFS
//!
//! Frontend functions to read/write files.\n
//! Optimized for Memory Size and Speed!
//!
//! For the whole application only single file handlers for read and write
//! operations are available. They are shared globally to save memory (because
//! each FatFs handler allocates more than 512 bytes to store the last read
//! sector)
//!
//! For read operations it is possible to re-open a file via a file_t reference
//! so that no directory access is required to find the first sector of the
//! file (again).
//!
//! NOTE: before accessing the SD Card, the upper level function should
//! synchronize with a SD Card semaphore!
//! E.g. (defined in tasks.h in various projects):
//!   MUTEX_SDCARD_TAKE; // to take the semaphore
//!   MUTEX_SDCARD_GIVE; // to release the semaphore



#include <mios32.h>

#include <ff.h>
#include <diskio.h>
#include <string.h>
#include <ctype.h>

#include "file.h"

#ifndef MIOS32_FAMILY_EMULATION
# include <FreeRTOS.h>
# include <portmacro.h>
# include <task.h>
#endif


/////////////////////////////////////////////////////////////////////////////
// Global variables
/////////////////////////////////////////////////////////////////////////////

// last error status returned by DFS
// can be used as additional debugging help if FILE_*ERR returned by function
u32 file_dfs_errno;

// for percentage display during copy operations
u8 file_copy_percentage;


/////////////////////////////////////////////////////////////////////////////
// Local types
/////////////////////////////////////////////////////////////////////////////

// from https://www.gnu.org/software/tar/manual/html_node/Standard.html
typedef struct
{                              /* byte offset */
  char name[100];               /*   0 */
  char mode[8];                 /* 100 */
  char uid[8];                  /* 108 */
  char gid[8];                  /* 116 */
  char size[12];                /* 124 */
  char mtime[12];               /* 136 */
  char chksum[8];               /* 148 */
  char typeflag;                /* 156 */
  char linkname[100];           /* 157 */
  char magic[6];                /* 257 */
  char version[2];              /* 263 */
  char uname[32];               /* 265 */
  char gname[32];               /* 297 */
  char devmajor[8];             /* 329 */
  char devminor[8];             /* 337 */
  char prefix[155];             /* 345 */
  char fill_to_512[12];         /* 500 */
                                /* 512 */
} tar_posix_header;


/////////////////////////////////////////////////////////////////////////////
// Local prototypes
/////////////////////////////////////////////////////////////////////////////

static s32 FILE_MountFS(void);


/////////////////////////////////////////////////////////////////////////////
// Local variables
/////////////////////////////////////////////////////////////////////////////

// FatFs variables
#define SECTOR_SIZE _MAX_SS

// Work area (file system object) for logical drives
static FATFS fs;

// disk label
static char disk_label[12];

// complete file structure for read/write accesses
static FIL file_read;
static u8 file_read_is_open; // only for safety purposes
static FIL file_write;
static u8 file_write_is_open; // only for safety purposes

// SD Card status
static u8 sdcard_available;
static u8 volume_available;
static u32 volume_free_bytes;

static u8 status_msg_ctr;

// buffer for copy operations and SysEx sender
#define TMP_BUFFER_SIZE _MAX_SS
static u8 tmp_buffer[TMP_BUFFER_SIZE];



static s32 (*browser_upload_callback_func)(char *filename);


/////////////////////////////////////////////////////////////////////////////
//! Initialisation
/////////////////////////////////////////////////////////////////////////////
s32 FILE_Init() {

  file_read_is_open = 0;
  file_write_is_open = 0;
  sdcard_available = 0;
  volume_available = 0;
  volume_free_bytes = 0;

  browser_upload_callback_func = NULL;

  // init SDCard access
  s32 error = MIOS32_SDCARD_Init(0);

  //MIOS32_SDCARD_PowerOn();	// 202 addet

  // for status message
  status_msg_ctr = 5;

  return error;
}


/////////////////////////////////////////////////////////////////////////////
//! This function should be called periodically to check the availability
//! of the SD Card
//! \return < 0 on errors (error codes are documented in file.h)
//! \return 1 if SD card has been connected
//! \return 2 if SD card has been disconnected
//! \return 3 if new files should be loaded, e.g. after startup or SD Card change
/////////////////////////////////////////////////////////////////////////////
s32 FILE_CheckSDCard(void) {

  // check if SD card is available
  // High-speed access if SD card was previously available
  u8 prev_sdcard_available = sdcard_available;
  sdcard_available = MIOS32_SDCARD_CheckAvailable(prev_sdcard_available);


  if( sdcard_available && !prev_sdcard_available ) {

												    s32 error = FILE_MountFS();


												    if( error < 0 ) {
												      // ensure that volume flagged as not available
												      volume_available = 0;

												      return error; // break here!
												    }

												    // status message after 3 seconds
												    status_msg_ctr = 3;

												    return 1; // SD card has been connected
													}

  else if( !sdcard_available && prev_sdcard_available ) {

												    volume_available = 0;

												    return 2; // SD card has been disconnected
													}

  if( status_msg_ctr ) {
					    if( !--status_msg_ctr )
					      return 3;
						}

  return 0; // no error
}








/////////////////////////////////////////////////////////////////////////////
//! Mount the file system
//! \return < 0 on errors (error codes are documented in file.h)
/////////////////////////////////////////////////////////////////////////////
static s32 FILE_MountFS(void) {

  FRESULT res;
  DIR dir;

  file_read_is_open = 0;
  file_write_is_open = 0;

  if( (res=f_mount(0, &fs)) != FR_OK ) {	return -1; } // error


  if( (res=f_opendir(&dir, "/")) != FR_OK ) {	return -2; } // error


  // TODO: read from master sector
  disk_label[0] = 0;

  volume_available = 1;

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! This function updates the number of free bytes by scanning the FAT for
//! unused clusters.\n
//! should be called before FILE_Volume* bytes are read
/////////////////////////////////////////////////////////////////////////////
s32 FILE_UpdateFreeBytes(void){

  FRESULT res;
  DIR dir;
  DWORD free_clust;

  if( (res=f_opendir(&dir, "/")) != FR_OK ) {

    return FILE_ERR_UPDATE_FREE;
  }

  if( (res=f_getfree("/", &free_clust, &dir.fs)) != FR_OK ) {

    return FILE_ERR_UPDATE_FREE;
  }

  volume_free_bytes = free_clust * fs.csize * 512;

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! \return number of sectors per cluster
/////////////////////////////////////////////////////////////////////////////
u32 FILE_VolumeSectorsPerCluster(void){	return fs.csize;	}


/////////////////////////////////////////////////////////////////////////////
//! \return the physical sector of the given cluster
/////////////////////////////////////////////////////////////////////////////
u32 FILE_VolumeCluster2Sector(u32 cluster) {

  // from ff.c
  cluster -= 2;
  if( cluster >= (fs.max_clust - 2) )
    return 0;              /* Invalid cluster# */

  return cluster * fs.csize + fs.database;
}


/////////////////////////////////////////////////////////////////////////////
//! \return 1 if SD card available
/////////////////////////////////////////////////////////////////////////////
s32 FILE_SDCardAvailable(void) { return sdcard_available; }


/////////////////////////////////////////////////////////////////////////////
//! \return 1 if volume available
/////////////////////////////////////////////////////////////////////////////
s32 FILE_VolumeAvailable(void) {	return volume_available; }


/////////////////////////////////////////////////////////////////////////////
//! \return number of available bytes
/////////////////////////////////////////////////////////////////////////////
u32 FILE_VolumeBytesFree(void) {	return volume_free_bytes;	}


/////////////////////////////////////////////////////////////////////////////
//! \return total number of available bytes
/////////////////////////////////////////////////////////////////////////////
u32 FILE_VolumeBytesTotal(void){	return (fs.max_clust-2)*fs.csize * 512;	}


/////////////////////////////////////////////////////////////////////////////
//! \return volume label
/////////////////////////////////////////////////////////////////////////////
char *FILE_VolumeLabel(void) {	return disk_label; }


/*	ORGINAL
s32 FILE_ReadOpen(file_t* file, char *filepath) {


  if( file_read_is_open ) {	return FILE_ERR_OPEN_READ_WITHOUT_CLOSE;	}

  if( (file_dfs_errno=f_open(&file_read, filepath, FA_OPEN_EXISTING | FA_READ)) != FR_OK ) {

							    s32 error;
							    if( (error = FILE_MountFS()) < 0 ) {	return FILE_ERR_SD_CARD;	}

							    if( (file_dfs_errno=f_open(&file_read, filepath, FA_OPEN_EXISTING | FA_READ)) != FR_OK ) {	return FILE_ERR_OPEN_READ; }
							  }


  // store current file variables in file_t
  file->flag = file_read.flag;
  file->csect = file_read.csect;
  file->fptr = file_read.fptr;
  file->fsize = file_read.fsize;
  file->org_clust = file_read.org_clust;
  file->curr_clust = file_read.curr_clust;
  file->dsect = file_read.dsect;
  file->dir_sect = file_read.dir_sect;
  file->dir_ptr = file_read.dir_ptr;

  // file is opened
  file_read_is_open = 1;

  return 0;
}
*/

/////////////////////////////////////////////////////////////////////////////
//! Öffnet eine Datei zum Lesen
//! \param filepath Der Dateipfad zur zu öffnenden Datei
//! \return < 0 bei Fehlern (Fehlercodes sind in file.h dokumentiert)
/////////////////////////////////////////////////////////////////////////////
s32 FILE_ReadOpen(char *filepath) {

	// Fehler, wenn bereits eine Leseoperation geöffnet ist
	if (file_read_is_open) { return FILE_ERR_OPEN_READ_WITHOUT_CLOSE; }


	// Öffnen der Datei zum Lesen
	if ((file_dfs_errno = f_open(&file_read, filepath, FA_OPEN_EXISTING | FA_READ)) != FR_OK) {

			s32 error;
			if ((error = FILE_MountFS()) < 0) { return FILE_ERR_SD_CARD; } // Fehler beim Einbinden der SD-Karte


			if ((file_dfs_errno = f_open(&file_read, filepath, FA_OPEN_EXISTING | FA_READ)) != FR_OK) { return FILE_ERR_OPEN_READ; }// Fehler beim Öffnen der Datei

			}

	// Den Zustand speichern, dass eine Leseoperation geöffnet ist
	file_read_is_open = 1;

	return 0; // Kein Fehler
}



/*	ORGINAL
s32 FILE_ReadClose(file_t *file) {
  // store current file variables in file_t
  file->flag = file_read.flag;
  file->csect = file_read.csect;
  file->fptr = file_read.fptr;
  file->fsize = file_read.fsize;
  file->org_clust = file_read.org_clust;
  file->curr_clust = file_read.curr_clust;
  file->dsect = file_read.dsect;
  file->dir_sect = file_read.dir_sect;
  file->dir_ptr = file_read.dir_ptr;

  // file has been closed
  file_read_is_open = 0;


  // don't close file via f_close()! We allow to open the file again
  return 0;
}
*/

/////////////////////////////////////////////////////////////////////////////
//! Closes a file which has been read
//! File can be re-opened if required thereafter w/o performance issues
//! \return < 0 on errors (error codes are documented in file.h)
/////////////////////////////////////////////////////////////////////////////
s32 FILE_ReadClose() {

  // Die Datei wurde geschlossen
  file_read_is_open = 0;

  return 0; // Kein Fehler
}





/////////////////////////////////////////////////////////////////////////////
//! Changes to a new file position
//! \return < 0 on errors (error codes are documented in file.h)
/////////////////////////////////////////////////////////////////////////////
s32 FILE_ReadSeek(u32 offset) {

  if( (file_dfs_errno=f_lseek(&file_read, offset)) != FR_OK ) {	return FILE_ERR_SEEK;	}
  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! Returns current size of write file
/////////////////////////////////////////////////////////////////////////////
u32 FILE_ReadGetCurrentSize(void) {	return file_read.fsize;	}


/////////////////////////////////////////////////////////////////////////////
//! Returns current file pointer of the write file
/////////////////////////////////////////////////////////////////////////////
u32 FILE_ReadGetCurrentPosition(void) {  return file_read.fptr;	}



/////////////////////////////////////////////////////////////////////////////
//! Read from file
//! \return < 0 on errors (error codes are documented in file.h)
/////////////////////////////////////////////////////////////////////////////
s32 FILE_ReadBuffer(u8 *buffer, u32 len) {

  UINT successcount;

  // exit if volume not available
  if( !volume_available )
    return FILE_ERR_NO_VOLUME;

  if( (file_dfs_errno=f_read(&file_read, buffer, len, &successcount)) != FR_OK ) {	return FILE_ERR_READ;	}

  if( successcount != len ) {	return FILE_ERR_READCOUNT;	}

  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! Read from file with unknown size
//! \return < 0 on errors (error codes are documented in file.h)
//! \return >= 0: value contains the actual read bytes
/////////////////////////////////////////////////////////////////////////////
s32 FILE_ReadBufferUnknownLen(u8 *buffer, u32 len) {

  UINT successcount;

  // exit if volume not available
  if( !volume_available )
    return FILE_ERR_NO_VOLUME;

  if( (file_dfs_errno=f_read(&file_read, buffer, len, &successcount)) != FR_OK ) {	return FILE_ERR_READ;	}

  return successcount;
}


/////////////////////////////////////////////////////////////////////////////
//! Read a string (terminated with CR) from file
//! \return < 0 on errors (error codes are documented in file.h)
/////////////////////////////////////////////////////////////////////////////
s32 FILE_ReadLine(u8 *buffer, u32 max_len)	{

  s32 status;
  u32 num_read = 0;

  while( file_read.fptr < file_read.fsize ) {
										    status = FILE_ReadBuffer(buffer, 1);

										    if( status < 0 )
										      return status;

										    ++num_read;

										    if( *buffer == '\n' || *buffer == '\r' )
										      break;

										    if( num_read < max_len )
										      ++buffer;
											}

  // replace newline by terminator
  *buffer = 0;

  return num_read;
}

/////////////////////////////////////////////////////////////////////////////
//! Read a 8bit value from file
//! \return < 0 on errors (error codes are documented in file.h)
/////////////////////////////////////////////////////////////////////////////
s32 FILE_ReadByte(u8 *byte) {	return FILE_ReadBuffer(byte, 1);	}

/////////////////////////////////////////////////////////////////////////////
//! Read a 16bit value from file
//! \return < 0 on errors (error codes are documented in file.h)
/////////////////////////////////////////////////////////////////////////////
s32 FILE_ReadHWord(u16 *hword) {

  // ensure little endian coding
  u8 tmp[2];
  s32 status = FILE_ReadBuffer(tmp, 2);
  *hword = ((u16)tmp[0] << 0) | ((u16)tmp[1] << 8);
  return status;
}

/////////////////////////////////////////////////////////////////////////////
//! Read a 32bit value from file
//! \return < 0 on errors (error codes are documented in file.h)
/////////////////////////////////////////////////////////////////////////////
s32 FILE_ReadWord(u32 *word) {

  // ensure little endian coding
  u8 tmp[4];
  s32 status = FILE_ReadBuffer(tmp, 4);
  *word = ((u32)tmp[0] << 0) | ((u32)tmp[1] << 8) | ((u32)tmp[2] << 16) | ((u32)tmp[3] << 24);
  return status;
}




/* ORGINAL
s32 FILE_WriteOpen(char *filepath, u8 create) {

  if( file_write_is_open ) {	return FILE_ERR_OPEN_WRITE_WITHOUT_CLOSE;	}

  if( (file_dfs_errno=f_open(&file_write, filepath, (create ? FA_CREATE_ALWAYS : FA_OPEN_EXISTING) | FA_WRITE)) != FR_OK ) {	return FILE_ERR_OPEN_WRITE;  }

  // remember state
  file_write_is_open = 1;

  return 0; // no error
}
*/


/////////////////////////////////////////////////////////////////////////////
//! Öffnet eine Datei zum Schreiben
//! \param filepath Der Dateipfad zur zu öffnenden Datei
//! \return < 0 bei Fehlern (Fehlercodes sind in file.h dokumentiert)
/////////////////////////////////////////////////////////////////////////////
s32 FILE_WriteOpen(char *filepath) {

  if (file_write_is_open) {	return FILE_ERR_OPEN_WRITE_WITHOUT_CLOSE; } // Fehler, wenn bereits eine Schreiboperation geöffnet ist


  // Öffnen der Datei zum Schreiben
  if ((file_dfs_errno = f_open(&file_write, filepath, FA_CREATE_ALWAYS | FA_WRITE)) != FR_OK) {	return FILE_ERR_OPEN_WRITE; }// Fehler beim Öffnen der Datei


  file_write_is_open = 1; // Den Zustand speichern, dass eine Schreiboperation geöffnet ist

  return 0; // Kein Fehler
}





/////////////////////////////////////////////////////////////////////////////
//! Closes a file by writing the last bytes
//! \return < 0 on errors (error codes are documented in file.h)
/////////////////////////////////////////////////////////////////////////////
s32 FILE_WriteClose(void) {

  s32 status = 0;

  // close file
  if( (file_dfs_errno=f_close(&file_write)) != FR_OK )
    status = FILE_ERR_WRITECLOSE;

  file_write_is_open = 0;

  return status;
}



/////////////////////////////////////////////////////////////////////////////
//! Changes to a new file position
//! \return < 0 on errors (error codes are documented in file.h)
/////////////////////////////////////////////////////////////////////////////
s32 FILE_WriteSeek(u32 offset) {

  if( (file_dfs_errno=f_lseek(&file_write, offset)) != FR_OK ) {

    return FILE_ERR_SEEK;
  }
  return 0; // no error
}


/////////////////////////////////////////////////////////////////////////////
//! Returns current size of write file
/////////////////////////////////////////////////////////////////////////////
u32 FILE_WriteGetCurrentSize(void)
{
  if( !file_write_is_open )
    return 0;
  return file_write.fsize;
}

/////////////////////////////////////////////////////////////////////////////
//! Returns current file pointer of the write file
/////////////////////////////////////////////////////////////////////////////
u32 FILE_WriteGetCurrentPosition(void)
{
  if( !file_write_is_open )
    return 0;
  return file_write.fptr;
}


/////////////////////////////////////////////////////////////////////////////
//! Writes into a file with caching mechanism (actual write at end of sector)
//! File has to be closed via FILE_WriteClose() after the last byte has
//! been written
//! \return < 0 on errors (error codes are documented in file.h)
/////////////////////////////////////////////////////////////////////////////
s32 FILE_WriteBuffer(u8 *buffer, u32 len) {

  // exit if volume not available
  if( !volume_available )
    return FILE_ERR_NO_VOLUME;

  UINT successcount;
  if( (file_dfs_errno=f_write(&file_write, buffer, len, &successcount)) != FR_OK ) { return FILE_ERR_WRITE;	}

  if( successcount != len ) {	return FILE_ERR_WRITECOUNT;	}

  return 0; // no error
}

/////////////////////////////////////////////////////////////////////////////
//! Writes a 8bit value into file
//! \return < 0 on errors (error codes are documented in file.h)
/////////////////////////////////////////////////////////////////////////////
s32 FILE_WriteByte(u8 byte) {	return FILE_WriteBuffer(&byte, 1);	}

/////////////////////////////////////////////////////////////////////////////
//! Writes a 16bit value into file
//! \return < 0 on errors (error codes are documented in file.h)
/////////////////////////////////////////////////////////////////////////////
s32 FILE_WriteHWord(u16 hword) {

  // ensure little endian coding
  u8 tmp[2];
  tmp[0] = (u8)(hword >> 0);
  tmp[1] = (u8)(hword >> 8);
  return FILE_WriteBuffer(tmp, 2);
}


/////////////////////////////////////////////////////////////////////////////
//! Writes a 32bit value into file
//! \return < 0 on errors (error codes are documented in file.h)
/////////////////////////////////////////////////////////////////////////////
s32 FILE_WriteWord(u32 word) {

  // ensure little endian coding
  u8 tmp[4];
  tmp[0] = (u8)(word >> 0);
  tmp[1] = (u8)(word >> 8);
  tmp[2] = (u8)(word >> 16);
  tmp[3] = (u8)(word >> 24);
  return FILE_WriteBuffer(tmp, 4);
}


/////////////////////////////////////////////////////////////////////////////
//! This function copies a file
//! \param[in] src_file the source file which should be copied
//! \param[in] dst_file the destination file
/////////////////////////////////////////////////////////////////////////////
s32 FILE_Copy(char *src_file, char *dst_file) {

  s32 status = 0;

  if( !volume_available ) { return FILE_ERR_NO_VOLUME; }



  if( (file_dfs_errno=f_open(&file_read, src_file, FA_OPEN_EXISTING | FA_READ)) != FR_OK ) {

    status = FILE_ERR_COPY_NO_FILE;
  } else {
    if( (file_dfs_errno=f_open(&file_write, dst_file, FA_CREATE_ALWAYS | FA_WRITE)) != FR_OK ) {

      status = FILE_ERR_COPY;
      //f_close(&file_read); // never close read files to avoid "invalid object"
    }
  }

  if( status < 0 ) {

  } else {
    // copy operation


    file_copy_percentage = 0; // for percentage display

    UINT successcount;
    UINT successcount_wr;
    u32 num_bytes = 0;
    do {
      if( (file_dfs_errno=f_read(&file_read, tmp_buffer, TMP_BUFFER_SIZE, &successcount)) != FR_OK ) {

	successcount = 0;
	status = FILE_ERR_READ;
      } else if( successcount && (file_dfs_errno=f_write(&file_write, tmp_buffer, successcount, &successcount_wr)) != FR_OK ) {

	status = FILE_ERR_WRITE;
      } else {
	num_bytes += successcount_wr;
	file_copy_percentage = (u8)((100 * num_bytes) / file_read.fsize);
      }
    } while( status == 0 && successcount > 0 );



    //f_close(&file_read); // never close read files to avoid "invalid object"
    f_close(&file_write);
  }

  return status;
}


/////////////////////////////////////////////////////////////////////////////
//! Creates a directory
/////////////////////////////////////////////////////////////////////////////
s32 FILE_MakeDir(char *path) {


  // exit if volume not available
  if( !volume_available ) {	return FILE_ERR_NO_VOLUME; }

  if( (file_dfs_errno=f_mkdir(path)) != FR_OK )
    return FILE_ERR_MKDIR;

  return 0; // directory created
}



/////////////////////////////////////////////////////////////////////////////
//! Returns 1 if file exists, 0 if it doesn't exist, < 0 on errors
/////////////////////////////////////////////////////////////////////////////
s32 FILE_FileExists(char *filepath) {

  // exit if volume not available
  if( !volume_available ) {	return FILE_ERR_NO_VOLUME;	}

  if( !filepath || !filepath[0] )
    return 0; // empty file name - handle like if it doesn't exist

  if( f_open(&file_read, filepath, FA_OPEN_EXISTING | FA_READ) != FR_OK )
    return 0; // file doesn't exist
  //f_close(&file_read); // never close read files to avoid "invalid object"
  return 1; // file exists
}
