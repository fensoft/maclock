#ifndef _ARDUINOAPI_H_
#define _ARDUINOAPI_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* ArduinoFile;

enum {
    Arduino_Seek_Set = 0,
    Arduino_Seek_Cur,
    Arduino_Seek_End
};

void ArduinoAPI_GetDisplayDimensions( int* OutWidthPtr, int* OutHeightPtr );
void ArduinoAPI_SetAddressWindow( int x0, int y0, int x1, int y1 );
void ArduinoAPI_WritePixels( const uint16_t* Pixels, size_t Count );

void ArduinoAPI_GetMouseDelta( int* OutXDeltaPtr, int* OutYDeltaPtr );
void ArduinoAPI_GiveEmulatedMouseToArduino( int* EmMouseX, int* EmMouseY );
int ArduinoAPI_GetMouseButton( void );

uint64_t ArduinoAPI_GetTimeMS( void );
void ArduinoAPI_Yield( void );
void ArduinoAPI_Delay( uint32_t MSToDelay );

ArduinoFile ArduinoAPI_open( const char* Path, const char* Mode );
void ArduinoAPI_close( ArduinoFile Handle );
size_t ArduinoAPI_read( void* Buffer, size_t Size, size_t Nmemb, ArduinoFile Handle );
size_t ArduinoAPI_write( const void* Buffer, size_t Size, size_t Nmemb, ArduinoFile Handle );
long ArduinoAPI_tell( ArduinoFile Handle );
long ArduinoAPI_seek( ArduinoFile Handle, long Offset, int Whence );
int ArduinoAPI_eof( ArduinoFile Handle );

void* ArduinoAPI_malloc( size_t Size );
void* ArduinoAPI_calloc( size_t Nmemb, size_t Size );
void ArduinoAPI_free( void* Memory );

void ArduinoAPI_CheckForEvents( void );

void ArduinoAPI_ScreenChanged( int Top, int Left, int Bottom, int Right );
void ArduinoAPI_DrawScreen( const uint8_t* Screen );
void ArduinoAPI_GiveScreenBufferToArduino( const uint8_t* ScreenPtr );

int minivmac_main( int Argc, char** Argv );

#ifdef __cplusplus
}
#endif

#endif
