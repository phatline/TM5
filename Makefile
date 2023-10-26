# MakeFile
# following setup taken from environment variables
PROCESSOR =	$(MIOS32_PROCESSOR)
FAMILY    = 	$(MIOS32_FAMILY)
BOARD	  = 	$(MIOS32_BOARD)
LCD       =     $(MIOS32_LCD)


THUMB_SOURCE    = 	app.c \
					seq.c \
					app_lcd.c \
					fnt_BIG.c \
					fnt_SYM.c \
					fnt_SMALL.c \
					fnt_TINY.c \
					blm_scalar.c\
					seq_bpm.c \
					file.c\
					mios32_sdcard.c\
					jsw_rand.c\
					mios32_ain.c\
					ainser.c\
					$(MIOS32_PATH)/modules/glcd_font/glcd_font_normal.c \
					$(MIOS32_PATH)/modules/glcd_font/glcd_font_normal_inv.c \
					$(MIOS32_PATH)/modules/glcd_font/glcd_font_big.c


# (following source stubs not relevant for Cortex M3 derivatives)
THUMB_AS_SOURCE =
ARM_SOURCE      =
ARM_AS_SOURCE   =
C_INCLUDE = 	-I .
A_INCLUDE = 	-I .
LIBS =

# Remaining variables
LD_FILE   = 	$(MIOS32_PATH)/etc/ld/$(FAMILY)/$(PROCESSOR).ld
PROJECT   = 	project
DEBUG     =	-g
OPTIMIZE  =	-Os
CFLAGS =	$(DEBUG) $(OPTIMIZE)

include $(MIOS32_PATH)/programming_models/traditional/programming_model.mk
#include $(MIOS32_PATH)/modules/ainser/ainser.mk
include $(MIOS32_PATH)/modules/fatfs/fatfs.mk 			# FATFS Driver
include $(MIOS32_PATH)/include/makefile/common.mk		# Please keep this include statement at the end of this Makefile. Add new modules above.
