/*!
 *  @file AddImageFrames.c
 *  QTImage2Mov
 *
 *  Created by René J.V. Bertin on 20100713.
 *  Copyright 2010 INRETS / RJVB. All rights reserved.
 *
 */

#include "winixdefs.h"
#include "copyright.h"
IDENTIFY("Add the frames from an image sequence file");

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#if ! defined(_WINDOWS) && !defined(WIN32) && !defined(_MSC_VER)
#	include <unistd.h>
#	include <libgen.h>
#	ifdef __APPLE_CC__
		// Mac OS X basename() can modify the input string when not in 'legacy' mode on 10.6
		// and indeed it does. So we use our own which doesn't, and also doesn't require internal
		// storage.
		static const char *lbasename( const char *url )
		{ const char *c = NULL;
			if( url ){
				if( (c =  strrchr( url, '/' )) ){
					c++;
				}
				else{
					c = url;
				}
			}
			return c;
		}
#		define basename(p)	lbasename((p))
#	endif
#else

	extern const char *basename( const char *url );
//	{ const char *c = NULL;
//		if( url ){
//			if( (c =  strrchr( url, '\\' )) ){
//				c++;
//			}
//			else{
//				c = url;
//			}
//		}
//		return c;
//	}

#endif
#if TARGET_OS_WIN32
#	include "Dllmain.h"
#	include "windows.h"
#	include "win32popen.h"
#endif

#if __APPLE_CC__
#	include <Carbon/Carbon.h>
#	include <QuickTime/QuickTime.h>
#else
#	include <ImageCodec.h>
#	include <TextUtils.h>
#	include <string.h>
#	include <Files.h>
#	include <Movies.h>
#	include <MediaHandlers.h>
#endif

#include "QTImage2Mov.h"
#include "timing.h"

#include "AddImageFrames.h"
#include "QTils.h"

#define xfree(x)	if((x)){ free((x)); (x)=NULL; }

unsigned long imgJPEGType, imgJPEG2000Type, imgPNGType, imgGIFType, imgBMPType, imgTIFFType, imgPDFType, imgSGIType, imgRAWType, imgSeqMP4Type;

#define	SEQMP4CODECTYPE	kMPEG4VisualCodecType /*kH263CodecType*/ 

#ifndef IMPORT_VOD
static void SetupPipeSignalHandler( int *gotPipeSignal )
{
#if defined(SIGPIPE) && defined(PTHREAD_ONCE_INIT)
	if( gotPipeSignal ){
		*gotPipeSignal = 0;
		pthread_once( &pipeSignalKeyCreated, createPipeSignalKey );
		pthread_setspecific( pipeSignalKey, gotPipeSignal );
		signal( SIGPIPE, PipeSignalH );
	}
	else{
		signal( SIGPIPE, SIG_DFL );
	}
#else
	if( gotPipeSignal ){
		*gotPipeSignal = 0;
	}
#endif
}
#endif //IMPORT_VOD

char *concat2(char *string, ...)
{ va_list ap;
  int n= 0, tlen= 0;
  char *c= string, *buf= NULL, *first= NULL;
	va_start(ap, string);
	do{
		if( !n ){
			first= c;
		}
		if( c ){
			tlen+= strlen(c);
		}
		n+= 1;
	}
	while( (c= va_arg(ap, char*)) != NULL || n== 0 );
	va_end(ap);
	if( n== 0 || tlen== 0 ){
		return( NULL );
	}
	tlen+= 4;
	if( first ){
		buf= realloc( first, tlen* sizeof(char) );
	}
	else{
		buf= first= calloc( tlen, sizeof(char) );
	}
	if( buf ){
		va_start(ap, string);
		  /* realloc() leaves the contents of the old array intact,
		   \ but it may no longer be found at the same location. Therefore
		   \ need not make a copy of it; the first argument can be discarded.
		   \ strcat'ing is done from the 2nd argument.
		   */
		while( (c= va_arg(ap, char*)) != NULL ){
			strcat( buf, c);
		}
		va_end(ap);
		buf[tlen-1]= '\0';
	}
	return( buf );
}

short CheckUserInterrupt( struct QI2MSourceFileRecord *theFile )
{ static double lastCheck = 0;
  double now = HRTime_Time();
	if( theFile ){
		if( !lastCheck || lastCheck > now || now - lastCheck >= 1 ){
			lastCheck = now;
			return (theFile->interruptRequested = SetProgressDialogStopRequested( theFile->progressDialog ) );
		}
		else{
			return theFile->interruptRequested;
		}
	}
	else{
		return 0;
	}
}

static TCTrackInfo *expand_TCInfo( TCTrackInfo *info, int N )
{
	if( info && N > 0 ){
		if( info->StartTimes ){
		  int i, oldN = info->N;
			if( (info->StartTimes = (double*) realloc( info->StartTimes, N * sizeof(double) ))
				&& (info->durations = (double*) realloc( info->durations, N * sizeof(double) ))
				&& (info->frequencies = (double*) realloc( info->frequencies, N * sizeof(double) ))
				&& (info->frames = (size_t*) realloc( info->frames, N * sizeof(size_t) ))
			){
				info->N = N;
				for( i = oldN ; i < info->N ; i++ ){
					info->StartTimes[i] = 0;
					info->durations[i] = 0;
					info->frequencies[i] = 0;
					info->frames[i] = 0;
				}
			}
			else{
				info = NULL;
			}
		}
		else{
			if( (info->StartTimes = (double*) calloc( N, sizeof(double) ))
				&& (info->durations = (double*) calloc( N, sizeof(double) ))
				&& (info->frequencies = (double*) calloc( N, sizeof(double) ))
				&& (info->frames = (size_t*) calloc( N, sizeof(size_t) ))
			){
				info->N = N;
			}
			else{
				info = NULL;
			}
		}
	}
	return info;
}

static TCTrackInfo *add_TCSegment( TCTrackInfo *info, double StartTime )
{ int i;
	if( info->N == 0 ){
		if( (info = expand_TCInfo( info, 2 ) ) ){
			info->current = i = 1;
		}
	}
	else{
		i = ++(info->current);
		if( i >= info->N ){
			info = expand_TCInfo( info, info->N * 2 );
		}
	}
	if( info ){
		info->StartTimes[i] = StartTime;
	}
	return info;
}

static TCTrackInfo *update_TCSegment( TCTrackInfo *info, double add_duration, double frequency, size_t add_frames )
{
	if( info && info->current >= 1 ){
	  int i = info->current;
		info->durations[i] += add_duration;
		info->frames[i] += add_frames;
		if( frequency > 0 ){
			info->frequencies[i] = frequency;
		}
		else{
			info->frequencies[i] = info->frames[i] / info->durations[i];
		}
	}
	return info;
}

void AddImgFramesAnnotations( QI2MSourceFile theFile, IMG_FILE_IO *jfio, QI2MSourceImage theImage, char *theURL,
					    ImageDescriptionHandle imgDesc, unsigned long numSamples, unsigned long frames, double duration,
					    long maxFrames, Boolean specifiedFrameRate, Boolean cancelled, char *pCommand  )
{ char infoStr[1024];
#	ifdef LOG_FRANCAIS
	char *cancelMsg[] = { "", "(importation interrompue) " };
#	else
	char *cancelMsg[] = { "", "(import interrupted) " };
#	endif
  long gRes;
  OSErr gErr = Gestalt(gestaltQuickTimeVersion, &gRes);
	if( gErr != noErr ){
		gRes = 0;
	}
	snprintf( infoStr, sizeof(infoStr), "QTImage2Mov - %s ; QT v%x.%x.%x %s",
		__DATE__,
		(gRes >> 24) & 0x00f, (gRes >> 20) & 0x00f, (gRes >> 16) & 0x00f,
#if defined(__MACH__) || defined(__APPLE_CC__)
		"Mac OS X"
#else
		"MS Windows"
#endif
	);
	AddMetaDataStringToCurrentTrack( theFile, akSoftware, infoStr, NULL );
	AddMetaDataStringToCurrentTrack( theFile, akWriter, "RJVB", NULL );
	AddMetaDataStringToCurrentTrack( theFile, akCopyRight, "(C) RJVB  - 2010 INRETS, 2011, 2012 IFSTTAR", NULL );
	AddMetaDataStringToCurrentTrack( theFile, akSource, theURL, NULL );
	// avoid division by 0:
	if( !duration ){
		duration = 1;
	}
#ifdef LOG_FRANCAIS
	snprintf( infoStr, sizeof(infoStr), "\"%s\" %s- type '%.4s', %lu sur %lu trames%s; %g sec @ %gHz",
		    basename(theURL), cancelMsg[cancelled], (imgDesc)? OSTStr((*imgDesc)->cType) : "????", numSamples, frames,
		    (maxFrames>0)? " (max demandé)" : "",
		    duration, numSamples/duration
	);
#else
	snprintf( infoStr, sizeof(infoStr), "\"%s\" %s- '%.4s' type, %lu of %lu frames%s; %g sec @ %gHz",
		    basename(theURL), cancelMsg[cancelled], (imgDesc)? OSTStr((*imgDesc)->cType) : "????", numSamples, frames,
		    ((maxFrames>0)? " (max requested)" : ""),
		    duration, numSamples/duration
	);
#endif
	if( specifiedFrameRate ){
	  size_t len = strlen(infoStr);
#ifdef LOG_FRANCAIS
		snprintf( &infoStr[len], sizeof(infoStr)-len, " (%gHz spécifié)", jfio->frameRate );
#else
		snprintf( &infoStr[len], sizeof(infoStr)-len, " (%gHz specified)", jfio->frameRate );
#endif
	}
	else{
#ifdef LOG_FRANCAIS
		strcat( infoStr, " (estiméé)" );
#else
		strcat( infoStr, " (estimated)" );
#endif
	}
	if( jfio->realFrameRate > 0 ){
	  size_t ilen = strlen(infoStr);
#ifdef LOG_FRANCAIS
		snprintf( &infoStr[ilen], sizeof(infoStr)-ilen, "\n\tfréquence réellement observée: %gHz",
			    jfio->realFrameRate
		);
#else
		snprintf( &infoStr[ilen], sizeof(infoStr)-ilen, "\n\treal observed frequency: %gHz",
			    jfio->realFrameRate
		);
#endif
	}
	if( pCommand ){
	  size_t ilen = strlen(infoStr);
		// prepare for adding ffmpeg command:
		snprintf( &infoStr[ilen], sizeof(infoStr)-ilen, "\n\tmp4 VOD conversion: " );
	}
#ifdef LOG_FRANCAIS
	AddMetaDataStringToCurrentTrack( theFile, akInfo, infoStr, "fr_FR" );
#else
	AddMetaDataStringToCurrentTrack( theFile, akInfo, infoStr, "en_GB" );
#endif
	if( pCommand ){
		// pCommand[0] is a space char on purpose:
		pCommand[0] = '\t';
#ifdef LOG_FRANCAIS
		AddMetaDataStringToCurrentTrack( theFile, akInfo, pCommand, "fr_FR" );
#else
		AddMetaDataStringToCurrentTrack( theFile, akInfo, pCommand, "en_GB" );
#endif
	}
}

//Technical Q&A QA1183
//What depth should I put in an Image Description?
//Section 2 - Where should information about the true depth get stored?
//
//When the image data in question is an uncompressed pixel array of a big-endian pixel format that QuickDraw supports (specifically, 1, 2, 4, 8, 16, 24, 32, 33, 34, 36, or 40), the cType field of the image description should be kRawCodecType and the depth field (as discussed above) should be the pixel format. For example, for 8-bit grayscale, the cType field should contain kRawCodecType and the depth field should contain 40.
//
//When the image data in question is an uncompressed pixel array of a non-QuickDraw pixel format, the cType field of the image description should be the pixel format code. For example, k2vuyPixelFormat ('2vuy'). In this case the depth field (as discussed above) should contain 24. For 16-bit grayscale, the cType field should contain k16GrayCodecType and the depth field should contain 40.
//
//When the image data in question is compressed, but has a special native pixel format that it can be decompressed directly into, the pixel format should be stored as a big-endian OSType in an image description extension of type kImageDescriptionColorSpace ('cspc'). The value of the cType field should reflect the way the image data was compressed.
//
//For example, a PNG file containing a 48-bit-per-pixel RGB image would be described by an image description with a cType of kPNGCodecType ('png '), a big-endian kImageDescriptionColorSpace ('cspc') image description extension containing the OSType k48RGBCodecType ('b48r') and a depth field (as discussed above) containing the value 24.
//
//If your application needs more detailed information about registered pixel formats, use ICMGetPixelFormatInfo.


// RJVB: parse and add the data from a file with img images. This contains information about each image's dimensions
// and byte length of the compressed representation, the presentation duration, followed by the binary data.
// We loop through the file, invoking ReadNextIMGFrame() to read all valid frames, and construct a SampleTable
// that stores references to the file offsets where each frame can be found.
// At the end, the sample table is added to a temporary movie's track media, which is then inserted in the
// output movie.
// The function updates <theTime> to keep track of the output movie's total duration.
OSErr Add_imgFrames_From_File( QI2MSourceFile theFile, QI2MSourceImage theImage, char *theURL,
			IMG_FILE_IO *jfio, long maxFrames, TimeValue *theTime, TimeValue *startTime )
{ OSErr err = -1, txtErr;
  IMG_FILE fp;
  imgFrames jF;
  // local copies:
  unsigned int imgmaxWidth, imgmaxHeight;
  // local copies!
  ImageDescriptionHandle imgDesc = nil;
  QTMutableSampleTableRef sampleTable = nil;
  QTSampleDescriptionID qtSampleDesc = (QTSampleDescriptionID) nil;
  unsigned long numSamples = 0, frames = 0;
  Boolean specifiedFrameRate;
  // local copy:
  TimeValue samplesDuration = 0;
  Track cTrack;
  TCTrackInfo TCInfo, *tcInfo = NULL;
  // some image formats require a sort of internal offset to be defined, relative to where
  // the image starts in the file:
  ByteCount internalOffset = 0;
  double lastProgressTime, startProgressTime;
  // for adding a text track:
  Track textTrack = nil;
  Media textMedia = nil;
  MediaHandler textMH = nil;
  TextDescriptionHandle textDesc = NULL;
  TimeValue textDuration = 0;
  unsigned long textSamples = 0;
  char *fragFName = NULL, *pCommand = NULL;
  FILE *ffFP = NULL;
  int gotPipeSignal;
  Movie fragMovie = nil;
  Track fragTrack[4] = {nil, nil, nil, nil}, importTrack[4] = {nil, nil, nil, nil};
  int importedTracks = 0;
  Boolean importsFromTempMedia = FALSE;
#if TARGET_OS_WIN32
  POpenExProperties popProps;
#endif

	if( !theFile || !jfio || !theImage || !theURL || !theTime ){
		return paramErr;
	}
	jfio->maxFramesToRead = maxFrames;
	if( jfio->log ){
	  int llen = strlen(theURL) + 4 + 2;
	  char *lname = (char*) calloc( llen, sizeof(char) );
		if( lname ){
			snprintf( lname, llen*sizeof(char), "%s.log", theURL );
			jfio->fpLog = fopen( lname, "w" );
			free(lname);
		}
	}

	memset( &TCInfo, 0, sizeof(TCInfo) );

	// 20110120: slot for storing the real, observed, frameRate:
	jfio->realFrameRate = 0;
	specifiedFrameRate = (jfio->frameRate > 0)? TRUE : FALSE;

	jfio->cancelCheck = CheckUserInterrupt;
	jfio->cancelCheckData = theFile;

	// Open the file using the provided method, or with the standard C routine:
	if( jfio->open ){
		fp = (*jfio->open)( theURL, "rb", jfio );
	}
	else{
		fp = (IMG_FILE) fopen( theURL, "rb" );
	}
	if( fp ){
	  short interval;
	  double recordingStartTime = 0;
	  TimeValue recordingStart = -1;
	  double duration;
	  Boolean done = FALSE, cancelled = FALSE, showProgress = FALSE;

		// values for the cType field. These are not equal to the kQTFileType constants, to make things easy!!
		// definitions are in ImageCompression.h .
		imgJPEGType = kJPEGCodecType;
		imgJPEG2000Type = kJPEG2000CodecType;
		imgPNGType = kPNGCodecType;
		imgBMPType = kBMPCodecType;
		imgTIFFType = kTIFFCodecType;
		imgSGIType = kSGICodecType;
		imgSeqMP4Type = SEQMP4CODECTYPE;
		// GIF import apparently requires more work, probably saving the 1st image data into a temp file
		// and obtaining a Quicktime-generated ImageDescription for it (which as a clut attached).
		imgGIFType = kGIFCodecType;
		// undocumented; doesn't actually appear to work (creates a blank movie)
		imgPDFType = 'pdf ';
		imgRAWType = kRawCodecType;

		err = noErr;
		memset( &jF, 0, sizeof(jF) );
		jF.imgID = -1;
		jF.recordingStartTime = -1;
		jF.destFile = theFile;
		cTrack = theFile->currentTrack;

		// check that we have set the timescale.
		if( !theFile->timeScale ){
//			if( jfio->sourceType != 'VOD ' || !((VODSources*) jfio->sourcePtr)->fp->hasMPG4 ){
				if( jfio->frameRate > 0 ){
					SetMovieTimeScale( theFile->theMovie, (TimeScale)(jfio->frameRate * 1000 + 0.5) );
				}
				theFile->timeScale = GetMovieTimeScale(theFile->theMovie);
//			}
//			else{
//				Log( qtLogPtr, "Add_imgFrames_From_File(): importing an MPG4 VOD file: delaying movie timeScale setting\n" );
//			}
		}

		if( theFile->MustUseTrack ){
			// just to be sure...
			theFile->currentTrack = theFile->theImageTrack;
		}
		else{
			theFile->currentTrack = NewMovieTrack( theFile->theMovie, Long2Fix(0), Long2Fix(0), kNoVolume );
		}
		theImage->theTrack = theFile->currentTrack;

		jF.frameStampString[0] = '\0';

		if( theImage->interval >= 0 ){
			interval = theImage->interval;
		}
		else{
			interval = 1;
		}

		startProgressTime = lastProgressTime = HRTime_Time();

#pragma mark ---loop through input file---
		// loop through the file, reading all image frames as they come and as long as EOF nor file errors are encountered:
		// ReadNextIMGFrame() should return leaving the input file pointing to the next record.
		while( !done && (*jfio->ReadNextIMGFrame)( jfio, theURL, fp, &jF ) && err == noErr ){

			if( jF.imgLen <= 0 ){
			  // 20100903: this should allow to account for non-visual frames (audio, etc):
				goto skip_next_frame;
			}
			else if( !interval || (frames % interval) ){
				frames += 1;
				goto skip_next_frame;
			}
			else{
				// adding only 1 out of every <interval> frames should not modify the playback speed!
				jF.imgDuration *= interval;
			}

			if( jF.fragmentBuf ){
				if( !fragFName ){
#pragma mark ---- open channel to ffmpeg
#if defined(_MSC_VER) || defined(WIN32) || defined(__MINGW32__)
				  const char *mode = "wb";
#else
				  const char *mode = "w";
#endif
				  const char *comm = " ffmpeg -y -v 1 -i - -threads auto";
					fragFName = tmpImportFileName( theURL, "mov" );
					if( !fragFName ){
						err = MemError();
						xfree(fragFName);
						xfree(pCommand);
						goto bail;
					}
					else{
						pCommand = concat2( pCommand, comm, NULL );
						if( theImage->ffmpSplit && jfio->sourceType == 'VOD ' ){
							pCommand = concat2( pCommand, " -filter_complex \"split=4 [a][b][c][d]; "
										    "[a]crop=360:288:0:0;[b]crop=360:288:360:0; "
										    "[c]crop=360:288:0:288;[d]crop=360:288:360:288\"", NULL );
						}
						if( theImage->ffmpBitRate && strlen((char*)theImage->ffmpBitRate) ){
							pCommand = concat2( pCommand, " -b:v ", theImage->ffmpBitRate, NULL );
						}
						if( theImage->ffmpCodec && strlen((char*)theImage->ffmpCodec) ){
							pCommand = concat2( pCommand, " -vcodec ", theImage->ffmpCodec, " -threads auto \"", fragFName, "\"", NULL );
 						}
						else{
							pCommand = concat2( pCommand, " -vcodec copy -threads auto \"", fragFName, "\"", NULL );
						}
#ifdef _MSC_VER
						memset( &popProps, 0, sizeof(popProps) );
						popProps.commandShell = "ffmpeg.exe";
						popProps.shellCommandArg = NULL;
						popProps.runPriorityClass = ABOVE_NORMAL_PRIORITY_CLASS;
						popProps.finishPriorityClass = HIGH_PRIORITY_CLASS;
						ffFP = popenEx( &pCommand[1], mode, &popProps );
#else
						ffFP = popen( &pCommand[1], mode );
#endif
						if( !ffFP ){
							err = (errno)? errno : EPIPE;
							Log( qtLogPtr, "Add_imgFrames_From_File(): error in popen(\"%s\") (%s)\n",
							    pCommand, strerror(err) );
#ifdef _MSC_VER
#	ifdef LOG_FRANCAIS
							PostWarningDialog( "Erreur de communication avec ffmpeg - pas disponible sur le PATH?", theURL );
#	else
							PostWarningDialog( "Communication error with ffmpeg - is it on the PATH?", theURL );
#	endif
#endif
							xfree(fragFName);
							xfree(pCommand);
							goto bail;
						}
						else{
							SetupPipeSignalHandler(&gotPipeSignal);
							Log( qtLogPtr, "Add_imgFrames_From_File(): popen(\"%s\") ok\n",
							    pCommand );
						}
					}
				}
				if( fragFName && ffFP ){
					fwrite( jF.fragmentBuf, 1, jF.fragmentLength, ffFP );
					if( gotPipeSignal ){
#ifdef _MSC_VER
						pcloseEx(ffFP);
#else
						pclose(ffFP);
#endif
						ffFP = NULL;
						err = (errno)? errno : EPIPE;
						xfree(fragFName);
						xfree(pCommand);
						goto bail;
					}
					// indicate downstream code that we're going to import from a temporary media file
					// that is currently being created. Note that once we start doing this, there's no going
					// back: any samples added will be discarded. Support for mixed files may be implemented
					// in a future revision but only if the need arises.
					importsFromTempMedia = TRUE;
				}
			}

			// construct a new, local ImageDescription handle if necessary
			if( imgDesc == nil ){
				imgDesc = (ImageDescriptionHandle) NewHandleClear(sizeof(ImageDescription));
				err = MemError();
				if( imgDesc ){
					(*imgDesc)->idSize = sizeof(ImageDescription);
					(*imgDesc)->dataRefIndex = 1;
					(*imgDesc)->clutID = -1;
					  // determined via GetMediaSampleReference()
					switch( jF.imgType ){
						case kBMPCodecType:
							// determined with Dumpster, from properly imported BMP images.
							internalOffset = 54;
						case kPNGCodecType:
						case kTIFFCodecType:
							(*imgDesc)->spatialQuality = codecLosslessQuality;
							break;
						case kSGICodecType:
							(*imgDesc)->spatialQuality = codecLosslessQuality;
							(*imgDesc)->version = 1;
							((*imgDesc)->name)[0] = 3;
							strcpy( (char*) &(((*imgDesc)->name)[1]), "SGI" );
							// determined with Dumpster, from properly imported SGI images.
							internalOffset = 512;
							break;
						case kGIFCodecType:
							(*imgDesc)->temporalQuality = codecNormalQuality;
							(*imgDesc)->spatialQuality = codecNormalQuality;
							// GIFs apparently need a clut that is stored "in" (= after) the ImageDescription...
							//(*imgDesc)->clutID = 0;
							break;
						case SEQMP4CODECTYPE:
							// 20121019: pure guessing for now.
							(*imgDesc)->temporalQuality = codecNormalQuality;
							(*imgDesc)->spatialQuality = codecNormalQuality;
							break;
					}
					if( jF.srcDesc && strlen(jF.srcDesc) < sizeof((*imgDesc)->name)-1 ){
						((*imgDesc)->name)[0] = strlen(jF.srcDesc);
						strcpy( (char*) &(((*imgDesc)->name)[1]), jF.srcDesc );
					}
				}
			}
			if( imgDesc ){
			 // store the current image's metadata in the description:
				(*imgDesc)->frameCount = jF.imgFrames;
				(*imgDesc)->cType = (!jF.imgType)? 'jpeg' : jF.imgType;
				(*imgDesc)->width = jF.imgWidth;
				(*imgDesc)->height = jF.imgHeight;
				(*imgDesc)->depth = jF.imgDepth;
				// no need to set the dataSize (but it doesn't hurt either)
				//(*imgDesc)->dataSize = jF.imgLen;
				(*imgDesc)->hRes = Long2Fix(jF.imgXRes);
				(*imgDesc)->vRes = Long2Fix(jF.imgYRes);
			}
			// create a new sample table if necessary:
			if( sampleTable == nil && err == noErr && !importsFromTempMedia ){
				err = (OSErr) QTSampleTableCreateMutable(NULL, theFile->timeScale, NULL, &sampleTable);
				if( err != noErr ){
					Log( qtLogPtr, "Add_imgFrames_From_File(): error creating SampleTable (%d)\n", err );
					sampleTable = nil;
				}
			}
#pragma mark ---add sample description---
			// as well as a new SampleDescription:
			if( sampleTable && !qtSampleDesc && !importsFromTempMedia ){
			  // if I add a qtSampleDesc for each sample, every jpeg frame I add seems to be added twice
			  // (while stepping through the movie) but the total duration will be correct?!
			  // It is apparently not necessary to create a sample description for each sample (note
			  // how it isn't even necessary to set the dataSize field!). This of course means that
			  // a single sample table expects images of identical dimensions — which corresponds nicely to
			  // the fact that it will go into a single track that will impose its dimensions on all frames
			  // it contains.
				err = (OSErr) QTSampleTableAddSampleDescription( sampleTable, (SampleDescriptionHandle) imgDesc, 0, &qtSampleDesc);
				if( err != noErr ){
					Log( qtLogPtr, "Add_imgFrames_From_File(): error creating qtSampleDesc (%d)\n", err );
				}
			}
			if( (sampleTable && qtSampleDesc) || importsFromTempMedia ){
			  SInt64 sampleNum;
			  TimeValue duration;
			  long additionalFlags;
#	if __APPLE_CC__
			  long QTSampleFlags;
				  // mediaSampleNotSync must NOT be set, or QT will beachball when trying to play the result movie
//				QTSampleFlags = mediaSampleDroppable|mediaSampleDoesNotDependOnOthers;
				QTSampleFlags = 0;
#	else
#		define	QTSampleFlags	0
#	endif

				if( numSamples == 0 ){
					txtErr = initTextTrackWithMedia( theFile, &textTrack, &textMedia,
									  Long2Fix(jF.imgWidth), Long2Fix(16), "timeStamp Track", &textDesc
					);
					if( txtErr == noErr ){
						txtErr = BeginMediaEdits(textMedia);
					}
					if( txtErr != noErr ){
						*textMedia = nil;
						Log( qtLogPtr, "error doing BeginMediaEdits(textMedia): %d\n", txtErr );
					}
					else{
						textMH = GetMediaHandler(textMedia);
					}
					additionalFlags = 0;
				}
				else if( jF.isKeyFrame ){
					// 20110120: mark a keyframe:
					additionalFlags |= mediaSampleNotSync;
				}
				else{
					// for the time being there are no additional flags to be set/maintained here
					additionalFlags = 0;
				}
//				additionalFlags |= (mediaSampleDoesNotDependOnOthers);

				if( TCInfo.current == 0 ){
					tcInfo = add_TCSegment( &TCInfo, jF.recordingStartTime );
					if( tcInfo ){
						tcInfo = update_TCSegment( tcInfo, jF.imgDuration, -1, 1 );
					}
				}
				else if( tcInfo ){
					if( jF.syncTimeCode ){
						tcInfo = add_TCSegment( &TCInfo, jF.imgTime );
						if( tcInfo ){
							tcInfo = update_TCSegment( tcInfo, jF.imgDuration, jfio->frameRate, 1 );
						}
						jF.syncTimeCode = FALSE;
					}
					else{
						tcInfo = update_TCSegment( tcInfo, jF.imgDuration, -1, 1 );
					}
				}

				// the frame's duration is specified in the movie's timescale, which is 600 per second by default:
				duration = (TimeValue)(jF.imgDuration * theFile->timeScale + 0.5);
				duration = max(1,duration);
#pragma mark ---add sample reference---
				if( !importsFromTempMedia ){
					// add a media sample reference to our movie; the offset
					// is the file position where the image starts.
					err = (OSErr) QTSampleTableAddSampleReferences(sampleTable,
							(SInt64) (jF.imgOffset+internalOffset), jF.imgLen-internalOffset, duration,
							0, 1, QTSampleFlags|additionalFlags,
							qtSampleDesc, &sampleNum
					);
				}
//				if( jF.imgFrames != 1 ){
//					qtSampleDesc = (QTSampleDescriptionID) nil;
//				}
				if( err != noErr ){
					if( importsFromTempMedia ){
						Log( qtLogPtr, "Add_imgFrames_From_File(): unexpected error preparing a temporary input media file (%d)\n", err );
					}
					else{
						Log( qtLogPtr, "Add_imgFrames_From_File(): error adding sample reference to table (%d)\n", err );
					}
					goto bail;
				}
				else{
					samplesDuration += duration;
					if( numSamples ){
						if( jF.imgWidth > imgmaxWidth ){
							imgmaxWidth = jF.imgWidth;
						}
						if( jF.imgHeight > imgmaxHeight ){
							imgmaxHeight = jF.imgHeight;
						}
					}
					else{
						imgmaxWidth = jF.imgWidth;
						imgmaxHeight = jF.imgHeight;
					}
					numSamples += 1 /*jF.imgFrames*/;
				}
#pragma mark ---- add chapterTitle and/or frameStamp ----
				if( jF.chapterTitle[0] && theImage->newChapter ){
					AddChapterTitle( theFile,
								 (theFile->theTCTrack)? theFile->theTCTrack : theImage->theTrack,
								 (const char*) jF.chapterTitle, samplesDuration - duration, 1
					);
					jF.chapterTitle[0] = '\0';
				}
				if( textMedia && jF.frameStampString[0] ){
				  UInt16 strLength;
				  RGBColor bgCol = {0xeeee, 0xf2f2, 0xe1e1}, fgCol = {0,0x1111,0};
				  TimeValue txtTime;
					strLength = strlen(jF.frameStampString);
					txtErr = TextMediaAddTextSample( textMH, jF.frameStampString, strLength,
									   4, 0, (jF.frameStampBold)? bold : normal,
									   &fgCol, &bgCol,
									   (**textDesc).textJustification, &((**textDesc).defaultTextBox),
									   (**textDesc).displayFlags, 0, 0, 0, NULL,
									   duration, &txtTime
					);
					if( txtErr == noErr ){
						textDuration += duration;
						textSamples += 1;
					}
				}
			}
			frames += 1 /*jF.imgFrames*/;
#pragma mark --- frame done, on to the next one---
skip_next_frame:
			// one never knows...:
			jF.destFile = theFile;
			if( (*jfio->eof)(fp) ){
				err = eofErr;
			}
// 20101008: feof() and ferror() can be expensive, so leave the file error checking to the code that does the actual
// file IO!
//			else if( (*jfio->error)(fp) ){
//				err = ioErr;
//			}
			if( maxFrames > 0 && numSamples >= maxFrames ){
				done = 1;
			}
#pragma mark --video progress indication---
			{ double now = HRTime_Time(), fractionRead;
				if( (now - lastProgressTime) >= 5 ){
					if( jfio->maxFramesToRead > 0 ){
						fractionRead = ((double)jF.imgID)/((double)jfio->maxFramesToRead);
						Log( qtLogPtr, "\"%s\": %.3g%% %lu smp; t|dt=%.3g|%.3gs; %.3g total\n",
							theURL, fractionRead * 100.0, numSamples, now - startProgressTime, now - lastProgressTime,
							theFile->currentMMElement + fractionRead
						);
					}
					else{
						fractionRead = jfio->fractionRead;
						Log( qtLogPtr, "\"%s\": %.3g%% %lu smp; t|dt=%.3g|%.3gs; %.3g total\n",
							theURL, fractionRead * 100.0, numSamples, now - startProgressTime, now - lastProgressTime,
							theFile->currentMMElement + fractionRead
						);
					}
					lastProgressTime = now;
					showProgress = FALSE;
					SetProgressDialogValue( theFile->progressDialog, theFile->currentMMElement + fractionRead, TRUE );
				}
				if( (now - lastProgressTime) >= 0.75 ){
					showProgress = TRUE;
				}
				if( showProgress ){
					if( jfio->maxFramesToRead > 0 ){
						fractionRead = ((double)jF.imgID)/((double)jfio->maxFramesToRead);
					}
					else{
						fractionRead = jfio->fractionRead;
					}
					SetProgressDialogValue( theFile->progressDialog, theFile->currentMMElement + fractionRead, FALSE );
				}
			}
			err = SetProgressDialogStopRequested( theFile->progressDialog );
		}
#pragma mark ---done looping through input file---
		if( theFile->interruptRequested ){
			if( err == noErr || err == userCanceledErr ){
				err = userCanceledErr;
			}
			cancelled = 1;
		}
		// a user cancel is not handled as a hard error, so we cache the fact and
		// unset the error indicator.
		if( err == userCanceledErr ){
			cancelled = 1;
			err = noErr;
		}
		else if( err == eofErr ){
		  // not an error...
			err = noErr;
		}
		if( maxFrames > 0 ){
			Log( qtLogPtr, "Add_imgFrames_From_File(): stored %lu of requested %lu of %lu samples from \"%s\"\n",
			    numSamples, maxFrames, frames, theURL
			);
		}
		else{
			Log( qtLogPtr, "Add_imgFrames_From_File(): stored %lu of %lu samples from \"%s\"\n",
				numSamples, frames, theURL
			);
		}

#pragma mark ---add sound---
		if( jfio->hasAudio && jfio->rewind ){
			if( (*jfio->rewind)(fp) > 0 ){
				Log( qtLogPtr, "Add_imgFrames_From_File(): there is audio to be imported some day (real[ly not very]) soon!\n" );
			}
		}

#pragma mark --bail---
// 20110120: moved up here; was just after the MD adding.
bail:
		// the file can now be closed:
		if( jfio->close ){
			// NB: we do not know what jfio->close does with fp, so it should not
			// be referenced anymore after closing!!
			(*jfio->close)(fp, jfio);
			fp = NULL;
		}
		else{
			fclose(fp);
			fp = NULL;
		}

		// close the pipe to the external command creating the temporary import media
		if( ffFP ){
		  int r;
			Log( qtLogPtr, "Add_imgFrames_From_File(): waiting for ffmpeg to terminate\n" );
#ifdef _MSC_VER
			r = pcloseEx(ffFP);
			if( r ){
				Log( qtLogPtr, "Add_imgFrames_From_File(): command \"%s\" terminated with exit code %d; processing times %g+%gs\n",
				    pCommand, r, popProps.times.user, popProps.times.kernel );
			}
			else{
				Log( qtLogPtr, "Add_imgFrames_From_File(): ffmpeg done; processing times %g+%gs\n",
				    popProps.times.user, popProps.times.kernel );
			}
#else
			r = pclose(ffFP);
			if( r ){
				Log( qtLogPtr, "Add_imgFrames_From_File(): command \"%s\" terminated with exit code %d\n", pCommand, r );
			}
#endif
		}
		if( importsFromTempMedia && err == noErr ){
			err = NewMovieFromURL( &fragMovie, 0, NULL, fragFName, NULL, NULL );
			if( err == noErr ){
			  int ft;
				SetMovieProgressProc( theFile->theMovie, (MovieProgressUPP)-1L, 0);
				SetMovieProgressProc( fragMovie, (MovieProgressUPP)-1L, 0);
				SetMovieTimeScale( fragMovie, theFile->timeScale );
				UpdateMovie(fragMovie);
				Log( qtLogPtr, "Add_imgFrames_From_File(): importing an MPG4 VOD file through a temporary import media with timeScale %ld\n", theFile->timeScale );
				fragTrack[0] = GetMovieIndTrackType( fragMovie, 1, FOUR_CHAR_CODE('vfrr'),
								 movieTrackCharacteristic | movieTrackEnabledOnly);
				err = GetMoviesError();
				if( theImage->ffmpSplit && jfio->sourceType == 'VOD ' ){
					if( GetMovieTrackCount(fragMovie) < 4 ){
						err = invalidMovie;
					}
					for( ft = 1 ; ft < 4 && err == noErr ; ft++ ){
						fragTrack[ft] = GetMovieIndTrackType( fragMovie, ft+1, FOUR_CHAR_CODE('vfrr'),
										 movieTrackCharacteristic | movieTrackEnabledOnly);
						if( (err = GetMoviesError()) == noErr ){
							CopyTrackSettings( fragTrack[ft], theFile->theImageTrack );
							samplesDuration = ((double)GetTrackDuration(fragTrack[ft])) / ((double)GetMovieTimeScale(fragMovie)) * theFile->timeScale;
							frames = numSamples = GetMediaSampleCount( GetTrackMedia(fragTrack[ft]) );
						}
						else{
							Log( qtLogPtr,
							    "Add_imgFrames_From_File(): Error getting video track %d from the temporary import media \"%s\" (%d/%d)\n",
								fragFName, ft+1, err, GetMoviesError() );
						}
					}
				}
				if( err == noErr ){
				  Fixed w, h;
					// we discard the dimension information passed on by the reader plugin, and get the dimensions
					// directly from the import movie. In split import these dimensions are supposed to be identical
					// across all tracks, so using the values for the 1st track will scale the 3 others if required to
					// validate that assumption.
					GetTrackDimensions( fragTrack[0], &w, &h );
					imgmaxWidth = Fix2Long(w), imgmaxHeight = Fix2Long(h);
					CopyTrackSettings( fragTrack[0], theFile->theImageTrack );
					samplesDuration = ((double)GetTrackDuration(fragTrack[0])) / ((double)GetMovieTimeScale(fragMovie)) * theFile->timeScale;
					frames = numSamples = GetMediaSampleCount( GetTrackMedia(fragTrack[0]) );
				}
				else{
					Log( qtLogPtr, "Add_imgFrames_From_File(): Error getting video track 1 from the temporary import media \"%s\" (%d/%d)\n", fragFName, err, GetMoviesError() );
				}
			}
			else{
				Log( qtLogPtr, "Add_imgFrames_From_File(): Error opening temporary import media \"%s\" (%d/%d)\n", fragFName, err, GetMoviesError() );
			}
		}

		if( !theImage->ignoreRecordStartTime && jF.recordingStartTime >= 0 ){
			recordingStartTime = jF.recordingStartTime;
			recordingStart = (TimeValue) (recordingStartTime * theFile->timeScale + 0.5);
		}

		duration = samplesDuration / ((double) theFile->timeScale);

#pragma mark ---add the samples to the current movie track---
		// if any image frames were read, now's the time to add them to the movie!
		if( numSamples && err == noErr ){
		  int ft = 0;
			if( theFile->movieWidth == 0 ){
				theFile->movieWidth = imgmaxWidth;
			}
			if( theFile->movieHeight == 0 ){
				theFile->movieHeight = imgmaxHeight;
			}
			theImage->discoveredWidth = Long2Fix(imgmaxWidth);
			theImage->discoveredHeight = Long2Fix(imgmaxHeight);
			if( theImage->ffmpSplit && jfio->sourceType == 'VOD ' ){
				// the split tracks will be laid out such that the movie dimensions are those of the original input content!
				theFile->movieWidth *= 2, theFile->movieHeight *= 2;
				theImage->discoveredWidth *= 2, theImage->discoveredHeight *= 2;
			}
			// fetch the imposed track or create a new one, set the dimensions:
ffmpSplitNextTrack:
			if( err == noErr && (!theFile->MustUseTrack || theFile->theImageTrack == nil) ){
				if( theFile->currentTrack ){
					SetTrackDimensions( theFile->currentTrack, Long2Fix(imgmaxWidth), Long2Fix(imgmaxHeight) );
					theImage->theTrack = theFile->theImageTrack = theFile->currentTrack;
				}
				else{
					theImage->theTrack = theFile->theImageTrack = NewMovieTrack( theFile->theMovie,
											Long2Fix(imgmaxWidth), Long2Fix(imgmaxHeight), kNoVolume
					);
				}
			}
#pragma mark ---add metadata---
			if( err == noErr ){
				theFile->currentTrack = theFile->theImageTrack;
				AddImgFramesAnnotations( theFile, jfio, theImage, theURL, imgDesc, numSamples, frames, duration, maxFrames, specifiedFrameRate, cancelled, pCommand );
			}
			theFile->currentTrack = nil;
			if( theFile->theImageTrack != nil ){
				// create a track media, using the dataRef to the file obtained above
//				theFile->theImage.Media = NewTrackMedia( theFile->theImageTrack, VideoMediaType,
//								    GetMovieTimeScale(theFile->theMovie), cDataRef, cDataRefType
//				);
				importTrack[ft] = theFile->theImageTrack;
				importedTracks += 1;
				if( importsFromTempMedia ){
					// if we have a destination dataRef, use that to create a track, so that there will
					// be no ambiguities as to the resource on which the imported track depends:
					if( theFile->odataRef ){
					  OSType fragTrackType;
						theFile->theImage.dataRef = theFile->odataRef;
						theFile->theImage.dataRefType = theFile->odataRefType;
						GetMediaHandlerDescription( GetTrackMedia(fragTrack[ft]), &fragTrackType, nil, nil);
						theFile->theImage.Media = NewTrackMedia( theFile->theImageTrack, fragTrackType,
														GetMovieTimeScale(theFile->theMovie),
														theFile->odataRef, theFile->odataRefType );
						if( !theFile->theImage.Media ){
							err = GetMoviesError();
						}
						else{
							Log( qtLogPtr, "Add_imgFrames_From_File(): created '%.4s' media for \"%s\"\n", OSTStr(fragTrackType), theFile->ofName );
						}
					}
					else{
						// no such luck, create a pure in-memory track:
						err = NewImageMediaForMemory( theFile, theFile->theImageTrack, VideoMediaType );
					}
				}
				else{
					err = NewImageMediaForURL( theFile, theFile->theImageTrack, VideoMediaType, theURL );
				}
				if( err == noErr && theFile->theImage.Media != nil ){
					if( sampleTable ){
						BeginMediaEdits( theFile->theImage.Media );
						err = AddSampleTableToMedia( theFile->theImage.Media, sampleTable, 1, (unsigned int) numSamples, NULL);
						EndMediaEdits( theFile->theImage.Media );
					}
					if( err != noErr ){
						Log( qtLogPtr, "Add_imgFrames_From_File(): Error adding sample table to media (%d/%d)\n", err, GetMoviesError() );
						if( !theFile->MustUseTrack ){
							DisposeTrackMedia( theFile->theImage.Media );
							DisposeMovieTrack( theFile->theImageTrack );
							theFile->theImageTrack = nil;
						}
					}
					else{
					  TimeValue trackStart, appendTime;
						if( ft == 0 ){
							if( recordingStart >= 0 ){
								trackStart = recordingStart;
								if( (theFile->startTime < 0)
								   // 20101116: not sure theFile->startTime has to be kept updated...!
								   // || (recordingStart < theFile->startTime)
								){
									theFile->startTime = recordingStart;
								}
								SetMovieStartTime( theFile->theMovie, trackStart, (theFile->numberMMElements == 1) );
								// we add a TimeCode track to the movie instead of inserting the video data
								// at an offset corresponding to the recording wallclock start time:
								if( !theImage->timePadding ){
									// 20101116: half-clever guess of when we have to set trackStart to 0...
									// for instance the 1st time around, and when we are reading the same file again
									// (recordingStart could be equal to theFile->startTime)
									if( (theFile->numTracks == 0 || recordingStart == theFile->startTime) ){
										trackStart = 0;
									}
									else{
										trackStart = *theTime;
									}
								}
							}
							else{
								trackStart = (theFile->MustUseTrack)? -1 : *theTime;
							}
							if( trackStart == -1 ){
								appendTime = GetTrackDuration( theFile->theImageTrack );
							}
						}
						if( importsFromTempMedia && !sampleTable ){
							Log( qtLogPtr, "Add_imgFrames_From_File(): inserting temporary import media \"%s\" into track #%d\n", fragFName, ft );
							BeginMediaEdits( theFile->theImage.Media );
							err = InsertTrackSegment( fragTrack[ft], theFile->theImageTrack, 0, GetTrackDuration(fragTrack[ft]), trackStart );
							EndMediaEdits( theFile->theImage.Media );
						}
						else{
							err = InsertMediaIntoTrack( theFile->theImageTrack, trackStart, 0, samplesDuration, fixed1);
						}
						if( err != noErr ){
							if( importsFromTempMedia ){
								Log( qtLogPtr, "Add_imgFrames_From_File(): Error inserting temporary import media into track (%d)\n",
								    err );
							}
							else{
								Log( qtLogPtr, "Add_imgFrames_From_File(): Error inserting media into track (%d)\n", err );
							}
							if( !theFile->MustUseTrack ){
								DisposeTrackMedia( theFile->theImage.Media );
								DisposeMovieTrack( theFile->theImageTrack );
								theFile->theImageTrack = nil;
							}
						}
						else{
							// non-logical jump-back to track-insertion here, when theImage->ffmpSplit
							if( theImage->ffmpSplit && jfio->sourceType == 'VOD ' && ft < 3 && err == noErr ){
								ft += 1;
								if( fragTrack[ft] ){
									UpdateMovie(theFile->theMovie);
									theFile->theImageTrack = nil;
									if( (err = SetProgressDialogStopRequested( theFile->progressDialog )) == noErr ){
										goto ffmpSplitNextTrack;
									}
								}
							}
							if( trackStart != -1 ){
								if( !theFile->PosterIsSet ){
									SetMoviePosterTime( theFile->theMovie, trackStart );
									theFile->PosterIsSet = TRUE;
								}
							}
							else{
								trackStart = appendTime;
							}
							// the media with our images was inserted; update the movie and ensure
							// that the upper-left corner is at (0,0):
							UpdateMovie( theFile->theMovie );
							// 20110125: DO NOT update *theTime by trackStart AND samplesDuation
							*theTime += /*trackStart +*/ samplesDuration;
							*startTime = trackStart;
							AnchorMovie2TopLeft( theFile->theMovie );
							if( !theImage->ignoreRecordStartTime && tcInfo ){
							  double startTimes[2], durations[2], frequencies[2];
							  size_t Nframes[2];
								if( theImage->timePadding ){
									tcInfo->StartTimes[0] = 0;
									tcInfo->durations[0] = recordingStartTime;
									tcInfo->frequencies[0] = jfio->frameRate;
									tcInfo->frames[0] = (size_t) (recordingStartTime*numSamples/duration);
									// finetune!
									tcInfo->StartTimes[1] = recordingStartTime;
									err = AddTimeCodeTrack( theFile, theImage, 0,
										tcInfo->current+1, tcInfo->StartTimes, tcInfo->durations,
										(jfio->frameRate>0)? tcInfo->frequencies : NULL, tcInfo->frames,
										theURL
									);
									if( err != noErr ){
										Log( qtLogPtr,
										    "Add_imgFrames_From_File(): Error adding precise TimeCode track (%d), retrying\n",
										    err
										);
										startTimes[1] = recordingStartTime;
										durations[1] = duration;
										frequencies[1] = jfio->frameRate;
										Nframes[1] = numSamples;
										// 20101118: the TC track should be inserted at time=0 in this case!
										err = AddTimeCodeTrack( theFile, theImage, 0,
											2, startTimes, durations,
											(jfio->frameRate>0)? frequencies : NULL, Nframes,
											theURL
										);
									}
								}
								else{
									// finetune!
									tcInfo->StartTimes[1] = recordingStartTime;
									err = AddTimeCodeTrack( theFile, theImage, trackStart,
										tcInfo->current, &tcInfo->StartTimes[1], &tcInfo->durations[1],
										(jfio->frameRate>0)? &tcInfo->frequencies[1] : NULL, &tcInfo->frames[1],
										theURL
									);
									if( err != noErr ){
										Log( qtLogPtr,
										    "Add_imgFrames_From_File(): Error adding precise TimeCode track (%d), retrying\n",
										    err
										);
										startTimes[0] = recordingStartTime;
										durations[0] = duration;
										frequencies[0] = jfio->frameRate;
										Nframes[0] = numSamples;
										// 20101118: the TC track should be inserted at time=trackStart in this case!
										// (that should still correspond to recordingStartTime...)
										err = AddTimeCodeTrack( theFile, theImage, trackStart,
											1, startTimes, durations,
											(jfio->frameRate>0)? frequencies : NULL, Nframes,
											theURL
										);
									}

								}
								if( err != noErr ){
									Log( qtLogPtr, "Add_imgFrames_From_File(): Error creating TimeCode track (%d)\n", err );
								}
								else{
									if( jfio->fpLog && theFile->theTCTrack ){
#pragma mark ----TCTrack logging----
									  TimeValue startTime;
									  MediaHandler mh;
									  long frameNr;
									  TimeCodeDef tcdef;
									  TimeCodeRecord tcrec;
									  HandlerError e;
									  long i;
										QTStep_GetStartTimeOfFirstVideoSample( theFile->theMovie, &startTime );
										mh = GetMediaHandler( GetTrackMedia( theFile->theTCTrack ) );
										if( mh && GetMoviesError() == noErr ){
											e = TCGetTimeCodeAtTime( mh, (TimeValue) startTime, &frameNr, &tcdef, &tcrec, NULL );
											if( e == noErr && !(tcdef.flags & tcCounter) ){
												fprintf( jfio->fpLog,
													"TimeTrack info for URL=\"%s\"; %lu frames starting at %ld = %02d:%02d:%02d;%d TC frameRate=%g(%d)Hz;\n",
													theURL, numSamples, frameNr,
													tcrec.t.hours, tcrec.t.minutes, tcrec.t.seconds, tcrec.t.frames,
													((double)tcdef.fTimeScale) / ((double)tcdef.frameDuration), tcdef.numFrames
												);
												for( i = 1; i < numSamples ; i++ ){
													e = TCFrameNumberToTimeCode( mh, frameNr+i, &tcdef, &tcrec );
													if( e == noErr ){
														fprintf( jfio->fpLog,
															"\tframe %ld = %02d:%02d:%02d;%d TC frameRate=%g(%d)Hz;\n",
															i,
															tcrec.t.hours, tcrec.t.minutes, tcrec.t.seconds, tcrec.t.frames,
															((double)tcdef.fTimeScale) / ((double)tcdef.frameDuration), tcdef.numFrames
														);
													}
												}
											}
										}
									}
								}
							}
							if( textMedia ){
#pragma mark ---- insert textTrack ----
								txtErr = EndMediaEdits(textMedia);
								if( txtErr == noErr ){
									if( textSamples ){
										txtErr = InsertMediaIntoTrack( textTrack, trackStart, 0,
												GetMediaDuration(textMedia), fixed1
										);
										if( txtErr == noErr ){
											SetTrackEnabled( textTrack, FALSE );
											if( !theImage->hideTS ){
												{ MatrixRecord theTextMatrix;
												  Fixed movieWidth, movieHeight;
													AnchorMovie2TopLeft( theFile->theMovie );
													GetMovieDimensions( theFile->theMovie, &movieWidth, &movieHeight );
													if( jfio->sourceType == 'VOD ' ){
														SetTrackDimensions( theFile->theTCTrack, movieWidth, Long2Fix(1) );
														AnchorMovie2TopLeft( theFile->theMovie );
														UpdateMovie(theFile->theMovie);
														GetMovieDimensions( theFile->theMovie, &movieWidth, &movieHeight );
													}
													GetTrackMatrix( textTrack, &theTextMatrix );
													// text below the filmed image (including the TC track if present):
													TranslateMatrix( &theTextMatrix, 0, movieHeight );
													SetTrackMatrix( textTrack, &theTextMatrix );
												}
												if( jfio->sourceType == 'VOD ' ){
													SetTrackEnabled( textTrack, TRUE );
												}
											}
											UpdateMovie( theFile->theMovie );
											AnchorMovie2TopLeft( theFile->theMovie );
										}
										{ char *fName = P2Cstr( theImage->src );
											MetaDataHandler( theFile->theMovie, textTrack, akTrack,
														 &fName, NULL, ", "
											);
										}
										Log( qtLogPtr, "Added textTrack, %ld samples; duration=%lg/%g (err=%d)\n",
										    textSamples,
										    ((double)textDuration)/theFile->timeScale,
										    ((double)GetMediaDuration(textMedia))/theFile->timeScale, txtErr
										);
									}
									else{
										DisposeTrackMedia(textMedia);
										DisposeMovieTrack(textTrack);
									}
								}
								else{
									Log( qtLogPtr, "error doing EndMediaEdits(textMedia): %d\n", txtErr );
									DisposeTrackMedia(textMedia);
									DisposeMovieTrack(textTrack);
								}
								DisposeHandle( (Handle) textDesc );
							}
						}
					}
#pragma mark ---- wrapup multiple track (ffmpSplit/quad) import ----
					if( importedTracks > 1 ){
					  Track cTrack = theFile->currentTrack;
					  char camString[32];
					  MatrixRecord m;
						// if there are multiple imported tracks, associate the TC track with all of them. We
						// also set the track name on those tracks that will not be visible to our callers
						// (only theImage->theTrack is)
						for( ft = 0 ; ft < importedTracks ; ft++ ){
							snprintf( camString, sizeof(camString), "Camera %d", ft+1 );
							theFile->currentTrack = importTrack[ft];
							if( importTrack[ft] != theImage->theTrack ){
								AddTrackReference( importTrack[ft], theFile->theTCTrack, kTrackReferenceTimeCode, NULL );
								AddMetaDataStringToCurrentTrack( theFile, akDisplayName, (const char*) P2Cstr(theImage->src), NULL );
							}
							// associate the camera ID with the track
							AddMetaDataStringToCurrentTrack( theFile, 'cam#', camString, NULL );
						}
						theFile->currentTrack = cTrack;
						AnchorMovie2TopLeft(theFile->theMovie);
						// move the tracks to their intended positions:
						GetTrackMatrix( importTrack[3], &m );
						TranslateMatrix( &m, Long2Fix(360), Long2Fix(288) );
						SetTrackMatrix( importTrack[3], &m ); UpdateMovie(theFile->theMovie);
						GetTrackMatrix( importTrack[1], &m );
						TranslateMatrix( &m, Long2Fix(360), Long2Fix(0) );
						SetTrackMatrix( importTrack[1], &m ); UpdateMovie(theFile->theMovie);
						GetTrackMatrix( importTrack[2], &m );
						TranslateMatrix( &m, Long2Fix(0), Long2Fix(288) );
						SetTrackMatrix( importTrack[2], &m ); UpdateMovie(theFile->theMovie);
					}
				}
				else{
					if( !theFile->MustUseTrack ){
						DisposeMovieTrack( theFile->theImageTrack );
						theFile->theImageTrack = nil;
					}
				}
				theFile->currentTrack = theFile->theImageTrack;
				if( !theFile->MustUseTrack ){
					// no one else should refer to this imageTrack!
					theFile->theImageTrack = nil;
				}
			}
			// clean up:
			xfree(pCommand);
			DisposeMovie(fragMovie);
			unlink(fragFName);
			xfree(fragFName);

			if( sampleTable ){
				QTSampleTableRelease(sampleTable);
			}
			sampleTable = nil;
			if( imgDesc ){
				DisposeHandle( (Handle) imgDesc);
				imgDesc = nil;
			}
			theFile->numTracks = GetMovieTrackCount(theFile->theMovie);
			if( !theFile->currentTrack ){
				theFile->currentTrack = GetMovieIndTrack( theFile->theMovie, theFile->numTracks );
			}
		}
		else{
			theFile->currentTrack = cTrack;
		}
		if( theFile->theImage.dataRef ){
			if( theFile->theImage.dataRef != theFile->odataRef ){
				DisposeHandle( (Handle) theFile->theImage.dataRef);
			}
			theFile->theImage.dataRef = nil;
		}
		if( cancelled ){
			// now reset the error indicator, as it is the only way we have to permeate the cancel request.
			err = userCanceledErr;
		}
	}
	else{
#if defined(DEBUG) && defined(_MSC_VER)
	  char cwd[1024], *c;
		c = _getcwd(cwd, sizeof(cwd));
		if( c && theFile->cwd && strcmp(cwd, theFile->cwd) ){
			Log( qtLogPtr, "Add_imgFrames_From_File(): cwd=\"%s\" is not theFile->cwd=\"%s\"!!\n",
			    cwd, theFile->cwd
			);
		}
#endif
		Log( qtLogPtr, "Add_imgFrames_From_File() cannot open \"%s\" relative to cwd=\"%s\" (%s)\n",
			theURL, (theFile->cwd)? theFile->cwd : ".", strerror(errno) );
		// MacErrors.h - fnfErr wfFileNotFound
		err = (errno)? errno : fnfErr;
	}
	if( TCInfo.StartTimes ){
		free(TCInfo.StartTimes);
	}
	if( TCInfo.frequencies ){
		free(TCInfo.frequencies);
	}
	if( TCInfo.durations ){
		free(TCInfo.durations);
	}
	if( TCInfo.frames ){
		free(TCInfo.frames);
	}
	if( jfio->fpLog ){
		fclose(jfio->fpLog);
		jfio->fpLog = NULL;
	}
	if( err == noErr && jfio->sourceType == 'VOD ' ){
		if( theImage->ffmpSplit ){
			AddMetaDataStringToMovie( theFile, 'quad', "MPG4 VOD imported as 4 tracks", NULL );
		}
		else{
			AddMetaDataStringToMovie( theFile, 'quad', "MPG4 VOD imported as a single track", NULL );
		}
	}
	return err;
}

