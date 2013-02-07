/*!
 *  @file AddImageFrames.h
 *  QTImage2Mov
 *
 *  Created by René J.V. Bertin on 20100713.
 *  Copyright 2010 INRETS / RJVB. All rights reserved.
 *
 */

#ifndef _ADD_IMG_FRAMES_H

//#define	IMPORT_VOD
//#define	IMPORT_JPGS

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#if defined(__DOXYGEN__)
// we're being scanned by doxygen: define some tokens that would get defined by headers
// that doxygen won't find, on this platform:
#	ifdef __APPLE_CC__
#		define TARGET_OS_MAC	1
#	else
#		define WIN32
#		define __WIN32__
#		define _WINDOWS_
#		define TARGET_OS_WIN32	1
#	endif
#endif

#if __APPLE_CC__
#	define OFFSET	off_t
#	define FTELL(fp)	ftello((fp))
#	define FSEEK(fp,offset,whence)	fseeko((fp), (OFFSET)(offset), (whence))
#elif defined(_MSC_VER)
#	define OFFSET	__int64
#	define FTELL(fp)	_ftelli64((fp))
#	define FSEEK(fp,offset,whence)	_fseeki64((fp), (OFFSET)(offset), (whence))
#else
#	define OFFSET	long
#	define FTELL(fp)	ftell((fp))
#	define FSEEK(fp,offset,whence)	fseek((fp), (OFFSET)(offset), (whence))
#endif

/*!
	constants that will be initialised with the proper Quicktime types, so that the parsing code
	does not need to include Quicktime-specific headers (ImageCompression.h, to be exact):
 */
extern unsigned long imgJPEGType, imgJPEG2000Type, imgPNGType, imgBMPType, imgTIFFType, imgSGIType, imgRAWType,
	imgSeqMP4Type;	//!< MPEG4 VOD files contain H.263 data but has to be identified as kMPEG4VisualCodecType
// incomplete support:
extern unsigned long imgGIFType, imgPDFType;

/*!
	read frame to retrieve the necessary information about images to be added
	to the movie we're constructing
 */
typedef struct imgFrames {
	long long imgID;		//!< img frame number in the source file
	unsigned long imgType;	//!< the image type. This should not change within a single sequence!
	long imgLen;			//!< byte length of the img frame
	short imgFrames;		//!< the number of frames in this image (typically 1)
	unsigned short imgWidth, imgHeight, imgDepth, imgXRes, imgYRes;	//!< img frame dimensions and resolution
	OFFSET imgOffset;		//!< file offset where the img image data starts (over imgLen bytes)
	double imgDuration;		//!< presentation duration for this img frame, in seconds
	double recordingStartTime, imgTime;	//!< (wallclock) starting time of the recording and the image recording time
	char frameStampString[256];			//!< text to stamp this image with (time, GPS data, ...)
	int frameStampBold;					//!< should the frame stamp be displayed in bold?
	char chapterTitle[256];				//!< if non-empty and theImage->newChapter=TRUE, a new chapter entry is added and the string emptied.
	struct QI2MSourceFileRecord *destFile;	//!< a pointer to the descriptor of the Movie we're importing into, for instance so that
									//!< client code (ReadNextIMGFrame methods) can add appropriate metadata
									//!< NB: typically an incomplete type in the client modules!
									//!<
	char *srcDesc;			//!< can point to a character string of max. 31 bytes to descripe the input format.
	short isKeyFrame,		//!< whether to use this image frame as a QuickTime "keyframe"
		syncTimeCode;		//!< when set, a new TimeCode track 'segment' is started, to improve time synchronisation for
						//!< non-integer frameRates.
	void *fragmentBuf;		//!< can point to an in-memory copy of an MP4 fragment. Managed by the format plugins!
	size_t fragmentLength;
} imgFrames;

/*!
	an opaque type to represent a generic access structure/descriptor to an image sequence file.
	Each client module can intialise/reinterpret this as required
 */
typedef void* IMG_FILE;

/*!
	A structure containing the information (function pointers) Add_imgFrames_From_File() needs to
	open, close and monitor an image data file, as well as to read all image frames in succession.
 */
typedef struct IMG_FILE_IO {
	void *sourcePtr;		//!< if the image data file reader requires additional state information, <sourcePtr> can point
						//!< to a properly initialised record of the correct type. In this case, it will be handed to
						//!< <open>.
						//!<
#ifdef __MACTYPES__
	OSType sourceType;		//!< FourCC describing the source type.
#else
	unsigned int sourceType;		//!< FourCC describing the source type.
#endif
	IMG_FILE (*open)(const char *name, const char *mode, struct IMG_FILE_IO *jfio, ...);	//!< given a name, access mode and possibly additional arguments, open/initialise the
						//!< image source file, initialise what needs to be, and return a pointer to a record that the
						//!< the other functions expect.
						//!<
	int (*close)(IMG_FILE source, struct IMG_FILE_IO *jfio),
		(*rewind)(IMG_FILE source),
		(*eof)(IMG_FILE source),		//!< eof: check for end-of-file
		(*error)(IMG_FILE source);	//!< error: check for a file error condition
	int (*ReadNextIMGFrame)( struct IMG_FILE_IO *jfio, char *name, IMG_FILE source, imgFrames *jF );	//!< the function that will read each frame in succession, until encountering eof or an error
						//!< (in which case it should return False)
						//!<
	int hasAudio;			//!< whether or not the source file also has audio. Currently, audio is (to be) handled in a 2nd
						//!< pass, after the video data has been read.
						//!<
	long maxFramesToRead;	//!< if positive, number of image frames to read from the input file
	double amountToRead,	//!< currently ignored
		fractionRead,		//!< progress state variable
		frameRate, realFrameRate;	//!< specified/requested and observed framerates
	int log,				//!< whether or not to maintain a log file of operations
		frameRepetitions;	//!< number of times to repeat each frame - not yet used.
	FILE *fpLog;			//!< pointer to an optional logfile, opened and closed by Add_imgFrames_From_File().
	int (*cancelCheck)(void *data);
	void *cancelCheckData;
} IMG_FILE_IO;

#ifdef IMPORT_VOD
#	include "IMGFrameFromVOD.h"
#endif
#ifdef IMPORT_JPGS
#	include "IMGFrameFromJPGS.h"
#endif


#ifdef _QTIMAGE2MOV_H

/*!
	parse and add the data from a file with img images. This contains information about each image's dimensions
	and byte length of the compressed representation, the presentation duration, followed by the binary data.
	We loop through the file, invoking ReadNextIMGFrame() to read all valid frames, and construct a SampleTable
	that stores references to the file offsets where each frame can be found.
	At the end, the sample table is added to a temporary movie's track media, which is then inserted in the
	output movie. A TimeCode track is also generated (or updated, if an applicable track already exists),
	as well as a (unique) Chapter track.
	The function updates <theTime> to keep track of the output movie's total duration.
	@param	theFile	the destination Movie descriptor
	@param	theImage	the current input object (an image sequence such as a VOD file)
	@param	theURL	the URL (filename) of the current input object
	@param	jfio		input file descriptor pre-initialised with the appropriate methods and parameters
	@param	theTime	the current time cursor into the destination Movie
	@param	startTime	absolute (wallclock) starting time
 */
extern OSErr Add_imgFrames_From_File( QI2MSourceFile theFile, QI2MSourceImage theImage, char *theURL,
						 IMG_FILE_IO *jfio, long maxFrames, TimeValue *theTime, TimeValue *startTime );

#endif

/*!
	invokes the interrupt checking method if theFile has one, and returns userCanceledErr if
	the user requested to interrupt the ongoing import.
 */
extern short CheckUserInterrupt( struct QI2MSourceFileRecord *theFile );

#define _ADD_IMG_FRAMES_H
#endif
