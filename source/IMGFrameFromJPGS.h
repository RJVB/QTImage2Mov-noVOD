/*!
 *  @file IMGFrameFromJPGS.h
 *  QTImage2Mov
 *
 *  Created by René J.V. Bertin on 20100713.
 *  Copyright 2010 INRETS / RJVB. All rights reserved.
 *
 */

#ifndef _IMGFRAMEFROMJPGS_H

#ifndef _BRIGADE_H

#include <stdio.h>
#include <stdlib.h>
#ifdef _MSC_VER
#	include "_stdint_.h"
	// win32popen.c contains a popen version that does not open a visible console window when
	// called from a GUI process.
#	ifndef _WIN32POPEN_H
	extern FILE *popenEx( const char *expr, const char *mode, struct POpenExProperties *props );
	extern int pcloseEx(FILE *fp);
#	endif
#else
#	include <stdint.h>
//#	include <aio.h>
#endif
#include <sys/types.h>

/*!
	encapsulate file seek and tell functions, opting for 64bit versions where they exist:
	type FOFFSET, functions FFTELL and FFSEEK.
 */
#if __APPLE_CC__
#	define FOFFSET	off_t
#	define FFTELL(fp)	ftello((fp))
#	define FFSEEK(fp,offset,whence)	fseeko((fp), (FOFFSET)(offset), (whence))
#elif defined(_MSC_VER)
#	define FOFFSET	__int64
#	define FFTELL(fp)	_ftelli64((fp))
#	define FFSEEK(fp,offset,whence)	_fseeki64((fp), (FOFFSET)(offset), (whence))
#else
#	define FOFFSET	long
#	define FFTELL(fp)	ftell((fp))
#	define FFSEEK(fp,offset,whence)	fseek((fp), (FOFFSET)(offset), (whence))
#endif

/*!
	Additional information used handle to MPEG4 chunks. This is just a copy
	of the state structure defined in brigade.h
 */
typedef struct MPG4_Fragments {
	short valid;			//!< true if the parsing results are valid
	short canIncrementVOBid;
	short done;			//!< true if the fragment was parsed completely.
	struct {
		short frameNr;		//!< current demuxed frame
		uint64_t offset;	//!< current frame's offset w.r.t. fragmentBuf (i.e. add fileOffset)
		int byteLength;	//!< byte length of the current frame
		double duration;	//!< presentation duration of the current frame
		struct {
			uint8_t minutes, hours;
			uint8_t subsec,			//!< appears to be unused
				seconds;
		} time;
	} current;
	short width, height;	//!< pixel dimensions of the fragment
	double timeBase;		//!< the timeBase used in the fragment
	double totalDuration;
	void *fragmentBuf;		//!< the MPG4 fragment read from the file
	FOFFSET fileOffset;		//!< offset in the file
	size_t byteLength;		//!< fragment length in bytes
	void *ffDemuxer;		//!< points to an internal structure for demuxing the fragment using libavformat
						//!< this member is non-NULL as long as there are frames left to be demuxed
	const char *codecName;	//!< as returned by libavformat
	unsigned int codecTag;	//!< codec FourCC (should be 'mp4v')
} MPG4_Fragments;
#	define CLEAR_MPG4_FRAGMENT(x)	if((x)){ memset(x, 0, sizeof(MPG4_Fragments)); }
#endif // _BRIGADE_H

/*!
	local state information for importing JPGS files
 */
typedef struct JPGSSources {
	FILE *fp;					//!< the actual file pointer
	MPG4_Fragments mp4Fragment;	//!< for handling JPGS files with MPEG4 fragments.
} JPGSSources;

extern IMG_FILE _open_JPGSFile( const char *theURL, const char *mode, IMG_FILE_IO *jfio );
extern int _close_JPGSFile( IMG_FILE fs, IMG_FILE_IO *jfio );
extern int _rewind_JPGSFile( IMG_FILE fs );
extern int _eof_JPGSFile( IMG_FILE fs );
extern int _error_JPGSFile( IMG_FILE fs );

extern int ReadNextIMGFrame_JPGS( IMG_FILE_IO *jfio, char *theURL, IMG_FILE FP, imgFrames *jF );

#define _IMGFRAMEFROMJPGS_H
#endif
