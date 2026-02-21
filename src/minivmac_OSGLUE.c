/*
	OSGLUSDL.c

	Copyright (C) 2012 Paul C. Pratt, Manuel Alfayate

	You can redistribute this file and/or modify it under the terms
	of version 2 of the GNU General Public License as published by
	the Free Software Foundation.  You should have received a copy
	of the license along with this file; see the file COPYING.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	license for more details.
*/

/*
	Operating System GLUe for SDL (1.2 and 2.0) library

	All operating system dependent code for the
	SDL Library should go here.

	This is also the "reference" implementation. General
	comments about what the platform dependent code
	does should go here, and not be repeated for each
	platform. Such comments are labeled with "OSGLUxxx common".

	The SDL port can be used to create a more native port. Once
	the SDL port runs on a new platform, the source code for
	Mini vMac and SDL can be merged together. Then any SDL code
	not used for this platform is removed, then a lot of clean
	up is done step by step to remove the rest of the SDL code.
	This technique is particular useful if you are not very
	familiar with the new platform. It is long but straightforward,
	and you can learn about the platform as you go. The Cocoa
	port was created this way, with no previous knowledge of
	Cocoa or Objective-C.

	The main entry point 'main' is at the end of this file.
*/

#include "CNFGRAPI.h"
#include "SYSDEPNS.h"
#include "ENDIANAC.h"

#include "MYOSGLUE.h"

#include "STRCONST.h"

/* --- some simple utilities --- */

GLOBALOSGLUPROC MyMoveBytes(anyp srcPtr, anyp destPtr, si5b byteCount)
{
	(void) memcpy((char *)destPtr, (char *)srcPtr, byteCount);
}

/* --- control mode and internationalization --- */

#define NeedCell2PlainAsciiMap 1

#define dbglog_OSGInit (0 && dbglog_HAVE)

#include "INTLCHAR.h"

#ifndef SDL_MAJOR_VERSION
#define SDL_MAJOR_VERSION 0
#endif

LOCALVAR char *d_arg = NULL;

#ifdef _WIN32
#define MyPathSep '\\'
#else
#define MyPathSep '/'
#endif

LOCALFUNC tMacErr ChildPath(char *x, char *y, char **r)
{
	tMacErr err = mnvm_miscErr;
	int nx = strlen(x);
	int ny = strlen(y);
	{
		if ((nx > 0) && (MyPathSep == x[nx - 1])) {
			--nx;
		}
		{
			int nr = nx + 1 + ny;
			char *p = ArduinoAPI_malloc(nr + 1);
			if (p != NULL) {
				char *p2 = p;
				(void) memcpy(p2, x, nx);
				p2 += nx;
				*p2++ = MyPathSep;
				(void) memcpy(p2, y, ny);
				p2 += ny;
				*p2 = 0;
				*r = p;
				err = mnvm_noErr;
			}
		}
	}

	return err;
}

LOCALPROC MyMayFree(char *p)
{
	if (NULL != p) {
		ArduinoAPI_free(p);
	}
}

/* --- sending debugging info to file --- */

#if dbglog_HAVE

#ifndef dbglog_ToStdErr
#define dbglog_ToStdErr 1
#endif
#ifndef dbglog_ToSDL_Log
#define dbglog_ToSDL_Log 0
#endif

#if ! dbglog_ToStdErr
LOCALVAR FILE *dbglog_File = NULL;
#endif

LOCALFUNC blnr dbglog_open0(void)
{
#if dbglog_ToStdErr || dbglog_ToSDL_Log
	return trueblnr;
#else
#if CanGetAppPath
	if (NULL == app_parent)
#endif
	{
		dbglog_File = fopen("dbglog.txt", "w");
	}
#if CanGetAppPath
	else {
		char *t = NULL;

		if (mnvm_noErr == ChildPath(app_parent, "dbglog.txt", &t)) {
			dbglog_File = fopen(t, "w");
		}

		MyMayFree(t);
	}
#endif

	return (NULL != dbglog_File);
#endif
}

LOCALPROC dbglog_write0(char *s, uimr L)
{
#if dbglog_ToStdErr
	(void) fwrite(s, 1, L, stderr);
#elif dbglog_ToSDL_Log
	char t[256 + 1];

	if (L > 256) {
		L = 256;
	}
	(void) memcpy(t, s, L);
	t[L] = 1;

	SDL_Log("%s", t);
#else
	if (dbglog_File != NULL) {
		(void) fwrite(s, 1, L, dbglog_File);
	}
#endif
}

LOCALPROC dbglog_close0(void)
{
#if ! dbglog_ToStdErr
	if (dbglog_File != NULL) {
		fclose(dbglog_File);
		dbglog_File = NULL;
	}
#endif
}

#endif

/* --- information about the environment --- */

#define WantColorTransValid 0

#include "COMOSGLU.h"

#include "PBUFSTDC.h"

#include "CONTROLM.h"

/* --- text translation --- */

LOCALPROC NativeStrFromCStr(char *r, char *s)
{
	ui3b ps[ClStrMaxLength];
	int i;
	int L;

	ClStrFromSubstCStr(&L, ps, s);

	for (i = 0; i < L; ++i) {
		r[i] = Cell2PlainAsciiMap[ps[i]];
	}

	r[L] = 0;
}

/* --- drives --- */

/*
	OSGLUxxx common:
	define NotAfileRef to some value that is different
	from any valid open file reference.
*/
#define NotAfileRef NULL

#define MyFilePtr ArduinoFile
#define MySeek ArduinoAPI_seek
#define MySeekSet Arduino_Seek_Set
#define MySeekCur Arduino_Seek_Cur
#define MySeekEnd Arduino_Seek_End
#define MyFileRead ArduinoAPI_read
#define MyFileWrite ArduinoAPI_write
#define MyFileTell ArduinoAPI_tell
#define MyFileClose ArduinoAPI_close
#define MyFileOpen ArduinoAPI_open
#define MyFileEof ArduinoAPI_eof

LOCALVAR MyFilePtr Drives[NumDrives]; /* open disk image files */

LOCALPROC InitDrives(void)
{
	/*
		This isn't really needed, Drives[i] and DriveNames[i]
		need not have valid values when not vSonyIsInserted[i].
	*/
	tDrive i;

	for (i = 0; i < NumDrives; ++i) {
		Drives[i] = NotAfileRef;
	}
}

GLOBALOSGLUFUNC tMacErr vSonyTransfer(blnr IsWrite, ui3p Buffer,
	tDrive Drive_No, ui5r Sony_Start, ui5r Sony_Count,
	ui5r *Sony_ActCount)
{
	/*
		OSGLUxxx common:
		return 0 if it succeeds, nonzero (a
		Macintosh style error code, but -1
		will do) on failure.
	*/
	tMacErr err = mnvm_miscErr;
	MyFilePtr refnum = Drives[Drive_No];
	ui5r NewSony_Count = 0;

	if (MySeek(refnum, Sony_Start, MySeekSet) >= 0) {
		if (IsWrite) {
			NewSony_Count = MyFileWrite(Buffer, 1, Sony_Count, refnum);
		} else {
			NewSony_Count = MyFileRead(Buffer, 1, Sony_Count, refnum);
		}

		if (NewSony_Count == Sony_Count) {
			err = mnvm_noErr;
		}
	}

	if (nullpr != Sony_ActCount) {
		*Sony_ActCount = NewSony_Count;
	}

	return err; /*& figure out what really to return &*/
}

GLOBALOSGLUFUNC tMacErr vSonyGetSize(tDrive Drive_No, ui5r *Sony_Count)
{
	/*
		OSGLUxxx common:
		set Sony_Count to the size of disk image number Drive_No.

		return 0 if it succeeds, nonzero (a
		Macintosh style error code, but -1
		will do) on failure.
	*/
	tMacErr err = mnvm_miscErr;
	MyFilePtr refnum = Drives[Drive_No];
	long v;

	if (MySeek(refnum, 0, MySeekEnd) >= 0) {
		v = MyFileTell(refnum);
		if (v >= 0) {
			*Sony_Count = v;
			err = mnvm_noErr;
		}
	}

	return err; /*& figure out what really to return &*/
}

LOCALFUNC tMacErr vSonyEject0(tDrive Drive_No, blnr deleteit)
{
	/*
		OSGLUxxx common:
		close disk image number Drive_No.

		return 0 if it succeeds, nonzero (a
		Macintosh style error code, but -1
		will do) on failure.
	*/
	MyFilePtr refnum = Drives[Drive_No];

	DiskEjectedNotify(Drive_No);

	MyFileClose(refnum);
	Drives[Drive_No] = NotAfileRef; /* not really needed */

	return mnvm_noErr;
}

GLOBALOSGLUFUNC tMacErr vSonyEject(tDrive Drive_No)
{
	return vSonyEject0(Drive_No, falseblnr);
}

LOCALPROC UnInitDrives(void)
{
	tDrive i;

	for (i = 0; i < NumDrives; ++i) {
		if (vSonyIsInserted(i)) {
			(void) vSonyEject(i);
		}
	}
}

LOCALFUNC blnr Sony_Insert0(MyFilePtr refnum, blnr locked,
	char *drivepath)
{
	/*
		OSGLUxxx common:
		Given reference to open file, mount it as a disk image file.
		if "locked", then mount it as a locked disk.
	*/

	tDrive Drive_No;
	blnr IsOk = falseblnr;

	if (! FirstFreeDisk(&Drive_No)) {
		MacMsg(kStrTooManyImagesTitle, kStrTooManyImagesMessage,
			falseblnr);
	} else {
		/* printf("Sony_Insert0 %d\n", (int)Drive_No); */

		{
			Drives[Drive_No] = refnum;
			DiskInsertNotify(Drive_No, locked);

			IsOk = trueblnr;
		}
	}

	if (! IsOk) {
		MyFileClose(refnum);
	}

	return IsOk;
}

LOCALFUNC blnr Sony_Insert1(char *drivepath, blnr silentfail)
{
	blnr locked = falseblnr;
	/* printf("Sony_Insert1 %s\n", drivepath); */
	MyFilePtr refnum = MyFileOpen(drivepath, "rb+");
	if (NULL == refnum) {
		locked = trueblnr;
		refnum = MyFileOpen(drivepath, "rb");
	}
	if (NULL == refnum) {
		if (! silentfail) {
			MacMsg(kStrOpenFailTitle, kStrOpenFailMessage, falseblnr);
		}
	} else {
		return Sony_Insert0(refnum, locked, drivepath);
	}
	return falseblnr;
}

LOCALFUNC tMacErr LoadMacRomFrom(char *path)
{
	tMacErr err;
	MyFilePtr ROM_File;
	int File_Size;

	ROM_File = MyFileOpen(path, "rb");
	if (NULL == ROM_File) {
		err = mnvm_fnfErr;
	} else {
		File_Size = MyFileRead(ROM, 1, kROM_Size, ROM_File);
		if (File_Size != kROM_Size) {
#ifdef MyFileEof
			if (MyFileEof(ROM_File))
#else
			if (File_Size > 0)
#endif
			{
				MacMsgOverride(kStrShortROMTitle,
					kStrShortROMMessage);
				err = mnvm_eofErr;
			} else {
				MacMsgOverride(kStrNoReadROMTitle,
					kStrNoReadROMMessage);
				err = mnvm_miscErr;
			}
		} else {
			err = ROM_IsValid();
		}
		MyFileClose(ROM_File);
	}

	return err;
}

LOCALFUNC blnr Sony_Insert2(char *s)
{
	char *d = d_arg;
	blnr IsOk = falseblnr;

	if (NULL == d) {
		IsOk = Sony_Insert1(s, trueblnr);
	} else
	{
		char *t = NULL;

		if (mnvm_noErr == ChildPath(d, s, &t)) {
			IsOk = Sony_Insert1(t, trueblnr);
		}

		MyMayFree(t);
	}

	return IsOk;
}

LOCALFUNC blnr Sony_InsertIth(int i)
{
	blnr v;

	if ((i > 9) || ! FirstFreeDisk(nullpr)) {
		v = falseblnr;
	} else {
		char s[] = "disk?.dsk";

		s[4] = '0' + i;

		v = Sony_Insert2(s);
	}

	return v;
}

LOCALFUNC blnr LoadInitialImages(void)
{
	if (! AnyDiskInserted()) {
		int i;

		for (i = 1; Sony_InsertIth(i); ++i) {
			/* stop on first error (including file not found) */
		}
	}

	return trueblnr;
}

/* --- ROM --- */

LOCALVAR char *rom_path = NULL;

LOCALFUNC blnr LoadMacRom(void)
{
	tMacErr err;

	if ((NULL == rom_path)
		|| (mnvm_fnfErr == (err = LoadMacRomFrom(rom_path))))
	if (mnvm_fnfErr == (err = LoadMacRomFrom(RomFileName)))
	{
	}

	return trueblnr; /* keep launching Mini vMac, regardless */
}

/* --- video out --- */

LOCALVAR blnr gBackgroundFlag = falseblnr;
LOCALVAR blnr gTrueBackgroundFlag = falseblnr;
LOCALVAR blnr CurSpeedStopped = falseblnr;

LOCALPROC HaveChangedScreenBuff(ui4r top, ui4r left, ui4r bottom, ui4r right) {
	ArduinoAPI_ScreenChanged( top, left, bottom, right );
}

LOCALPROC MyDrawChangesAndClear(void)
{
	if (ScreenChangedBottom > ScreenChangedTop) {
		HaveChangedScreenBuff(ScreenChangedTop, ScreenChangedLeft,
			ScreenChangedBottom, ScreenChangedRight);
		ScreenClearChanges();
	}
}

GLOBALOSGLUPROC DoneWithDrawingForTick(void)
{
#if EnableFSMouseMotion
	if (HaveMouseMotion) {
		AutoScrollScreen();
	}
#endif
	MyDrawChangesAndClear();
}

/* --- mouse --- */

/* cursor hiding */

LOCALVAR blnr HaveCursorHidden = falseblnr;
LOCALVAR blnr WantCursorHidden = falseblnr;

LOCALPROC ForceShowCursor(void)
{
}

/* cursor moving */

/*
	OSGLUxxx common:
	When "EnableFSMouseMotion" the platform
	specific code can get relative mouse
	motion, instead of absolute coordinates
	on the emulated screen. It should
	set HaveMouseMotion to true when
	it is doing this (normally when in
	full screen mode.)

	This can usually be implemented by
	hiding the platform specific cursor,
	and then keeping it within a box,
	moving the cursor back to the center whenever
	it leaves the box. This requires the
	ability to move the cursor (in MyMoveMouse).
*/

#ifndef HaveWorkingWarp
#define HaveWorkingWarp 1
#endif

#if EnableMoveMouse && HaveWorkingWarp
LOCALFUNC blnr MyMoveMouse(si4b h, si4b v)
{
	/*
		OSGLUxxx common:
		Move the cursor to the point h, v on the emulated screen.
		If detect that this fails return falseblnr,
			otherwise return trueblnr.
		(On some platforms it is possible to move the curser,
			but there is no way to detect failure.)
	*/

	return trueblnr;
}
#endif

/* cursor state */

LOCALPROC MousePositionNotify(int NewMousePosh, int NewMousePosv)
{
	blnr ShouldHaveCursorHidden = trueblnr;

#if EnableFSMouseMotion
	if (HaveMouseMotion) {
		MyMousePositionSetDelta(NewMousePosh - SavedMouseH,
			NewMousePosv - SavedMouseV);
		SavedMouseH = NewMousePosh;
		SavedMouseV = NewMousePosv;
	} else
#endif
	{
		if (NewMousePosh < 0) {
			NewMousePosh = 0;
			ShouldHaveCursorHidden = falseblnr;
		} else if (NewMousePosh >= vMacScreenWidth) {
			NewMousePosh = vMacScreenWidth - 1;
			ShouldHaveCursorHidden = falseblnr;
		}
		if (NewMousePosv < 0) {
			NewMousePosv = 0;
			ShouldHaveCursorHidden = falseblnr;
		} else if (NewMousePosv >= vMacScreenHeight) {
			NewMousePosv = vMacScreenHeight - 1;
			ShouldHaveCursorHidden = falseblnr;
		}

		/* if (ShouldHaveCursorHidden || CurMouseButton) */
		/*
			for a game like arkanoid, would like mouse to still
			move even when outside window in one direction
		*/
		MyMousePositionSet(NewMousePosh, NewMousePosv);
	}

	WantCursorHidden = ShouldHaveCursorHidden;
}

LOCALPROC CheckMouseState(void)
{
	int MouseH = 0;
	int MouseV = 0;
	int dx = 0;
	int dy = 0;

	ArduinoAPI_GetMouseDelta( &dx, &dy );

	MyMousePositionSetDelta( dx, dy );
	MyMouseButtonSet( ArduinoAPI_GetMouseButton( ) );

	MouseH = CurMouseH;
	MouseV = CurMouseV;

	ArduinoAPI_GiveEmulatedMouseToArduino( &MouseH, &MouseV );
#if 0 != SDL_MAJOR_VERSION
	/*
		this doesn't work as desired, doesn't get mouse movements
		when outside of our window.
	*/
	int x;
	int y;

	(void) SDL_GetMouseState(&x, &y);
	MousePositionNotify(x, y);
#endif /* 0 != SDL_MAJOR_VERSION */
}

/* --- keyboard input --- */

#if 1 == SDL_MAJOR_VERSION
LOCALFUNC ui3r SDLKey2MacKeyCode(SDLKey i)
{
	ui3r v = MKC_None;

	switch (i) {
		case SDLK_BACKSPACE: v = MKC_BackSpace; break;
		case SDLK_TAB: v = MKC_Tab; break;
		case SDLK_CLEAR: v = MKC_Clear; break;
		case SDLK_RETURN: v = MKC_Return; break;
		case SDLK_PAUSE: v = MKC_Pause; break;
		case SDLK_ESCAPE: v = MKC_formac_Escape; break;
		case SDLK_SPACE: v = MKC_Space; break;
		case SDLK_EXCLAIM: /* ? */ break;
		case SDLK_QUOTEDBL: /* ? */ break;
		case SDLK_HASH: /* ? */ break;
		case SDLK_DOLLAR: /* ? */ break;
		case SDLK_AMPERSAND: /* ? */ break;
		case SDLK_QUOTE: v = MKC_SingleQuote; break;
		case SDLK_LEFTPAREN: /* ? */ break;
		case SDLK_RIGHTPAREN: /* ? */ break;
		case SDLK_ASTERISK: /* ? */ break;
		case SDLK_PLUS: /* ? */ break;
		case SDLK_COMMA: v = MKC_Comma; break;
		case SDLK_MINUS: v = MKC_Minus; break;
		case SDLK_PERIOD: v = MKC_Period; break;
		case SDLK_SLASH: v = MKC_formac_Slash; break;
		case SDLK_0: v = MKC_0; break;
		case SDLK_1: v = MKC_1; break;
		case SDLK_2: v = MKC_2; break;
		case SDLK_3: v = MKC_3; break;
		case SDLK_4: v = MKC_4; break;
		case SDLK_5: v = MKC_5; break;
		case SDLK_6: v = MKC_6; break;
		case SDLK_7: v = MKC_7; break;
		case SDLK_8: v = MKC_8; break;
		case SDLK_9: v = MKC_9; break;
		case SDLK_COLON: /* ? */ break;
		case SDLK_SEMICOLON: v = MKC_SemiColon; break;
		case SDLK_LESS: /* ? */ break;
		case SDLK_EQUALS: v = MKC_Equal; break;
		case SDLK_GREATER: /* ? */ break;
		case SDLK_QUESTION: /* ? */ break;
		case SDLK_AT: /* ? */ break;

		case SDLK_LEFTBRACKET: v = MKC_LeftBracket; break;
		case SDLK_BACKSLASH: v = MKC_formac_BackSlash; break;
		case SDLK_RIGHTBRACKET: v = MKC_RightBracket; break;
		case SDLK_CARET: /* ? */ break;
		case SDLK_UNDERSCORE: /* ? */ break;
		case SDLK_BACKQUOTE: v = MKC_formac_Grave; break;

		case SDLK_a: v = MKC_A; break;
		case SDLK_b: v = MKC_B; break;
		case SDLK_c: v = MKC_C; break;
		case SDLK_d: v = MKC_D; break;
		case SDLK_e: v = MKC_E; break;
		case SDLK_f: v = MKC_F; break;
		case SDLK_g: v = MKC_G; break;
		case SDLK_h: v = MKC_H; break;
		case SDLK_i: v = MKC_I; break;
		case SDLK_j: v = MKC_J; break;
		case SDLK_k: v = MKC_K; break;
		case SDLK_l: v = MKC_L; break;
		case SDLK_m: v = MKC_M; break;
		case SDLK_n: v = MKC_N; break;
		case SDLK_o: v = MKC_O; break;
		case SDLK_p: v = MKC_P; break;
		case SDLK_q: v = MKC_Q; break;
		case SDLK_r: v = MKC_R; break;
		case SDLK_s: v = MKC_S; break;
		case SDLK_t: v = MKC_T; break;
		case SDLK_u: v = MKC_U; break;
		case SDLK_v: v = MKC_V; break;
		case SDLK_w: v = MKC_W; break;
		case SDLK_x: v = MKC_X; break;
		case SDLK_y: v = MKC_Y; break;
		case SDLK_z: v = MKC_Z; break;

		case SDLK_KP0: v = MKC_KP0; break;
		case SDLK_KP1: v = MKC_KP1; break;
		case SDLK_KP2: v = MKC_KP2; break;
		case SDLK_KP3: v = MKC_KP3; break;
		case SDLK_KP4: v = MKC_KP4; break;
		case SDLK_KP5: v = MKC_KP5; break;
		case SDLK_KP6: v = MKC_KP6; break;
		case SDLK_KP7: v = MKC_KP7; break;
		case SDLK_KP8: v = MKC_KP8; break;
		case SDLK_KP9: v = MKC_KP9; break;

		case SDLK_KP_PERIOD: v = MKC_Decimal; break;
		case SDLK_KP_DIVIDE: v = MKC_KPDevide; break;
		case SDLK_KP_MULTIPLY: v = MKC_KPMultiply; break;
		case SDLK_KP_MINUS: v = MKC_KPSubtract; break;
		case SDLK_KP_PLUS: v = MKC_KPAdd; break;
		case SDLK_KP_ENTER: v = MKC_formac_Enter; break;
		case SDLK_KP_EQUALS: v = MKC_KPEqual; break;

		case SDLK_UP: v = MKC_Up; break;
		case SDLK_DOWN: v = MKC_Down; break;
		case SDLK_RIGHT: v = MKC_Right; break;
		case SDLK_LEFT: v = MKC_Left; break;
		case SDLK_INSERT: v = MKC_formac_Help; break;
		case SDLK_HOME: v = MKC_formac_Home; break;
		case SDLK_END: v = MKC_formac_End; break;
		case SDLK_PAGEUP: v = MKC_formac_PageUp; break;
		case SDLK_PAGEDOWN: v = MKC_formac_PageDown; break;

		case SDLK_F1: v = MKC_formac_F1; break;
		case SDLK_F2: v = MKC_formac_F2; break;
		case SDLK_F3: v = MKC_formac_F3; break;
		case SDLK_F4: v = MKC_formac_F4; break;
		case SDLK_F5: v = MKC_formac_F5; break;
		case SDLK_F6: v = MKC_F6; break;
		case SDLK_F7: v = MKC_F7; break;
		case SDLK_F8: v = MKC_F8; break;
		case SDLK_F9: v = MKC_F9; break;
		case SDLK_F10: v = MKC_F10; break;
		case SDLK_F11: v = MKC_F11; break;
		case SDLK_F12: v = MKC_F12; break;

		case SDLK_F13: /* ? */ break;
		case SDLK_F14: /* ? */ break;
		case SDLK_F15: /* ? */ break;

		case SDLK_NUMLOCK: v = MKC_formac_ForwardDel; break;
		case SDLK_CAPSLOCK: v = MKC_formac_CapsLock; break;
		case SDLK_SCROLLOCK: v = MKC_ScrollLock; break;
		case SDLK_RSHIFT: v = MKC_formac_RShift; break;
		case SDLK_LSHIFT: v = MKC_formac_Shift; break;
		case SDLK_RCTRL: v = MKC_formac_RControl; break;
		case SDLK_LCTRL: v = MKC_formac_Control; break;
		case SDLK_RALT: v = MKC_formac_RCommand; break;
		case SDLK_LALT: v = MKC_formac_Command; break;
		case SDLK_RMETA: v = MKC_formac_RCommand; break;
		case SDLK_LMETA: v = MKC_formac_Command; break;
		case SDLK_LSUPER: v = MKC_formac_Option; break;
		case SDLK_RSUPER: v = MKC_formac_ROption; break;

		case SDLK_MODE: /* ? */ break;
		case SDLK_COMPOSE: /* ? */ break;

		case SDLK_HELP: v = MKC_formac_Help; break;
		case SDLK_PRINT: v = MKC_Print; break;

		case SDLK_SYSREQ: /* ? */ break;
		case SDLK_BREAK: /* ? */ break;
		case SDLK_MENU: /* ? */ break;
		case SDLK_POWER: /* ? */ break;
		case SDLK_EURO: /* ? */ break;
		case SDLK_UNDO: /* ? */ break;

		default:
			break;
	}

	return v;
}
#endif /* SDL_MAJOR_VERSION */

#if 1 == SDL_MAJOR_VERSION
LOCALPROC DoKeyCode(SDL_keysym *r, blnr down)
{
	ui3r v = SDLKey2MacKeyCode(r->sym);
	if (MKC_None != v) {
		Keyboard_UpdateKeyMap2(v, down);
	}
}
#endif /* SDL_MAJOR_VERSION */

GLOBALOSGLUPROC MinivMacAPI_UpdateKey( uint8_t Key, uint8_t Down ) {
	Keyboard_UpdateKeyMap2( Key, Down );
}

LOCALPROC DisableKeyRepeat(void)
{
	/*
		OSGLUxxx common:
		If possible and useful, disable keyboard autorepeat.
	*/
}

LOCALPROC RestoreKeyRepeat(void)
{
	/*
		OSGLUxxx common:
		Undo any effects of DisableKeyRepeat.
	*/
}

LOCALPROC ReconnectKeyCodes3(void)
{
}

LOCALPROC DisconnectKeyCodes3(void)
{
	DisconnectKeyCodes2();
	MyMouseButtonSet(falseblnr);
}

/* --- time, date, location --- */

#define dbglog_TimeStuff (0 && dbglog_HAVE)

LOCALVAR ui5b TrueEmulatedTime = 0;
	/*
		OSGLUxxx common:
		The amount of time the program has
		been running, measured in Macintosh
		"ticks". There are 60.14742 ticks per
		second.

		(time when the emulation is
		stopped for more than a few ticks
		should not be counted.)
	*/

#define HaveWorkingTime 1

#define MyInvTimeDivPow 16
#define MyInvTimeDiv (1 << MyInvTimeDivPow)
#define MyInvTimeDivMask (MyInvTimeDiv - 1)
#define MyInvTimeStep 1089590 /* 1000 / 60.14742 * MyInvTimeDiv */

LOCALVAR uint64_t LastTime;

LOCALVAR uint64_t NextIntTime;
LOCALVAR ui5b NextFracTime;

LOCALPROC IncrNextTime(void)
{
	NextFracTime += MyInvTimeStep;
	NextIntTime += (NextFracTime >> MyInvTimeDivPow);
	NextFracTime &= MyInvTimeDivMask;
}

LOCALPROC InitNextTime(void)
{
	NextIntTime = LastTime;
	NextFracTime = 0;
	IncrNextTime();
}

LOCALVAR ui5b NewMacDateInSeconds;

LOCALFUNC blnr UpdateTrueEmulatedTime(void)
{
	/*
		OSGLUxxx common:
		Update TrueEmulatedTime. Needs to convert between how the host
		operating system measures time and Macintosh ticks.
	*/
	uint64_t LatestTime;
	si5b TimeDiff;

	LatestTime = ArduinoAPI_GetTimeMS( );

	if (LatestTime != LastTime) {

		NewMacDateInSeconds = LatestTime / 1000;
			/* no date and time api in SDL */

		LastTime = LatestTime;
		TimeDiff = (LatestTime - NextIntTime);
			/* this should work even when time wraps */
		if (TimeDiff >= 0) {
			if (TimeDiff > 256) {
				/* emulation interrupted, forget it */
				++TrueEmulatedTime;
				InitNextTime();

#if dbglog_TimeStuff
				dbglog_writelnNum("emulation interrupted",
					TrueEmulatedTime);
#endif
			} else {
				do {
					++TrueEmulatedTime;
					IncrNextTime();
					TimeDiff = (LatestTime - NextIntTime);
				} while (TimeDiff >= 0);
			}
			return trueblnr;
		} else {
			if (TimeDiff < -256) {
#if dbglog_TimeStuff
				dbglog_writeln("clock set back");
#endif
				/* clock goofed if ever get here, reset */
				InitNextTime();
			}
		}
	}

	return falseblnr;
}


LOCALFUNC blnr CheckDateTime(void)
{
	/*
		OSGLUxxx common:
		Update CurMacDateInSeconds, the number
		of seconds since midnight January 1, 1904.

		return true if CurMacDateInSeconds is
		different than it was on the last
		call to CheckDateTime.
	*/

	if (CurMacDateInSeconds != NewMacDateInSeconds) {
		CurMacDateInSeconds = NewMacDateInSeconds;
		return trueblnr;
	} else {
		return falseblnr;
	}
}

LOCALPROC StartUpTimeAdjust(void)
{
	/*
		OSGLUxxx common:
		prepare to call UpdateTrueEmulatedTime.

		will be called again when haven't been
		regularly calling UpdateTrueEmulatedTime,
		(such as the emulation has been stopped).
	*/

	LastTime = ArduinoAPI_GetTimeMS( );
	InitNextTime();
}

LOCALFUNC blnr InitLocationDat(void)
{
#if dbglog_OSGInit
	dbglog_writeln("enter InitLocationDat");
#endif

	LastTime = ArduinoAPI_GetTimeMS( );
	InitNextTime();
	NewMacDateInSeconds = LastTime / 1000;
	CurMacDateInSeconds = NewMacDateInSeconds;

	return trueblnr;
}

/* --- sound --- */

#if MySoundEnabled

#define kLn2SoundBuffers 4 /* kSoundBuffers must be a power of two */
#define kSoundBuffers (1 << kLn2SoundBuffers)
#define kSoundBuffMask (kSoundBuffers - 1)

#define DesiredMinFilledSoundBuffs 3
	/*
		if too big then sound lags behind emulation.
		if too small then sound will have pauses.
	*/

#define kLnOneBuffLen 9
#define kLnAllBuffLen (kLn2SoundBuffers + kLnOneBuffLen)
#define kOneBuffLen (1UL << kLnOneBuffLen)
#define kAllBuffLen (1UL << kLnAllBuffLen)
#define kLnOneBuffSz (kLnOneBuffLen + kLn2SoundSampSz - 3)
#define kLnAllBuffSz (kLnAllBuffLen + kLn2SoundSampSz - 3)
#define kOneBuffSz (1UL << kLnOneBuffSz)
#define kAllBuffSz (1UL << kLnAllBuffSz)
#define kOneBuffMask (kOneBuffLen - 1)
#define kAllBuffMask (kAllBuffLen - 1)
#define dbhBufferSize (kAllBuffSz + kOneBuffSz)

#define dbglog_SoundStuff (0 && dbglog_HAVE)
#define dbglog_SoundBuffStats (0 && dbglog_HAVE)

LOCALVAR tpSoundSamp TheSoundBuffer = nullpr;
volatile static ui4b ThePlayOffset;
volatile static ui4b TheFillOffset;
volatile static ui4b MinFilledSoundBuffs;
#if dbglog_SoundBuffStats
LOCALVAR ui4b MaxFilledSoundBuffs;
#endif
LOCALVAR ui4b TheWriteOffset;

LOCALPROC MySound_Init0(void)
{
	ThePlayOffset = 0;
	TheFillOffset = 0;
	TheWriteOffset = 0;
}

LOCALPROC MySound_Start0(void)
{
	/* Reset variables */
	MinFilledSoundBuffs = kSoundBuffers + 1;
#if dbglog_SoundBuffStats
	MaxFilledSoundBuffs = 0;
#endif
}

GLOBALOSGLUFUNC tpSoundSamp MySound_BeginWrite(ui4r n, ui4r *actL)
{
	ui4b ToFillLen = kAllBuffLen - (TheWriteOffset - ThePlayOffset);
	ui4b WriteBuffContig =
		kOneBuffLen - (TheWriteOffset & kOneBuffMask);

	if (WriteBuffContig < n) {
		n = WriteBuffContig;
	}
	if (ToFillLen < n) {
		/* overwrite previous buffer */
#if dbglog_SoundStuff
		dbglog_writeln("sound buffer over flow");
#endif
		TheWriteOffset -= kOneBuffLen;
	}

	*actL = n;
	return TheSoundBuffer + (TheWriteOffset & kAllBuffMask);
}

#if 4 == kLn2SoundSampSz
LOCALPROC ConvertSoundBlockToNative(tpSoundSamp p)
{
	int i;

	for (i = kOneBuffLen; --i >= 0; ) {
		*p++ -= 0x8000;
	}
}
#else
#define ConvertSoundBlockToNative(p)
#endif

LOCALPROC MySound_WroteABlock(void)
{
#if (4 == kLn2SoundSampSz)
	ui4b PrevWriteOffset = TheWriteOffset - kOneBuffLen;
	tpSoundSamp p = TheSoundBuffer + (PrevWriteOffset & kAllBuffMask);
#endif

#if dbglog_SoundStuff
	dbglog_writeln("enter MySound_WroteABlock");
#endif

	ConvertSoundBlockToNative(p);

	TheFillOffset = TheWriteOffset;

#if dbglog_SoundBuffStats
	{
		ui4b ToPlayLen = TheFillOffset
			- ThePlayOffset;
		ui4b ToPlayBuffs = ToPlayLen >> kLnOneBuffLen;

		if (ToPlayBuffs > MaxFilledSoundBuffs) {
			MaxFilledSoundBuffs = ToPlayBuffs;
		}
	}
#endif
}

LOCALFUNC blnr MySound_EndWrite0(ui4r actL)
{
	blnr v;

	TheWriteOffset += actL;

	if (0 != (TheWriteOffset & kOneBuffMask)) {
		v = falseblnr;
	} else {
		/* just finished a block */

		MySound_WroteABlock();

		v = trueblnr;
	}

	return v;
}

LOCALPROC MySound_SecondNotify0(void)
{
	if (MinFilledSoundBuffs <= kSoundBuffers) {
		if (MinFilledSoundBuffs > DesiredMinFilledSoundBuffs) {
#if dbglog_SoundStuff
			dbglog_writeln("MinFilledSoundBuffs too high");
#endif
			IncrNextTime();
		} else if (MinFilledSoundBuffs < DesiredMinFilledSoundBuffs) {
#if dbglog_SoundStuff
			dbglog_writeln("MinFilledSoundBuffs too low");
#endif
			++TrueEmulatedTime;
		}
#if dbglog_SoundBuffStats
		dbglog_writelnNum("MinFilledSoundBuffs",
			MinFilledSoundBuffs);
		dbglog_writelnNum("MaxFilledSoundBuffs",
			MaxFilledSoundBuffs);
		MaxFilledSoundBuffs = 0;
#endif
		MinFilledSoundBuffs = kSoundBuffers + 1;
	}
}

typedef ui4r trSoundTemp;

#define kCenterTempSound 0x8000

#define AudioStepVal 0x0040

#if 3 == kLn2SoundSampSz
#define ConvertTempSoundSampleFromNative(v) ((v) << 8)
#elif 4 == kLn2SoundSampSz
#define ConvertTempSoundSampleFromNative(v) ((v) + kCenterSound)
#else
#error "unsupported kLn2SoundSampSz"
#endif

#if 3 == kLn2SoundSampSz
#define ConvertTempSoundSampleToNative(v) ((v) >> 8)
#elif 4 == kLn2SoundSampSz
#define ConvertTempSoundSampleToNative(v) ((v) - kCenterSound)
#else
#error "unsupported kLn2SoundSampSz"
#endif

LOCALPROC SoundRampTo(trSoundTemp *last_val, trSoundTemp dst_val,
	tpSoundSamp *stream, int *len)
{
	trSoundTemp diff;
	tpSoundSamp p = *stream;
	int n = *len;
	trSoundTemp v1 = *last_val;

	while ((v1 != dst_val) && (0 != n)) {
		if (v1 > dst_val) {
			diff = v1 - dst_val;
			if (diff > AudioStepVal) {
				v1 -= AudioStepVal;
			} else {
				v1 = dst_val;
			}
		} else {
			diff = dst_val - v1;
			if (diff > AudioStepVal) {
				v1 += AudioStepVal;
			} else {
				v1 = dst_val;
			}
		}

		--n;
		*p++ = ConvertTempSoundSampleToNative(v1);
	}

	*stream = p;
	*len = n;
	*last_val = v1;
}

struct MySoundR {
	tpSoundSamp fTheSoundBuffer;
	volatile ui4b (*fPlayOffset);
	volatile ui4b (*fFillOffset);
	volatile ui4b (*fMinFilledSoundBuffs);

	volatile trSoundTemp lastv;

	blnr wantplaying;
	blnr HaveStartedPlaying;
};
typedef struct MySoundR MySoundR;

#if 0 != SDL_MAJOR_VERSION
static void my_audio_callback(void *udata, Uint8 *stream, int len)
{
	ui4b ToPlayLen;
	ui4b FilledSoundBuffs;
	int i;
	MySoundR *datp = (MySoundR *)udata;
	tpSoundSamp CurSoundBuffer = datp->fTheSoundBuffer;
	ui4b CurPlayOffset = *datp->fPlayOffset;
	trSoundTemp v0 = datp->lastv;
	trSoundTemp v1 = v0;
	tpSoundSamp dst = (tpSoundSamp)stream;

#if kLn2SoundSampSz > 3
	len >>= (kLn2SoundSampSz - 3);
#endif

#if dbglog_SoundStuff
	dbglog_writeln("Enter my_audio_callback");
	dbglog_writelnNum("len", len);
#endif

label_retry:
	ToPlayLen = *datp->fFillOffset - CurPlayOffset;
	FilledSoundBuffs = ToPlayLen >> kLnOneBuffLen;

	if (! datp->wantplaying) {
#if dbglog_SoundStuff
		dbglog_writeln("playing end transistion");
#endif

		SoundRampTo(&v1, kCenterTempSound, &dst, &len);

		ToPlayLen = 0;
	} else if (! datp->HaveStartedPlaying) {
#if dbglog_SoundStuff
		dbglog_writeln("playing start block");
#endif

		if ((ToPlayLen >> kLnOneBuffLen) < 8) {
			ToPlayLen = 0;
		} else {
			tpSoundSamp p = datp->fTheSoundBuffer
				+ (CurPlayOffset & kAllBuffMask);
			trSoundTemp v2 = ConvertTempSoundSampleFromNative(*p);

#if dbglog_SoundStuff
			dbglog_writeln("have enough samples to start");
#endif

			SoundRampTo(&v1, v2, &dst, &len);

			if (v1 == v2) {
#if dbglog_SoundStuff
				dbglog_writeln("finished start transition");
#endif

				datp->HaveStartedPlaying = trueblnr;
			}
		}
	}

	if (0 == len) {
		/* done */

		if (FilledSoundBuffs < *datp->fMinFilledSoundBuffs) {
			*datp->fMinFilledSoundBuffs = FilledSoundBuffs;
		}
	} else if (0 == ToPlayLen) {

#if dbglog_SoundStuff
		dbglog_writeln("under run");
#endif

		for (i = 0; i < len; ++i) {
			*dst++ = ConvertTempSoundSampleToNative(v1);
		}
		*datp->fMinFilledSoundBuffs = 0;
	} else {
		ui4b PlayBuffContig = kAllBuffLen
			- (CurPlayOffset & kAllBuffMask);
		tpSoundSamp p = CurSoundBuffer
			+ (CurPlayOffset & kAllBuffMask);

		if (ToPlayLen > PlayBuffContig) {
			ToPlayLen = PlayBuffContig;
		}
		if (ToPlayLen > len) {
			ToPlayLen = len;
		}

		for (i = 0; i < ToPlayLen; ++i) {
			*dst++ = *p++;
		}
		v1 = ConvertTempSoundSampleFromNative(p[-1]);

		CurPlayOffset += ToPlayLen;
		len -= ToPlayLen;

		*datp->fPlayOffset = CurPlayOffset;

		goto label_retry;
	}

	datp->lastv = v1;
}
#endif /* 0 != SDL_MAJOR_VERSION */

LOCALVAR MySoundR cur_audio;

LOCALVAR blnr HaveSoundOut = falseblnr;

LOCALPROC MySound_Stop(void)
{
#if dbglog_SoundStuff
	dbglog_writeln("enter MySound_Stop");
#endif

	if (cur_audio.wantplaying && HaveSoundOut) {
		ui4r retry_limit = 50; /* half of a second */

		cur_audio.wantplaying = falseblnr;

label_retry:
		if (kCenterTempSound == cur_audio.lastv) {
#if dbglog_SoundStuff
			dbglog_writeln("reached kCenterTempSound");
#endif

			/* done */
		} else if (0 == --retry_limit) {
#if dbglog_SoundStuff
			dbglog_writeln("retry limit reached");
#endif
			/* done */
		} else
		{
			/*
				give time back, particularly important
				if got here on a suspend event.
			*/

#if dbglog_SoundStuff
			dbglog_writeln("busy, so sleep");
#endif

#if 0 != SDL_MAJOR_VERSION
			(void) SDL_Delay(10);
#endif

			goto label_retry;
		}

#if 0 != SDL_MAJOR_VERSION
		SDL_PauseAudio(1);
#endif
	}

#if dbglog_SoundStuff
	dbglog_writeln("leave MySound_Stop");
#endif
}

LOCALPROC MySound_Start(void)
{
	if ((! cur_audio.wantplaying) && HaveSoundOut) {
		MySound_Start0();
		cur_audio.lastv = kCenterTempSound;
		cur_audio.HaveStartedPlaying = falseblnr;
		cur_audio.wantplaying = trueblnr;

#if 0 != SDL_MAJOR_VERSION
		SDL_PauseAudio(0);
#endif
	}
}

LOCALPROC MySound_UnInit(void)
{
	if (HaveSoundOut) {
#if 0 != SDL_MAJOR_VERSION
		SDL_CloseAudio();
#endif
	}
}

#define SOUND_SAMPLERATE 22255 /* = round(7833600 * 2 / 704) */

LOCALFUNC blnr MySound_Init(void)
{
#if dbglog_OSGInit
	dbglog_writeln("enter MySound_Init");
#endif

#if 0 != SDL_MAJOR_VERSION
	SDL_AudioSpec desired;
#endif

	MySound_Init0();

	cur_audio.fTheSoundBuffer = TheSoundBuffer;
	cur_audio.fPlayOffset = &ThePlayOffset;
	cur_audio.fFillOffset = &TheFillOffset;
	cur_audio.fMinFilledSoundBuffs = &MinFilledSoundBuffs;
	cur_audio.wantplaying = falseblnr;

#if 0 != SDL_MAJOR_VERSION
	desired.freq = SOUND_SAMPLERATE;

#if 3 == kLn2SoundSampSz
	desired.format = AUDIO_U8;
#elif 4 == kLn2SoundSampSz
	desired.format = AUDIO_S16SYS;
#else
#error "unsupported audio format"
#endif

	desired.channels = 1;
	desired.samples = 1024;
	desired.callback = my_audio_callback;
	desired.userdata = (void *)&cur_audio;

	/* Open the audio device */
	if (SDL_OpenAudio(&desired, NULL) < 0) {
		fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
	} else {
		HaveSoundOut = trueblnr;

		MySound_Start();
			/*
				This should be taken care of by LeaveSpeedStopped,
				but since takes a while to get going properly,
				start early.
			*/
	}
#endif /* 0 != SDL_MAJOR_VERSION */

	return trueblnr; /* keep going, even if no sound */
}

GLOBALOSGLUPROC MySound_EndWrite(ui4r actL)
{
	if (MySound_EndWrite0(actL)) {
	}
}

LOCALPROC MySound_SecondNotify(void)
{
	/*
		OSGLUxxx common:
		called once a second.
		can be used to check if sound output it
		lagging or gaining, and if so
		adjust emulated time by a tick.
	*/

	if (HaveSoundOut) {
		MySound_SecondNotify0();
	}
}

#endif /* MySoundEnabled */

/* --- basic dialogs --- */

LOCALPROC CheckSavedMacMsg(void)
{
	/*
		OSGLUxxx common:
		This is currently only used in the
		rare case where there is a message
		still pending as the program quits.
	*/

	if (nullpr != SavedBriefMsg) {
		char briefMsg0[ClStrMaxLength + 1];
		char longMsg0[ClStrMaxLength + 1];

		NativeStrFromCStr(briefMsg0, SavedBriefMsg);
		NativeStrFromCStr(longMsg0, SavedLongMsg);

#if 2 == SDL_MAJOR_VERSION
		if (0 != SDL_ShowSimpleMessageBox(
			SDL_MESSAGEBOX_ERROR,
			SavedBriefMsg,
			SavedLongMsg,
			my_main_wind
			))
#endif
		{
			fprintf(stderr, "%s\n", briefMsg0);
			fprintf(stderr, "%s\n", longMsg0);
		}

		SavedBriefMsg = nullpr;
	}
}

/* --- event handling for main window --- */

#define UseMotionEvents 1

#if UseMotionEvents
LOCALVAR blnr CaughtMouse = falseblnr;
#endif

#if 0 != SDL_MAJOR_VERSION
LOCALPROC HandleTheEvent(SDL_Event *event)
{
	switch (event->type) {
		case SDL_QUIT:
			RequestMacOff = trueblnr;
			break;
#if 1 == SDL_MAJOR_VERSION
		case SDL_ACTIVEEVENT:
			switch (event->active.state) {
				case SDL_APPINPUTFOCUS:
					gTrueBackgroundFlag = (0 == event->active.gain);
#if 0 && UseMotionEvents
					if (! gTrueBackgroundFlag) {
						CheckMouseState();
					}
#endif
					break;
				case SDL_APPMOUSEFOCUS:
					CaughtMouse = (0 != event->active.gain);
					break;
			}
			break;
#endif /* 1 == SDL_MAJOR_VERSION */
#if 2 == SDL_MAJOR_VERSION
		case SDL_WINDOWEVENT:
			switch (event->window.event) {
				case SDL_WINDOWEVENT_FOCUS_GAINED:
					gTrueBackgroundFlag = 0;
					break;
				case SDL_WINDOWEVENT_FOCUS_LOST:
					gTrueBackgroundFlag = 1;
					break;
				case SDL_WINDOWEVENT_ENTER:
					CaughtMouse = 1;
					break;
				case SDL_WINDOWEVENT_LEAVE:
					CaughtMouse = 0;
					break;
			}
			break;
#endif /* 2 == SDL_MAJOR_VERSION */
		case SDL_MOUSEMOTION:
#if EnableFSMouseMotion && ! HaveWorkingWarp
			if (HaveMouseMotion) {
				MousePositionNotifyRelative(
					event->motion.xrel, event->motion.yrel);
			} else
#endif
			{
				MousePositionNotify(
					event->motion.x, event->motion.y);
			}
			break;
		case SDL_MOUSEBUTTONDOWN:
			/* any mouse button, we don't care which */
#if EnableFSMouseMotion && ! HaveWorkingWarp
			if (HaveMouseMotion) {
				/* ignore position */
			} else
#endif
			{
				MousePositionNotify(
					event->button.x, event->button.y);
			}
			MyMouseButtonSet(trueblnr);
			break;
		case SDL_MOUSEBUTTONUP:
#if EnableFSMouseMotion && ! HaveWorkingWarp
			if (HaveMouseMotion) {
				/* ignore position */
			} else
#endif
			{
				MousePositionNotify(
					event->button.x, event->button.y);
			}
			MyMouseButtonSet(falseblnr);
			break;
		case SDL_KEYDOWN:
			DoKeyCode(&event->key.keysym, trueblnr);
			break;
		case SDL_KEYUP:
			DoKeyCode(&event->key.keysym, falseblnr);
			break;
#if 2 == SDL_MAJOR_VERSION
		case SDL_MOUSEWHEEL:
			if (event->wheel.x < 0) {
				Keyboard_UpdateKeyMap2(MKC_Left, trueblnr);
				Keyboard_UpdateKeyMap2(MKC_Left, falseblnr);
			} else if (event->wheel.x > 0) {
				Keyboard_UpdateKeyMap2(MKC_Right, trueblnr);
				Keyboard_UpdateKeyMap2(MKC_Right, falseblnr);
			}
			if (event->wheel.y < 0) {
				Keyboard_UpdateKeyMap2(MKC_Down, trueblnr);
				Keyboard_UpdateKeyMap2(MKC_Down, falseblnr);
			} else if(event->wheel.y > 0) {
				Keyboard_UpdateKeyMap2(MKC_Up, trueblnr);
				Keyboard_UpdateKeyMap2(MKC_Up, falseblnr);
			}
			break;
		case SDL_DROPFILE:
			{
				char *s = event->drop.file;

				(void) Sony_Insert1a(s, falseblnr);
				SDL_RaiseWindow(my_main_wind);
				SDL_free(s);
			}
			break;
#endif /* 2 == SDL_MAJOR_VERSION */
#if 0
		case Expose: /* SDL doesn't have an expose event */
			int x0 = event->expose.x;
			int y0 = event->expose.y;
			int x1 = x0 + event->expose.width;
			int y1 = y0 + event->expose.height;

			if (x0 < 0) {
				x0 = 0;
			}
			if (x1 > vMacScreenWidth) {
				x1 = vMacScreenWidth;
			}
			if (y0 < 0) {
				y0 = 0;
			}
			if (y1 > vMacScreenHeight) {
				y1 = vMacScreenHeight;
			}
			if ((x0 < x1) && (y0 < y1)) {
				HaveChangedScreenBuff(y0, x0, y1, x1);
			}
			break;
#endif
	}
}
#endif

/* --- main window creation and disposal --- */

LOCALVAR int my_argc;
LOCALVAR char **my_argv;

LOCALFUNC blnr Screen_Init(void)
{
#if dbglog_OSGInit
	dbglog_writeln("enter Screen_Init");
#endif

	InitKeyCodes();

	return trueblnr;
}

#if EnableFSMouseMotion && HaveWorkingWarp
LOCALPROC MyMouseConstrain(void)
{
	si4b shiftdh;
	si4b shiftdv;

	if (SavedMouseH < ViewHStart + (ViewHSize / 4)) {
		shiftdh = ViewHSize / 2;
	} else if (SavedMouseH > ViewHStart + ViewHSize - (ViewHSize / 4)) {
		shiftdh = - ViewHSize / 2;
	} else {
		shiftdh = 0;
	}
	if (SavedMouseV < ViewVStart + (ViewVSize / 4)) {
		shiftdv = ViewVSize / 2;
	} else if (SavedMouseV > ViewVStart + ViewVSize - (ViewVSize / 4)) {
		shiftdv = - ViewVSize / 2;
	} else {
		shiftdv = 0;
	}
	if ((shiftdh != 0) || (shiftdv != 0)) {
		SavedMouseH += shiftdh;
		SavedMouseV += shiftdv;
		if (! MyMoveMouse(SavedMouseH, SavedMouseV)) {
			HaveMouseMotion = falseblnr;
		}
	}
}
#endif

LOCALFUNC blnr CreateMainWindow(void)
{
#if dbglog_OSGInit
	dbglog_writeln("enter CreateMainWindow");
#endif

	return trueblnr;
}

LOCALPROC CloseMainWindow(void)
{
}

LOCALPROC ZapWinStateVars(void)
{
}

#if VarFullScreen
LOCALPROC ToggleWantFullScreen(void)
{
	WantFullScreen = ! WantFullScreen;
}
#endif

/* --- SavedTasks --- */

LOCALPROC LeaveBackground(void)
{
	ReconnectKeyCodes3();
	DisableKeyRepeat();
}

LOCALPROC EnterBackground(void)
{
	RestoreKeyRepeat();
	DisconnectKeyCodes3();

	ForceShowCursor();
}

LOCALPROC LeaveSpeedStopped(void)
{
#if MySoundEnabled
	MySound_Start();
#endif

	StartUpTimeAdjust();
}

LOCALPROC EnterSpeedStopped(void)
{
#if MySoundEnabled
	MySound_Stop();
#endif
}

LOCALPROC CheckForSavedTasks(void)
{
	if (MyEvtQNeedRecover) {
		MyEvtQNeedRecover = falseblnr;

		/* attempt cleanup, MyEvtQNeedRecover may get set again */
		MyEvtQTryRecoverFromFull();
	}

#if EnableFSMouseMotion && HaveWorkingWarp
	if (HaveMouseMotion) {
		MyMouseConstrain();
	}
#endif

	if (RequestMacOff) {
		RequestMacOff = falseblnr;
		if (AnyDiskInserted()) {
			MacMsgOverride(kStrQuitWarningTitle,
				kStrQuitWarningMessage);
		} else {
			ForceMacOff = trueblnr;
		}
	}

	if (ForceMacOff) {
		return;
	}

	if (gTrueBackgroundFlag != gBackgroundFlag) {
		gBackgroundFlag = gTrueBackgroundFlag;
		if (gTrueBackgroundFlag) {
			EnterBackground();
		} else {
			LeaveBackground();
		}
	}

	if (CurSpeedStopped != (SpeedStopped ||
		(gBackgroundFlag && ! RunInBackground
#if EnableAutoSlow && 0
			&& (QuietSubTicks >= 4092)
#endif
		)))
	{
		CurSpeedStopped = ! CurSpeedStopped;
		if (CurSpeedStopped) {
			EnterSpeedStopped();
		} else {
			LeaveSpeedStopped();
		}
	}

	if ((nullpr != SavedBriefMsg) & ! MacMsgDisplayed) {
		MacMsgDisplayOn();
	}

	if (NeedWholeScreenDraw) {
		NeedWholeScreenDraw = falseblnr;
		ScreenChangedAll();
	}

#if NeedRequestIthDisk
	if (0 != RequestIthDisk) {
		Sony_InsertIth(RequestIthDisk);
		RequestIthDisk = 0;
	}
#endif
}

/* --- main program flow --- */

LOCALPROC WaitForTheNextEvent(void)
{
	ArduinoAPI_Yield( );
}

LOCALPROC CheckForSystemEvents(void)
{
	/*
		OSGLUxxx common:
		Handle any events that are waiting for us.
		Return immediately when no more events
		are waiting, don't wait for more.
	*/
	ArduinoAPI_CheckForEvents( );
	ArduinoAPI_DrawScreen( GetCurDrawBuff( ) );
}

/*
	OSGLUxxx common:
	In general, attempt to emulate one Macintosh tick (1/60.14742
	seconds) for every tick of real time. When done emulating
	one tick, wait for one tick of real time to elapse, by
	calling WaitForNextTick.

	But, Mini vMac can run the emulation at greater than 1x speed, up to
	and including running as fast as possible, by emulating extra cycles
	at the end of the emulated tick. In this case, the extra emulation
	should continue only as long as the current real time tick is not
	over - until ExtraTimeNotOver returns false.
*/

GLOBALOSGLUFUNC blnr ExtraTimeNotOver(void)
{
	UpdateTrueEmulatedTime();
	return TrueEmulatedTime == OnTrueTime;
}

GLOBALOSGLUPROC WaitForNextTick(void)
{
label_retry:
	CheckForSystemEvents();
	CheckForSavedTasks();

	if (ForceMacOff) {
		return;
	}

	if (CurSpeedStopped) {
		DoneWithDrawingForTick();
		WaitForTheNextEvent();
		goto label_retry;
	}

#if ! HaveWorkingTime
	++TrueEmulatedTime;
#endif

	if (ExtraTimeNotOver()) {
		ArduinoAPI_Yield( );
		goto label_retry;
	}

	if (CheckDateTime()) {
#if MySoundEnabled
		MySound_SecondNotify();
#endif
#if EnableDemoMsg
		DemoModeSecondNotify();
#endif
	}

	if ((! gBackgroundFlag)
#if UseMotionEvents
		&& (! CaughtMouse)
#endif
		)
	{
		CheckMouseState();
	}

	OnTrueTime = TrueEmulatedTime;

#if dbglog_TimeStuff
	dbglog_writelnNum("WaitForNextTick, OnTrueTime", OnTrueTime);
#endif
}

/* --- platform independent code can be thought of as going here --- */

#include "PROGMAIN.h"

LOCALPROC ZapOSGLUVars(void)
{
	/*
		OSGLUxxx common:
		Set initial values of variables for
		platform dependent code, where not
		done using c initializers. (such
		as for arrays.)
	*/

	InitDrives();
	ZapWinStateVars();
}

LOCALPROC ReserveAllocAll(void)
{
#if dbglog_HAVE
	dbglog_ReserveAlloc();
#endif
	ReserveAllocOneBlock(&ROM, kROM_Size, 5, falseblnr);

	ReserveAllocOneBlock(&screencomparebuff,
		vMacScreenNumBytes, 5, trueblnr);
#if UseControlKeys
	ReserveAllocOneBlock(&CntrlDisplayBuff,
		vMacScreenNumBytes, 5, falseblnr);
#endif

#if MySoundEnabled
	ReserveAllocOneBlock((ui3p *)&TheSoundBuffer,
		dbhBufferSize, 5, falseblnr);
#endif

	EmulationReserveAlloc();
}

LOCALFUNC blnr AllocMyMemory(void)
{
	uimr n;
	blnr IsOk = falseblnr;

	ReserveAllocOffset = 0;
	ReserveAllocBigBlock = nullpr;
	ReserveAllocAll();
	n = ReserveAllocOffset;
	ReserveAllocBigBlock = (ui3p) ArduinoAPI_calloc(1, n);
	if (NULL == ReserveAllocBigBlock) {
		MacMsg(kStrOutOfMemTitle, kStrOutOfMemMessage, trueblnr);
	} else {
		ReserveAllocOffset = 0;
		ReserveAllocAll();
		if (n != ReserveAllocOffset) {
			/* oops, program error */
		} else {
			IsOk = trueblnr;
		}
	}

	return IsOk;
}

LOCALPROC UnallocMyMemory(void)
{
	if (nullpr != ReserveAllocBigBlock) {
		ArduinoAPI_free((char *)ReserveAllocBigBlock);
	}
}

LOCALFUNC blnr InitOSGLU(void)
{
	/*
		OSGLUxxx common:
		Run all the initializations needed for the program.
	*/

	if (AllocMyMemory())
#if dbglog_HAVE
	if (dbglog_open())
#endif
	if (LoadMacRom())
	if (LoadInitialImages())
	if (InitLocationDat())
#if MySoundEnabled
	if (MySound_Init())
#endif
	if (Screen_Init())
	if (CreateMainWindow())
	if (WaitForRom())
	{
		return trueblnr;
	}
	return falseblnr;
}

LOCALPROC UnInitOSGLU(void)
{
	/*
		OSGLUxxx common:
		Do all clean ups needed before the program quits.
	*/

	if (MacMsgDisplayed) {
		MacMsgDisplayOff();
	}

	RestoreKeyRepeat();
#if MySoundEnabled
	MySound_Stop();
#endif
#if MySoundEnabled
	MySound_UnInit();
#endif
#if IncludePbufs
	UnInitPbufs();
#endif
	UnInitDrives();

	ForceShowCursor();

#if dbglog_HAVE
	dbglog_close();
#endif

	UnallocMyMemory();

	CheckSavedMacMsg();

	CloseMainWindow();
}

int minivmac_main(int argc, char **argv)
{
	my_argc = argc;
	my_argv = argv;

	ZapOSGLUVars();

	if (InitOSGLU()) {
		ProgramMain();
	}
	UnInitOSGLU();

	return 0;
}
