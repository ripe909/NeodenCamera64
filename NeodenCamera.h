#pragma once

#pragma once
//#include <CyAPI.h>
//#include <dbt.h>


__declspec (dllexport) int _cdecl img_capture(int which_camera, unsigned char* pFrameBuffer, long buffSize);

__declspec (dllexport) int _cdecl img_init();

__declspec (dllexport) BOOL _cdecl img_led(int which_camera, int16_t mode);

__declspec (dllexport) BOOL _cdecl img_set(int which_camera, int speed, int16_t exposure, int16_t gain);

__declspec (dllexport) BOOL _cdecl img_set_speed(int which_camera, int16_t speed);

__declspec (dllexport) BOOL _cdecl img_setRoi(int which_camera, short a2, short a3, short w, short h);

//__declspec (dllexport) BOOL _cdecl img_capture(int which_camera);

//__declspec (dllexport) int _cdecl img_read(int which_camera, unsigned char* pFrameBuffer, int BytesToRead, int ms);

//__declspec (dllexport) int _cdecl img_readAsy(int which_camera, unsigned char* pFrameBuffer, int BytesToRead, int ms);

//__declspec (dllexport) int _cdecl img_reset(int which_camera);

__declspec (dllexport) BOOL _cdecl img_set_exp(int which_camera, int16_t exposure);

__declspec (dllexport) BOOL _cdecl img_get_exp(int which_camera);

__declspec (dllexport) BOOL _cdecl img_set_gain(int which_camera, int16_t gain);

__declspec (dllexport) BOOL _cdecl img_get_gain(int which_camera);


__declspec (dllexport) BOOL _cdecl img_set_lt(int which_camera, int16_t a2, int16_t a3);

__declspec (dllexport) BOOL _cdecl img_set_wh(int which_camera, int16_t w, int16_t h);