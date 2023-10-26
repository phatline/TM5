#ifndef _AINSER_H
#define _AINSER_H

#ifdef __cplusplus
extern "C" {
#endif

extern s32 AINSER_Init(u32 mode);

extern s32 AINSER_Handler(void (*_callback)(u32 module, u32 pin, u32 value));

#ifdef __cplusplus
}
#endif

#endif /* _AINSER_H */
