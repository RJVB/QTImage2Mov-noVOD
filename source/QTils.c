/*!
 *  @file QTils.c
 *  QTImage2Mov
 *
 *  Created by René J.V. Bertin on 20100723.
 *  Copyright 2010 INRETS / RJVB. All rights reserved.
 *  20101118: see Tech. Q&A 1262 for examples on determining static framerate/mpeg movies (http://developer.apple.com/library/mac/#qa/qa2001/qa1262.html)
 *
 */

#include "winixdefs.h"
#include "copyright.h"
IDENTIFY("QT utilities: Track clip/flip, add metadata, add TimeCode track, ..." );

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#if ! defined(_WINDOWS) && !defined(WIN32) && !defined(_MSC_VER)
#	include <unistd.h>
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

#include "QTils.h"

#include "NaN.h"

#ifndef MIN
#	define	MIN(a,b) (((a)<(b))?(a):(b))
#endif /* MIN */

#define kTimeCodeTrackMaxHeight (320 << 16)	// max height of timecode track
#define kTimeCodeTrackHeight (16)			// default hardcoded height for timecode track
#undef TC64bits
// 64bits TimeCode supposes one of 24, 25, 30 or 29.97 fps?!
#ifdef TC64bits
#	define kTimeCodeMediaType	TimeCode64MediaType
#else
#	define kTimeCodeMediaType	TimeCodeMediaType
#endif

OSErr AnchorMovie2TopLeft( Movie theMovie )
{ Rect mBox;
  OSErr err;
	GetMovieBox( theMovie, &mBox );
	mBox.right -= mBox.left;
	mBox.bottom -= mBox.top;
	mBox.top = mBox.left = 0;
	SetMovieBox( theMovie, &mBox );
	err = GetMoviesError();
	UpdateMovie( theMovie );
	return err;
}

// define and install a rectangular clipping region to the current track to mask off all but the requested
// quadrant (channel). If requested, also flip the track horizontally.
void ClipFlipCurrentTrackToQuadrant( QI2MSourceFile theFile, short quadrant, short hflip )
{ int clipped = FALSE;
  Rect trackBox;
  Fixed trackHeight, trackWidth;
  char mdInfo[128] = "";

	if( !theFile->currentTrack ){
		return;
	}

	GetTrackDimensions( theFile->currentTrack, &trackWidth, &trackHeight );
	trackBox.left = trackBox.top = 0;
	trackBox.right = FixRound( trackWidth );
	trackBox.bottom = FixRound( trackHeight );

	if( quadrant >= 0 && quadrant < 4 ){
	// From http://developer.apple.com/mac/library/documentation/QuickTime/RM/MovieInternals/MTTimeSpace/B-Chapter/2MovieTimeandSpace.html:
	  QDErr err;
	  short camWidth, camHeight;
	  Rect camBox;
	  RgnHandle trClip;
		camWidth = trackBox.right / 2;
		camHeight = trackBox.bottom / 2;
		switch( quadrant ){
			case 0:
				camBox.left = camBox.top = 0;
				camBox.right = camWidth;
				camBox.bottom = camHeight;
				break;
			case 1:
				camBox.left = camWidth;
				camBox.top = 0;
				camBox.right = trackBox.right;
				camBox.bottom = camHeight;
				break;
			case 2:
				camBox.left = 0;
				camBox.top = camHeight;
				camBox.right = camWidth;
				camBox.bottom = trackBox.bottom;
				break;
			case 3:
				camBox.left = camWidth;
				camBox.top = camHeight;
				camBox.right = trackBox.right;
				camBox.bottom = trackBox.bottom;
				break;
		}
		// the clipping region is initialised and defined using obsolescent functions, but I have
		// found no "new" way of doing the same thing.
		trClip = NewRgn();
		RectRgn(trClip, &camBox);
		SetTrackClipRgn( theFile->currentTrack, trClip );
		err = GetMoviesError();
		DisposeRgn( trClip );
		UpdateMovie( theFile->theMovie );
		if( err == noErr ){
			clipped = TRUE;
			// the new track bounding box:
			trackBox = camBox;
			snprintf( mdInfo, sizeof(mdInfo), "quadrant #%d ", quadrant );
		}
	}
//	else{
//		Log( qtLogPtr, "ClipFlipCurrentTrackToQuadrant() ignoring quadrant=%hd\n", quadrant );
//	}
	// a Horizontal flip is obtained through a matrix operation:
	if( hflip ){
	  MatrixRecord m;
		GetTrackMatrix( theFile->currentTrack, &m );
#ifdef DEBUG
		Log( qtLogPtr, "[[%6d %6d %6d]\n", FixRound(m.matrix[0][0]), FixRound(m.matrix[0][1]), FixRound(m.matrix[0][2]) );
		Log( qtLogPtr, " [%6d %6d %6d]\n", FixRound(m.matrix[1][0]), FixRound(m.matrix[1][1]), FixRound(m.matrix[1][2]) );
		Log( qtLogPtr, " [%6d %6d %6d]] type %d --> (hori.flip)\n",
			FixRound(m.matrix[2][0]), FixRound(m.matrix[2][1]), FixRound(m.matrix[2][2]),
			GetMatrixType(&m)
		);
#endif
#if 1
		{ Rect flippedBox;
			// construct the flipped version of the track's bounding box:
			flippedBox.right = trackBox.left;
			flippedBox.top = trackBox.top;
			flippedBox.left = trackBox.right;
			flippedBox.bottom = trackBox.bottom;
			MapMatrix( &m, &trackBox, &flippedBox );
		}
#else
		// different solution - this one implies a translation over -trackWidth which becomes
		// visible when adding a subsequent track. SetIdentityMatrix isn't required if one already
		// has obtained the track's matrix...
//		SetIdentityMatrix( &m );
		ScaleMatrix( &m, -fixed1, fixed1, 0, 0 );
#endif
#ifdef DEBUG
		Log( qtLogPtr, "[[%6d %6d %6d]\n", FixRound(m.matrix[0][0]), FixRound(m.matrix[0][1]), FixRound(m.matrix[0][2]) );
		Log( qtLogPtr, " [%6d %6d %6d]\n", FixRound(m.matrix[1][0]), FixRound(m.matrix[1][1]), FixRound(m.matrix[1][2]) );
		Log( qtLogPtr, " [%6d %6d %6d]] type %d\n",
			FixRound(m.matrix[2][0]), FixRound(m.matrix[2][1]), FixRound(m.matrix[2][2]),
			GetMatrixType(&m)
		);
#endif
		SetTrackMatrix( theFile->currentTrack, &m );
		if( GetMoviesError() == noErr ){
			strncat( mdInfo, "h-flipped", sizeof(mdInfo) - strlen(mdInfo) );
		}
	}
	if( clipped || hflip ){
	  Track TCTrack, TCRefTrack = nil;
	  long tidx = 1, ridx;
	  int found = FALSE;
		AnchorMovie2TopLeft( theFile->theMovie );
#if 0
		while( !found &&
			(TCTrack = GetMovieIndTrackType(theFile->theMovie,
								tidx, kTimeCodeMediaType, movieTrackMediaType|movieTrackEnabledOnly ))
		){
			ridx = 1;
			if( GetTrackReferenceCount( TCTrack, kTrackReferenceTimeCode ) ){
				while( (TCRefTrack = GetTrackReference( TCTrack, kTrackReferenceTimeCode, ridx ))
					 && (TCRefTrack != theFile->currentTrack)
				){
					ridx += 1;
	}
				if( TCRefTrack == theFile->currentTrack ){
					found = TRUE;
	}
				else{
					tidx += 1;
}
	}
	else{
				// a TCTrack without references. Weird, but let's assume it applies to all tracks
				// in the movie..!
				found = TRUE;
			}
		}
		if( found ){
			SetTrackDimensions( TCTrack, trackWidth, Long2Fix(kTimeCodeTrackHeight) );
			GetTrackDimensions( TCTrack, &trackWidth, &trackHeight );
		}
#endif
	}
	// add some metadata about what we just did:
	if( strlen(mdInfo) ){
		AddMetaDataStringToCurrentTrack( theFile, akInfo, mdInfo, NULL );
	}
}

OSErr MetaDataHandler( Movie theMovie, Track toTrack,
						  AnnotationKeys key, char **Value, char **Lang, char *separator )
{ QTMetaDataRef theMetaData;
  OSErr err;
  const char *value, *lang = NULL;
	if( theMovie && Value ){
	  OSType inkey;
	  QTMetaDataStorageFormat storageF;
	  QTMetaDataKeyFormat inkeyF;
	  UInt8 *inkeyPtr;
	  ByteCount inkeySize;

		value = (const char*) *Value;
		if( Lang ){
			lang = (const char*) *Lang;
		}
		// 20110325:
		if( !separator ){
			separator = (key == akSource)? ", " : "\n";
		}

		// by default, we use QuickTime storage with Common Format keys that are OSTypes:
		storageF = kQTMetaDataStorageFormatQuickTime;
		inkeyF = /*kQTMetaDataKeyFormatQuickTime*/ kQTMetaDataKeyFormatCommon;
		inkeyPtr = (UInt8*) &inkey;
		inkeySize = sizeof(inkey);
		switch( key ){
			case akAuthor:
				inkey = kQTMetaDataCommonKeyAuthor;
				break;
			case akComment:
				inkey = kQTMetaDataCommonKeyComment;
				break;
			case akCopyRight:
				inkey = kQTMetaDataCommonKeyCopyright;
				break;
			case akDisplayName:
//				inkey = kUserDataTextFullName;
// with kQTMetaDataCommonKeyDisplayName, akDisplayName sets the track name
// as shown in the QT Player inspector.
				inkey = kQTMetaDataCommonKeyDisplayName;
				storageF = kQTMetaDataStorageFormatUserData;
				inkeyF = kQTMetaDataKeyFormatUserData;
				break;
			case akInfo:
				inkey = kQTMetaDataCommonKeyInformation;
				break;
			case akKeywords:
				inkey = kQTMetaDataCommonKeyKeywords;
				break;
			case akDescr:
				inkey = kUserDataTextDescription;
				storageF = kQTMetaDataStorageFormatUserData;
				inkeyF = kQTMetaDataKeyFormatUserData;
				break;
			case akFormat:
				inkey = kUserDataTextOriginalFormat;
				storageF = kQTMetaDataStorageFormatUserData;
				inkeyF = kQTMetaDataKeyFormatUserData;
				break;
			case akSource:
				inkey = kUserDataTextOriginalSource;
				storageF = kQTMetaDataStorageFormatUserData;
				inkeyF = kQTMetaDataKeyFormatUserData;
				break;
			case akSoftware:
				inkey = kQTMetaDataCommonKeySoftware;
				break;
			case akWriter:
				inkey = kUserDataTextWriter;
				storageF = kQTMetaDataStorageFormatUserData;
				inkeyF = kQTMetaDataKeyFormatUserData;
				break;
			case akCreationDate:
				inkey = kUserDataTextCreationDate;
				storageF = kQTMetaDataStorageFormatUserData;
				inkeyF = kQTMetaDataKeyFormatUserData;
				break;
			case akYear:
				// the Year key shown by QuickTime Player Pro doesn't have an OSType associated with it
				// but only an URI that serves as the key. Set up things appropriately:
				inkey = 'year';
				inkeyPtr = (UInt8*) "com.apple.quicktime.year";
				inkeySize = strlen( (char*) inkeyPtr);
				inkeyF = kQTMetaDataKeyFormatQuickTime;
				break;
			case akTrack:
				inkey = kUserDataTextTrack;
				storageF = kQTMetaDataStorageFormatUserData;
				inkeyF = kQTMetaDataKeyFormatUserData;
				break;
			default:
//				return kQTMetaDataInvalidKeyFormatErr;
				inkey = key;
				storageF = kQTMetaDataStorageFormatUserData;
				inkeyF = kQTMetaDataKeyFormatUserData;
				break;
		}
		if( toTrack ){
			err = QTCopyTrackMetaData( toTrack, &theMetaData );
		}
		else{
			err = QTCopyMovieMetaData( theMovie, &theMetaData );
		}
		if( err == noErr ){
		  char *newvalue = NULL;
		  QTMetaDataItem item = kQTMetaDataItemUninitialized;
		  OSStatus qmdErr;
			// not sure if we have to make copies of the metadata values...
			if( value ){
				value = strdup(value);
			}
			// check if the key is already present. If so, append the new value to the existing value,
			// separated by a newline.
			qmdErr = QTMetaDataGetNextItem(theMetaData, storageF, item, inkeyF, inkeyPtr, inkeySize, &item);
			if( qmdErr == noErr ){
				if( item != kQTMetaDataItemUninitialized ){
				  ByteCount size=0, nvlen;
					qmdErr = QTMetaDataGetItemValue( theMetaData, item, NULL, 0, &size );
					if( value ){
						// RJVB 20110426: pass an empty string to remove the MD item
						if( qmdErr == noErr && !*value ){
							QTMetaDataRemoveItem( theMetaData, item );
							QTMetaDataRelease( theMetaData );
							return qmdErr;
						}
						nvlen = size + strlen(value) + 2;
					}
					else{
						nvlen = size + 1;
					}
					if( qmdErr == noErr && (newvalue = calloc( nvlen, sizeof(char) )) ){
						// get the <size> bytes of the value:
						if( (qmdErr = QTMetaDataGetItemValue( theMetaData, item,
										(UInt8*) newvalue, size, 0 )) == noErr
						){
							if( !value ){
							  char *lng=NULL;
							  ByteCount n = 0;
								*Value = newvalue;
								if( Lang && (lng = calloc(16, sizeof(char))) ){
									err = QTMetaDataGetItemProperty( theMetaData, item,
											  kPropertyClass_MetaDataItem,
											  kQTMetaDataItemPropertyID_Locale,
											  16, lng, &n
									);
									if( n ){
										*Lang = lng;
									}
									else{
										free(lng);
										*Lang = NULL;
									}
								}
								QTMetaDataRelease( theMetaData );
								return qmdErr;
							}
							{ size_t len = strlen(newvalue);
								snprintf( &newvalue[len], nvlen - len, "%s%s", separator, value );
							}
							free((void*)value);
							value = newvalue;
							newvalue = NULL;
							 // set the item to the new value:
							if( (qmdErr = QTMetaDataSetItem( theMetaData, item,
										(UInt8*) value, strlen(value), kQTMetaDataTypeUTF8 )) != noErr
							){
								// let's hope QTMetaDataAddItem() will add the item correctly again
								QTMetaDataRemoveItem( theMetaData, item );
							}
							else{
								err = noErr;
								if( lang && *lang ){
									err = QTMetaDataSetItemProperty( theMetaData, item,
											 kPropertyClass_MetaDataItem,
											 kQTMetaDataItemPropertyID_Locale,
											 strlen(lang), lang
									);
								}
								UpdateMovie( theMovie );
							}
						}
					}
					else{
						qmdErr = -1;
					}
				}
			}
			if( qmdErr != noErr && value ){
				err = QTMetaDataAddItem( theMetaData, storageF, inkeyF,
						inkeyPtr, inkeySize, (UInt8 *)value, strlen(value), kQTMetaDataTypeUTF8, &item
				);
				if( err != noErr ){
					Log( qtLogPtr, "Error adding key %.4s=\"%s\" (%d)\n", OSTStr(inkey), value, err );
					// failure, in this case we're sure we can release the allocated memory.
					if( value != newvalue ){
						free((void*)value);
						value = NULL;
					}
					if( newvalue ){
						free((void*)newvalue);
						newvalue = NULL;
					}
				}
				else{
					// 20110927: leak analysis suggests that we are supposed to release allocated memory
					// also when the metadata has been updated successfully:
					if( value != newvalue ){
						free((void*)value);
						value = NULL;
					}
					if( newvalue ){
						free((void*)newvalue);
						newvalue = NULL;
					}
					if( lang && *lang ){
						err = QTMetaDataSetItemProperty( theMetaData, item,
								  kPropertyClass_MetaDataItem,
								  kQTMetaDataItemPropertyID_Locale,
								  strlen(lang), lang
						);
					}
					UpdateMovie( theMovie );
				}
			}
			QTMetaDataRelease( theMetaData );
//			if( newvalue ){
//				free(newvalue);
//				newvalue = NULL;
//			}
		}
	}
	else{
		err = paramErr;
	}
	return err;
}

short AddMetaDataStringToCurrentTrack( struct QI2MSourceFileRecord *theFile,
							    AnnotationKeys key, const char *value, const char *lang )
{
	if( !theFile || !theFile->currentTrack ){
		return paramErr;
	}
	else{
		return (short) MetaDataHandler( theFile->theMovie, theFile->currentTrack, key,
								 (char**) &value, (char**) &lang, "\n"
		);
	}
}

short AddMetaDataStringToMovie( struct QI2MSourceFileRecord *theFile,
							    AnnotationKeys key, const char *value, const char *lang )
{
	if( !theFile ){
		return paramErr;
	}
	else{
		return (short) MetaDataHandler( theFile->theMovie, NULL, key, (char**) &value, (char**) &lang, "\n" );
	}
}

short GetMetaDataStringFromCurrentTrack( struct QI2MSourceFileRecord *theFile,
							    AnnotationKeys key, char **value, char **lang )
{
	if( !theFile || !theFile->currentTrack ){
		return paramErr;
	}
	else{
		return (short) MetaDataHandler( theFile->theMovie, theFile->currentTrack, key, value, lang, "\n" );
	}
}

short GetMetaDataStringFromMovie( struct QI2MSourceFileRecord *theFile,
							    AnnotationKeys key, char **value, char **lang )
{
	if( !theFile ){
		return paramErr;
	}
	else{
		return (short) MetaDataHandler( theFile->theMovie, NULL, key, value, lang, "\n" );
	}
}

int CopyMovieMetaData( Movie dstMovie, Movie srcMovie )
{ char *mdstr = NULL, *lang = NULL;
  int n = 0, i;
  OSErr err;
  AnnotationKeys key[] = { akAuthor, akComment, akCopyRight, akDisplayName,
		akInfo, akKeywords, akDescr, akFormat, akSource,
		akSoftware, akWriter, akYear, akCreationDate };
	for( i = 0 ; i < sizeof(key)/sizeof(AnnotationKeys) ; i++ ){
		err = MetaDataHandler( srcMovie, NULL, key[i], &mdstr, &lang, "\n" );
		if( err == noErr ){
			if( mdstr ){
				MetaDataHandler( dstMovie, NULL, key[i], &mdstr, &lang, "\n" );
				free(mdstr), free(lang);
				mdstr = NULL, lang = NULL;
				n += 1;
			}
			else if( lang ){
				free(lang);
				lang = NULL;
			}
		}
		else{
			// just be sure that we don't re-enter MetaDataHandler with non-NULL
			// mdstr and/or lang arguments if the goal is to retrieve existing meta-data!
			mdstr = lang = NULL;
		}
	}
	if( n ){
		UpdateMovie(dstMovie);
	}
	return n;
}

int CopyTrackMetaData( Movie dstMovie, Track dstTrack, Movie srcMovie, Track srcTrack )
{ char *mdstr = NULL, *lang = NULL;
  int n = 0, i;
  OSErr err;
  AnnotationKeys key[] = { akAuthor, akComment, akCopyRight, akDisplayName,
		akInfo, akKeywords, akDescr, akFormat, akSource,
		akSoftware, akWriter, akYear, akCreationDate };
	for( i = 0 ; i < sizeof(key)/sizeof(AnnotationKeys) ; i++ ){
		err = MetaDataHandler( srcMovie, srcTrack, key[i], &mdstr, &lang, "\n" );
		if( err == noErr && mdstr ){
			MetaDataHandler( dstMovie, dstTrack, key[i], &mdstr, &lang, "\n" );
			free(mdstr); mdstr = NULL;
				n += 1;
			}
		if( lang ){
				free(lang);
				lang = NULL;
			}
		}
	if( n ){
		UpdateMovie(dstMovie);
	}
	return n;
}

// from http://developer.apple.com/library/mac/#qa/qa2001/qa1262.html
/*
 * Get the identifier for the media that contains the first
 * video track's sample data, and also get the media handler for
 * this media.
 * 20110105: if no enabled video track found, try a TimeCode track...
 */
/*static*/ OSErr MovieGetVideoMediaAndMediaHandler( Movie inMovie, Track *inOutVideoTrack, Media *outMedia, MediaHandler *outMediaHandler )
{ OSErr err;
	if( (inMovie || (inOutVideoTrack && *inOutVideoTrack)) && outMedia && outMediaHandler ){
	  Track lTrack = nil;
		*outMedia = NULL;
		*outMediaHandler = NULL;
		if( !inOutVideoTrack ){
			inOutVideoTrack = &lTrack;
		}

		if( inMovie ){
			/* get first video track with a framefrate */
			*inOutVideoTrack = GetMovieIndTrackType(inMovie, 1, FOUR_CHAR_CODE('vfrr'),
							movieTrackCharacteristic | movieTrackEnabledOnly);
		}
		if( *inOutVideoTrack ){
			/* get media ref. for track's sample data */
			*outMedia = GetTrackMedia(*inOutVideoTrack);
			if( *outMedia ){
				/* get a reference to the media handler component */
				*outMediaHandler = GetMediaHandler(*outMedia);
			}
		}
		err = GetMoviesError();
	}
	else{
		err = paramErr;
	}
	return err;
}

TimeScale GetTrackTimeScale( Track theTrack )
{ OSErr err;
  TimeScale ts = -1;
	if( theTrack ){
	  Media theMedia;
	  MediaHandler mh;
		err = MovieGetVideoMediaAndMediaHandler( nil, &theTrack, &theMedia, &mh );
		if( err == noErr && theMedia ){
			ts = GetMediaTimeScale( theMedia );
		}
	}
	return ts;
}

// the static framerate of a movie, calculated from its first video track:
OSErr GetMovieStaticFrameRate( Movie theMovie, double *outFPS, double *fpsTC, double *fpsMedia )
{ OSErr err;
	if( theMovie && outFPS ){
	  Media theMedia;
	  MediaHandler mh;
	  Track TCTrack;
	  double tcfps = -1, mfps = -1;
		TCTrack = GetMovieIndTrackType( theMovie, 1, TimeCodeMediaType,
								    movieTrackMediaType | movieTrackEnabledOnly
		);
		err = GetMoviesError();
		if( err == noErr && TCTrack ){
		  MediaHandler mh;
		  TimeCodeDef tcdef;
			mh = GetMediaHandler( GetTrackMedia(TCTrack) );
			err = (OSErr) TCGetCurrentTimeCode( mh, NULL, &tcdef, NULL, NULL );
			if( err == noErr ){
			 // this one is actualle much more accurate?!
				tcfps = ((double)tcdef.fTimeScale) / ((double)tcdef.frameDuration);
			}
		}
		err = MovieGetVideoMediaAndMediaHandler( theMovie, NULL, &theMedia, &mh );
		if( err == noErr && theMedia ){
			err = GetMediaStaticFrameRate( theMedia, &mfps, NULL, NULL );
		}
			if( tcfps >= 0 && fabs(tcfps - mfps) < 0.01 ){
				*outFPS = tcfps;
			}
			else if( mfps >= 0 ){
				*outFPS = mfps;
			}
		if( fpsTC ){
			*fpsTC = tcfps;
		}
		if( fpsMedia ){
			*fpsMedia = mfps;
		}
	}
	else{
		err = paramErr;
	}
	return err;
}

OSErr GetTrackStaticFrameRate( Track theTrack, double *outFPS, long *frames, double *duration )
{ OSErr err;
	if( theTrack && outFPS ){
	  Media theMedia;
	  MediaHandler mh;
	  double mfps = -1;
		err = MovieGetVideoMediaAndMediaHandler( nil, &theTrack, &theMedia, &mh );
		if( err == noErr && theMedia ){
			err = GetMediaStaticFrameRate( theMedia, &mfps, frames, duration );
					}
		if( mfps >= 0 ){
			*outFPS = mfps;
				}
					}
	else{
		err = paramErr;
				}
	return err;
			}

// from http://developer.apple.com/library/mac/#qa/qa2001/qa1262.html
/*
 * Given a reference to the media that contains the sample data for a track,
 * calculate the static frame rate.
 */
OSErr GetMediaStaticFrameRate( Media inMovieMedia, double *outFPS, long *frames, double *duration )
{ OSErr err = GetMoviesError();
	if( inMovieMedia && outFPS ){
	  /* get the number of samples in the media */
	  long sampleCount = GetMediaSampleCount(inMovieMedia);

		if( err == noErr && !sampleCount ){
			err = invalidMedia;
		}
		if( err == noErr ){
		  /* find the media duration */
		  TimeValue64 duration64 = GetMediaDisplayDuration(inMovieMedia);
			err = GetMoviesError();
			if( frames ){
				*frames = sampleCount;
		}
			if( err == noErr ){
			  /* get the media time scale */
			  TimeValue64 timeScale = GetMediaTimeScale(inMovieMedia);
			err = GetMoviesError();
				if( err == noErr ){
					/* calculate the frame rate:
					 * frame rate = (sample count * media time scale) / media duration
					 */
					*outFPS = (double)sampleCount * (double)timeScale / (double)duration64;
					if( duration ){
						*duration = ((double)duration64) / ((double)timeScale);
					}
				}
			}
		}
	}
	else{
		err = paramErr;
	}
	return err;
}

////////// from Apple's qtframestepper.c :
//
// QTStep_GetStartTimeOfFirstVideoSample
// Return, through the theTime parameter, the starting time of the first video sample of the
// specified QuickTime movie.
//
// The "trick" here is to set the nextTimeEdgeOK flag, to indicate that you want to get the
// starting time of the beginning of the movie.
//
// If this function encounters an error, it returns a (bogus) starting time of -1. Note that
// GetMovieNextInterestingTime also returns -1 as a starting time if the search criteria
// specified in the myFlags parameter are not matched by any interesting time in the movie.
//
//////////

OSErr QTStep_GetStartTimeOfFirstVideoSample (Movie theMovie, TimeValue *theTime)
{
	short			myFlags;
	OSType			myTypes[1];

	*theTime = -1;			// a bogus starting time
	if (theMovie == NULL)
		return(invalidMovie);

	myFlags = nextTimeMediaSample + nextTimeEdgeOK;			// we want the first sample in the movie
	myTypes[0] = VisualMediaCharacteristic;					// we want video samples

	GetMovieNextInterestingTime(theMovie, myFlags, 1, myTypes, (TimeValue)0, fixed1, theTime, NULL);
	return(GetMoviesError());
}

void SetMovieStartTime( Movie theMovie, TimeValue startTime, int changeTimeBase )
{ TimeRecord mTime;
	GetMovieTime( theMovie, &mTime );
	if( changeTimeBase ){
	  TimeRecord tr;
		mTime.base = GetMovieTimeBase(theMovie);
		GetTimeBaseStartTime(mTime.base, GetMovieTimeScale(theMovie), &tr );
		tr.value.hi = 0;
		tr.value.lo = startTime;
		SetTimeBaseStartTime(mTime.base, &tr );
//		Log( qtLogPtr, "SetMovieStartTime() -> %gs\n", (double) startTime / (double) GetMovieTimeScale(theMovie) );
//		SetMovieMasterTimeBase(theMovie, mTime.base, nil );
	}
	mTime.value.hi = 0;
	mTime.value.lo = startTime;
	SetMovieTime( theMovie, &mTime );
}

OSErr MaybeShowMoviePoster( Movie theMovie )
{ QTVisualContextRef ctx;
  OSErr err;
	if( theMovie ){
		err = (OSErr) GetMovieVisualContext( theMovie, &ctx );
		if( err == noErr ){
			ShowMoviePoster( theMovie );
			err = GetMoviesError();
		}
	}
	else{
		err = paramErr;
	}
	return err;
}

// 20101017: kudos to Jan Schotsman for giving the solution on how to create a "dref atom" that allows to
// store all the TimeCode information in theFile->theMovie. The second approach using PtrToHand comes from
// http://developer.apple.com/library/mac/#qa/qa1539/_index.html
// see also http://www.mactech.com/articles/mactech/Vol.18/18.11/ShesGottaHaveIt/index.html ("saving movies from memory")
OSErr DataRefFromVoid( Handle *dataRef, OSType *dataRefType )
{ unsigned long drefAtomHeader[2];
  OSErr err = paramErr;
	if( dataRef && dataRefType ){
#if 1
		*dataRef = NewHandleClear( sizeof(Handle) + sizeof(char) );

		drefAtomHeader[0] = EndianU32_NtoB(sizeof(drefAtomHeader));
		drefAtomHeader[1] = EndianU32_NtoB( kDataRefExtensionInitializationData );
		err = PtrAndHand( drefAtomHeader, *dataRef, sizeof(drefAtomHeader) );
#else
	  Handle hMovieData;
		*dataRef = NULL;
		hMovieData = NewHandle(0);
		err = PtrToHand( &hMovieData, dataRef, sizeof(Handle) );
#endif
		if( err == noErr ){
			*dataRefType = HandleDataHandlerSubType;
		}
		else{
			*dataRef = NULL;
		}
	}
	return err;
}

OSErr DataRefFromURL( const char **URL, Handle *dataRef, OSType *dataRefType )
{ OSErr err;
  CFStringRef URLRef = (CFStringRef) nil;
  int hasFullPath, freeAnchor = FALSE, isHTTP;
  char *theURL_anchor, *theURL;
	if( !URL || !*URL || !**URL ){
		return paramErr;
	}
	if( strncasecmp( *URL, "ftp://", 6 ) == 0 || strncasecmp( *URL, "http://", 7 ) == 0 ){
		isHTTP = TRUE;
	}
	else{
		isHTTP = FALSE;
	}
#if TARGET_OS_WIN32
	if( !isHTTP ){
		theURL = (char*) *URL;
		theURL++;
		while( *theURL ){
			if( *theURL == '.' && theURL[-1] == '.' ){
				Log( qtLogPtr, "DataRefFromURL(\"%s\"): URL contains an unsupported reference to the parent directory!\n",
				    *URL
				);
				return paramErr;
			}
			theURL++;
		}
	}
#endif
	// we make a local working copy as we will be modifying the string
	theURL = strdup(*URL);
	if( !theURL ){
		return MemError();
	}
	// we keep a copy for later...
	theURL_anchor = theURL;

	if( isHTTP ){
		goto getURLReference;
	}

	if( theURL[0] == '.' ){
		if(
#if TARGET_OS_WIN32
		   theURL[1] == '\\'
#else
		   theURL[1] == '/'
#endif
		){
			// 20101202: we don't want to get stuck on the '/' or '\' character,
			// so we have to increment TWICE, not once!
			theURL += 2;
		}
	}
#if TARGET_OS_WIN32
	hasFullPath = (theURL_anchor[1] == ':'
				|| (theURL_anchor[0]=='\\' && theURL_anchor[1]=='\\')
				|| (theURL_anchor[0]=='/' && theURL_anchor[1]=='/')
			);
#else
	hasFullPath = (theURL_anchor[0] == '/');
#endif
	// not a fully specified path, so we get the current working directory, and prepend it
	// in the appropriate way to our filename.
	if( !hasFullPath ){
	  char *c, cwd[1024];
#ifdef _MSC_VER
	  extern char* _getcwd(char*, int);
		c = _getcwd( cwd, sizeof(cwd) );
#else
		c = getwd( cwd );
#endif
		if( c ){
			if( (c = (char*) malloc( (strlen(cwd) + strlen(theURL) + 2) * sizeof(char) )) ){
				strcpy( c, cwd );
#if TARGET_OS_WIN32
				if( cwd[ strlen(cwd)-1 ] != '\\' ){
				strcat( c, "\\" );
				}
#else
				if( cwd[ strlen(cwd)-1 ] != '/' ){
				strcat( c, "/" );
				}
#endif
				strcat( c, theURL );
				freeAnchor = TRUE;
				theURL = c;
				*URL = theURL;
			}
		}
	}
#if TARGET_OS_WIN32
	// 20101027: correct the pathname for MSWindows, if necessary:
	if( strchr(theURL, '/') ){
	  char *c = theURL;
		if( *c == '/' ){
			*c++ = '\\';
		}
		while( *c ){
			if( *c == '/' /* && c[-1] != '\\'*/ ){
				*c = '\\';
			}
			c++;
		}
	}
#endif

getURLReference:
	{ CFIndex n;
		// We first need to create a CFString from our C string:
		URLRef = CFStringCreateWithCString(kCFAllocatorDefault, theURL, CFStringGetSystemEncoding() );
#ifdef DEBUG
		CFShowStr(URLRef);
#endif
		n = CFStringGetLength(URLRef);
		if( URLRef && (n == strlen(theURL)) ){
			if( isHTTP ){
				err = QTNewDataReferenceFromURLCFString( URLRef, 0, dataRef, dataRefType );
			}
			else{
				err = QTNewDataReferenceFromFullPathCFString( URLRef, kQTNativeDefaultPathStyle, 0,
							dataRef, dataRefType
				);
			}
#ifdef DEBUG
			Log( qtLogPtr, "\"%s\" -> \"%s\" -> dref=%p,%.4s err=%d\n",
				theURL_anchor, theURL, *dataRef, OSTStr(*dataRefType), err
				);
#endif
			CFAllocatorDeallocate( kCFAllocatorDefault, (void*) URLRef );
		}
		else{
			err = internalQuickTimeError;
			goto bail;
		}
	   }

bail:
	if( freeAnchor ){
		free(theURL_anchor);
	}
	if( *URL != theURL && theURL ){
		free(theURL);
	}
	return err;
}

OSErr NewImageMediaForMemory( QI2MSourceFile theFile, Track theTrack, OSType mediaType )
{ OSErr err = noErr;
  Handle cDataRef = nil;
  OSType cDataRefType = 0;

	err = DataRefFromVoid( &cDataRef, &cDataRefType );
	if( err != noErr ){
		Log( qtLogPtr, "NewImageMediaForMemory(): error %d getting Handle dataRef with empty filenaming extension\n",
		    err );
	}
	else{
		theFile->theImage.dataRef = cDataRef;
		theFile->theImage.dataRefType = cDataRefType;
		theFile->theImage.Media = NewTrackMedia( theTrack, mediaType,
						    GetMovieTimeScale(theFile->theMovie), cDataRef, cDataRefType
		);
		if( !theFile->theImage.Media ){
			err = GetMoviesError();
		}
	}
	return err;
}

OSErr NewImageMediaForURL( QI2MSourceFile theFile, Track theTrack, OSType mediaType, char *theURL )
{ OSErr err = noErr;
  Handle cDataRef = nil;
  OSType cDataRefType = 0;

	if( theURL && *theURL ){
	  char *orgURL = theURL;
		// Obtaining a dataRef to the input file first, as this is
		// how QuickTime knows where to look for the sample references in the sample table.
//		err = URLtoDataRef( theURL, theFile, &cDataRef, &cDataRefType );
		err = DataRefFromURL( (const char**) &theURL, &cDataRef, &cDataRefType );
		if( theURL && theURL != orgURL ){
			free(theURL);
			theURL = orgURL;
		}
	}
	if( err != noErr ){
	  char *c, cwd[1024];
#ifdef _MSC_VER
		c = _getcwd( cwd, sizeof(cwd) );
#else
		c = getwd( cwd );
#endif
		Log( qtLogPtr, "NewImageMediaForURL(): cannot get dataRef for \"%s\" (%d; cwd=%s)\n",
		    theURL, err,
		    (c)? c : "??"
		);
	}
	else{
		theFile->theImage.dataRef = cDataRef;
		theFile->theImage.dataRefType = cDataRefType;
		theFile->theImage.Media = NewTrackMedia( theTrack, mediaType,
						    GetMovieTimeScale(theFile->theMovie), cDataRef, cDataRefType
		);
		if( !theFile->theImage.Media ){
			err = GetMoviesError();
		}
	}
	return err;
}

OSErr CopyMovieTracks( Movie dstMovie, Movie srcMovie )
{ long tracks = GetMovieTrackCount(dstMovie);
  long i, srcTracks = GetMovieTrackCount(srcMovie), n;
  Track srcTrack = (Track) -1, dstTrack = nil;
  Media srcMedia, dstMedia = nil;
  Handle cdataRef;
  OSType cdataRefType;
  OSErr err = noErr;

	if( srcTracks > 0 ){
		SetMovieTimeScale( dstMovie, GetMovieTimeScale(srcMovie) );
	}
	for( n = 0, i = 1; i <= srcTracks && srcTrack && err == noErr; i++ ){
	  OSType srcTrackType;
		srcTrack = GetMovieIndTrack(srcMovie, i);
		err = GetMoviesError();
		if( srcTrack && err == noErr ){
		  short refcnt;
		  long attr;
			srcMedia = GetTrackMedia(srcTrack);
			err = GetMoviesError();
			if( err != noErr ){
				srcMedia = nil;
			}
			else{
				GetMediaHandlerDescription(srcMedia, &srcTrackType, nil, nil);
				GetMediaDataRefCount( srcMedia, &refcnt );
				if( (err = GetMediaDataRef( srcMedia, MIN(1,refcnt), &cdataRef, &cdataRefType, &attr )) ){
					Log( qtLogPtr, "Error retrieving MediaDataRef for track %ld with %hd datarefs (%d)\n",
						i, refcnt, (int) err
					);
					cdataRef = nil, cdataRefType = 0;
					err = noErr;
				}
			}
		}
		if( srcTrack && srcMedia ){
			{ Fixed w, h;
				GetTrackDimensions( srcTrack, &w, &h );
#if 1
				err = AddEmptyTrackToMovie( srcTrack, srcMovie, cdataRef, cdataRefType, &dstTrack );
				if( err == noErr && dstTrack ){
					dstMedia = GetTrackMedia(dstTrack);
					err = GetMoviesError();
				}
#else
				dstTrack = NewMovieTrack( dstMovie, w, h, GetTrackVolume(srcTrack) );
#endif
				err = GetMoviesError();
			}
			if( dstTrack && err == noErr ){
				if( !dstMedia ){
					// dataRef,dataRefType refer to srcMovie on disk, but passing them in gives
					// a couldNotResolveDataRef on QT 7.6.6/Win32?!
					dstMedia = NewTrackMedia( dstTrack, srcTrackType /*VideoMediaType*/,
								GetMovieTimeScale(dstMovie), cdataRef, cdataRefType
					);
					err = GetMoviesError();
				}
				if( dstMedia && err == noErr ){
					BeginMediaEdits(dstMedia);
					err = InsertTrackSegment( srcTrack, dstTrack, 0, GetTrackDuration(srcTrack), 0 );
					EndMediaEdits(dstMedia);
					// no need to call InsertMediaIntoTrack here.
				}
				else{
					Log( qtLogPtr, "error creating '%.4s' media for destination track %ld (%d)\n",
						OSTStr(cdataRefType), tracks + 1 + i, err
					);
				}
				if( cdataRef ){
					DisposeHandle( (Handle)cdataRef );
				}
			}
			else{
				err = GetMoviesError();
			}
			if( err != noErr ){
				DisposeMovieTrack( dstTrack );
				dstTrack = nil;
				Log( qtLogPtr, "error inserting track %ld into destination track %ld (%d)\n",
					i, tracks + 1 + i, err
				);
			}
			else{
				dstTrack = nil;
			}
			n += 1;
		}
	}
	if( n ){
		SetMoviePosterTime( dstMovie, GetMoviePosterTime(srcMovie) );
		{ TimeRecord mTime;
			GetMovieTime( dstMovie, &mTime );
			mTime.value.hi = 0;
			mTime.value.lo = GetMoviePosterTime(srcMovie);
			SetMovieTime( dstMovie, &mTime );
		}
		MaybeShowMoviePoster(dstMovie);
		UpdateMovie( dstMovie );
	}
	return err;
}

// obtain a data reference to the requested file. That means we first need to construct the full
// pathname to the file, which entails guessing whether or not theURL is already a full path.
OSErr CreateMovieStorageFromURL( const char *URL, OSType creator, ScriptCode scriptTag, long flags,
						   Handle *dataRef, OSType *dataRefType, CSBackupHook *csbHook,
						   DataHandler *outDataHandler, Movie *newMovie )
{ OSErr err;
  const char *orgURL = URL;
	err = DataRefFromURL( &URL, dataRef, dataRefType );
	if( err == noErr ){
#if TARGET_OS_MAC
		csbHook->urlRef = CFURLCreateFromFileSystemRepresentation( NULL, (const UInt8*) URL,
													   strlen(URL), false );
		if( csbHook->urlRef ){
			// exclude the file from being backed up by TimeMachine to work around
			// a bug on 10.7 (20111124):
			csbHook->bakExcl = CSBackupIsItemExcluded( csbHook->urlRef, &csbHook->bakExclPath );
			CSBackupSetItemExcluded( csbHook->urlRef, false, true );
		}
#endif
		err = CreateMovieStorage( *dataRef, *dataRefType, creator,
					scriptTag, flags, outDataHandler, newMovie
		);
		if( URL && URL != orgURL ){
			free( (void*) URL);
			URL = orgURL;
		}
	}
	return err;
}

OSErr NewMovieFromURL( Movie *newMovie, short flags, short *id, const char *URL, Handle *dataRef, OSType *dataRefType )
{ OSErr err;
  const char *orgURL = URL;
  Handle ldataRef;
  OSType ldataRefType;
	if( !dataRef ){
		dataRef = &ldataRef;
	}
	if( !dataRefType ){
		dataRefType = &ldataRefType;
	}
	err = DataRefFromURL( &URL, dataRef, dataRefType );
	if( err == noErr ){
		err = NewMovieFromDataRef( newMovie, flags, id, *dataRef, *dataRefType );
		if( URL && URL != orgURL ){
			free( (void*) URL);
			URL = orgURL;
		}
		if( dataRef == &ldataRef ){
			DisposeHandle( *dataRef );
		}
	}
	return err;
}

OSErr SaveMovieAsRefMov( const char *dstURL, QI2MSourceFile srcFile, Movie *theNewMovie )
{ Handle odataRef;
  OSType odataRefType;
  DataHandler odataHandler;
  Movie oMovie;
  OSErr err2;
  CSBackupHook cbState;
    	err2 = CreateMovieStorageFromURL( dstURL, 'TVOD',
							   smCurrentScript, createMovieFileDeleteCurFile|createMovieFileDontCreateResFile,
							   &odataRef, &odataRefType, &cbState,
							   &odataHandler, &oMovie
	);
	if( err2 == noErr ){
		SetMovieProgressProc( oMovie, (MovieProgressUPP)-1L, 0);
		// stuff InsertMovieSegment() does NOT copy:
		SetMovieTimeScale( oMovie, GetMovieTimeScale(srcFile->theMovie) );
		err2 = InsertMovieSegment( srcFile->theMovie, oMovie, 0, GetMovieDuration(srcFile->theMovie), 0 );
		if( err2 == noErr ){
			// more stuff InsertMovieSegment() does NOT copy:
#if 1
			SetMovieStartTime( oMovie, GetMoviePosterTime(srcFile->theMovie), (srcFile->numberMMElements == 1) );
			SetMoviePosterTime( oMovie, GetMoviePosterTime(srcFile->theMovie) );
#else
			{ TimeRecord mTime;
				GetMovieTime( oMovie, &mTime );
				mTime.value.hi = 0;
				mTime.value.lo = GetMoviePosterTime(srcFile->theMovie);
				SetMovieTime( oMovie, &mTime );
			}
			SetMoviePosterTime( oMovie, GetMoviePosterTime(srcFile->theMovie) );
#endif
			CopyMovieSettings( srcFile->theMovie, oMovie );
			CopyMovieUserData( srcFile->theMovie, oMovie, kQTCopyUserDataReplace );
			CopyMovieMetaData( oMovie, srcFile->theMovie );
			UpdateMovie(oMovie);
			MaybeShowMoviePoster(oMovie);
			UpdateMovie(oMovie);
			err2 = AddMovieToStorage( oMovie, odataHandler );
			if( err2 == noErr ){
				err2 = CloseMovieStorage( odataHandler );
			}
			else{
				CloseMovieStorage(odataHandler);
			}
#if TARGET_OS_MAC
			if( cbState.urlRef ){
				CSBackupSetItemExcluded( cbState.urlRef,
								    cbState.bakExcl, cbState.bakExclPath );
				CFRelease(cbState.urlRef);
			}
#endif
		}
		DisposeMovie(oMovie);
		if( err2 == noErr && theNewMovie ){
			NewMovieFromURL( theNewMovie, newMovieActive, NULL, dstURL, NULL, NULL );
		}
	}
	return err2;
}

OSErr FlattenMovieToURL( const char *dstURL, QI2MSourceFile srcFile, Movie *theNewMovie )
{ Handle odataRef;
  OSType odataRefType;
  Movie oMovie = NULL;
  OSErr err2 = 1;
  const char *orgURL = dstURL;
#if TARGET_OS_MAC
  Boolean excl, exclPath;
  CFURLRef urlRef = NULL;
#endif

	err2 = DataRefFromURL( &dstURL, &odataRef, &odataRefType );
#if TARGET_OS_MAC
	urlRef = CFURLCreateFromFileSystemRepresentation( NULL, (const UInt8*) dstURL, strlen(dstURL), false );
#endif
	if( err2 == noErr ){
#if TARGET_OS_MAC
		if( urlRef ){
			// exclude the file from being backed up by TimeMachine to work around
			// a bug on 10.7 (20111124):
			excl = CSBackupIsItemExcluded( urlRef, &exclPath );
			CSBackupSetItemExcluded( urlRef, false, true );
		}
#endif
		oMovie = FlattenMovieDataToDataRef( srcFile->theMovie,
				flattenAddMovieToDataFork|flattenCompressMovieResource,
				odataRef, odataRefType, 'TVOD', smCurrentScript,
				createMovieFileDontOpenFile|createMovieFileDeleteCurFile|createMovieFileDontCreateResFile
		);
		err2 = GetMoviesError();
	}
	if( err2 == noErr && oMovie ){
		SetMovieProgressProc( oMovie, (MovieProgressUPP)-1L, 0);
		// stuff InsertMovieSegment() does NOT copy:
		SetMovieTimeScale( oMovie, GetMovieTimeScale(srcFile->theMovie) );
		if( err2 == noErr ){
			// more stuff InsertMovieSegment() does NOT copy:
			SetMovieStartTime( oMovie, GetMoviePosterTime(srcFile->theMovie), (srcFile->numberMMElements == 1) );
			SetMoviePosterTime( oMovie, GetMoviePosterTime(srcFile->theMovie) );
			CopyMovieSettings( srcFile->theMovie, oMovie );
			CopyMovieUserData( srcFile->theMovie, oMovie, kQTCopyUserDataReplace );
			CopyMovieMetaData( oMovie, srcFile->theMovie );
			UpdateMovie(oMovie);
			MaybeShowMoviePoster(oMovie);
			UpdateMovie(oMovie);
			if( theNewMovie ){
				*theNewMovie = oMovie;
			}
		}
	}
#if TARGET_OS_MAC
	if( urlRef ){
		CSBackupSetItemExcluded( urlRef, excl, exclPath );
		CFRelease(urlRef);
	}
#endif
	if( dstURL && dstURL != orgURL ){
		free( (void*) dstURL);
	}
	return err2;
}

void GetMovieDimensions( Movie theMovie, Fixed *width, Fixed *height )
{ long w, h;
  Rect box;
	GetMovieNaturalBoundsRect( theMovie, &box );
	if( box.left > box.right ){
		w = box.left - box.right;
	}
	else{
		w = box.right - box.left;
}
	if( box.bottom > box.top ){
		h = box.bottom - box.top;
	}
	else{
		h = box.top - box.bottom;
	}
	*width = Long2Fix(w);
	*height = Long2Fix(h);
}

// Add a TimeCode track to the current movie, which will allow to register wall time without
// adding a bogus frame that lasts from t=00:00:00 to t=<start time>.
// Code after Apple's QTKitTimecode example project, (C) Apple 2007

// a TimeCode track can have multiple segments, each of its own duration and with its own playback
// rate. the TCTrackInfo functions below provide an easy/simple interface to this feature.

#define xfree(x)	if((x)){ free((x)); (x)=NULL; }

void *dispose_TCTrackInfo( void *ptr )
{ TCTrackInfo *info = (TCTrackInfo*) ptr, *ret = NULL;
	if( info ){
		xfree( info->StartTimes );
		xfree( info->durations );
		xfree( info->frames );
		xfree( info->theURL );
		info->theTCTrack = nil;
		ret = info->cdr;
		free( info );
	}
	return (void*) ret;
}

static TCTrackInfo *new_TCTrackInfo( Track TCTrack, int N, double *StartTimes, double *durations, double *frequencies, size_t *frames, const char *theURL )
{ TCTrackInfo *info = NULL;
  int i;
	if( N > 0 && theURL ){
		if( (info = (TCTrackInfo*) calloc( 1, sizeof(TCTrackInfo) ) ) ){
			if( (info->theURL = strdup(theURL))
			   && (info->StartTimes = (double*) malloc( N * sizeof(double) ))
			   && (info->durations = (double*) malloc( N * sizeof(double) ))
			   && (!frequencies || (info->frequencies = (double*) malloc( N * sizeof(double) )))
			   && (info->frames = (size_t*) malloc( N * sizeof(size_t) ))
			){
				for( i = 0 ; i < N ; i++ ){
					info->StartTimes[i] = StartTimes[i];
					info->durations[i] = durations[i];
					if( frequencies ){
						info->frequencies[i] = frequencies[i];
					}
					info->frames[i] = frames[i];
				}
				info->N = N;
				info->theTCTrack = TCTrack;
				info->dealloc = dispose_TCTrackInfo;
			}
			else{
				dispose_TCTrackInfo(info);
				info = NULL;
			}
		}
	}
	return info;
}

static int compare_TCTrackInfo( TCTrackInfo *info, int N, double *StartTimes, double *durations, double *frequencies, size_t *frames )
{ int n = 0, ok = 1, i;
	if( (frequencies && !info->frequencies)
	   || (!frequencies && info->frequencies)
	){
		return n;
	}
	if( info && N > 0 && info->N == N ){
		for( i = 0 ; i < N && ok ; i++ ){
			if( (StartTimes[i] == info->StartTimes[i])
			   && (durations[i] == info->durations[i])
			   && (!info->frequencies || (frequencies[i] == info->frequencies[i]))
			   && (frames[i] == info->frames[i])
			){
				n += 1;
			}
			else{
				ok = 0;
			}
		}
	}
	return n;
}

short AddTimeCodeTrack( struct QI2MSourceFileRecord *theFile, QI2MSourceImage theImage, long trackStart,
				   int N, double *StartTimes, double *durations, double *frequencies, size_t *frames, const char *theURL )
{ OSErr err = noErr;
  Track theTCTrack = nil;
  TCTrackInfo *TCinfo;
  Media theTCMedia = nil;
  MediaHandler theTCMediaHandler = nil;
  TimeCodeDef theTCDef;
  TimeCodeDescriptionHandle theTCSampleDescriptionH = NULL;
#ifdef TC64bits
  SInt64 theMediaSample = 0L;
  SMPTETime theTCRec = { 0, 1, 0, kSMPTETimeType30Drop, kSMPTETimeValid, 1, 0, 0, 0 }; // 01:00:00:00;
#else
  long theMediaSample = 0L;
  TimeCodeRecord theTCRec;
#endif
  int i, newtrack = 0;
  double totalDuration = 0;
  Handle dataRef = nil;
  OSType dataRefType;
  Fixed Width;

	if( !theFile || !theFile->theMovie || !theImage || !theImage->theTrack
	   || !durations || !frames || !StartTimes
	){
		return (short) paramErr;
	}
	Width = (theFile->movieWidth)? Long2Fix(theFile->movieWidth) : theImage->discoveredWidth;
	if( !Width ){
		return paramErr;
	}

	// check that we have set the timescale.
	if( !theFile->timeScale ){
		theFile->timeScale = GetMovieTimeScale(theFile->theMovie);
	}

	if( (TCinfo = theFile->TCInfoList) ){
		while( TCinfo && compare_TCTrackInfo(TCinfo, N, StartTimes, durations, frequencies, frames)!=N ){
			TCinfo = TCinfo->cdr;
		}
		// if we find an entry for a TimeCode track with the exact same specifications, do not
		// create a new one. Instead, just add a reference for that track to the current image track.
		if( TCinfo ){
			theTCTrack = TCinfo->theTCTrack;
			goto add_track_reference;
		}
	}
	TCinfo = NULL;

	if( !theTCTrack ){
		theTCTrack = NewMovieTrack( theFile->theMovie,
						Width, Long2Fix(kTimeCodeTrackHeight), kNoVolume
		);
		newtrack = 1;
	}
	if( !theTCTrack ){
		err = GetMoviesError();
		goto bail;
	}
	// if we take the data reference from the current image media, that media will be modified
	// by the AddMediaSample2() call below
	if( theFile->odataRef ){
		dataRef = theFile->odataRef;
		dataRefType = theFile->odataRefType;
		err = noErr;
	}
	else{
		err = DataRefFromVoid( &dataRef, &dataRefType );
	}
	if( err == noErr ){
		theTCMedia = NewTrackMedia( theTCTrack, kTimeCodeMediaType, theFile->timeScale,
						dataRef, dataRefType
		);
	}

	if( !theTCMedia ){
		if( err == noErr ){
			err = GetMoviesError();
		}
		goto bail;
	}

	theTCMediaHandler = GetMediaHandler(theTCMedia);
	if( !theTCMediaHandler ){
		err = GetMoviesError();
		goto bail;
	}

	// get default display style options and set them appropriately - not necessary!
	{ TCTextOptions theTextOptions;
	  MatrixRecord theTrackMatrix;
//		TCGetDisplayOptions( theTCMediaHandler, &theTextOptions );
//		theTextOptions.txFont = kFontIDHelvetica;
//		TCSetDisplayOptions(theTCMediaHandler, &theTextOptions);
		GetTrackMatrix( theTCTrack, &theTrackMatrix );
// TimeCode above the filmed image:
//		TranslateMatrix( &theTrackMatrix, 0, Long2Fix(-kTimeCodeTrackHeight) );
// TimeCode below the filmed image:
		TranslateMatrix( &theTrackMatrix, 0, Long2Fix(theFile->movieHeight) );
		SetTrackMatrix( theTCTrack, &theTrackMatrix );
	}

	// enable the track and set the flags to show the timecode
	// the QT Controller will display the information ... but without these options
	// the TimeCode track is ignored for time control.
	SetTrackEnabled(theTCTrack, true);
//	TCSetTimeCodeFlags( theTCMediaHandler, tcdfShowTimeCode, ~tcdfShowTimeCode );
	TCSetTimeCodeFlags( theTCMediaHandler, tcdfShowTimeCode, tcdfShowTimeCode );

	theTCSampleDescriptionH = (TimeCodeDescriptionHandle) NewHandleClear(sizeof(TimeCodeDescription));
	if( !theTCSampleDescriptionH ){
		err = MemError();
		goto bail;
	}

	err = BeginMediaEdits(theTCMedia);
	if( err != noErr ){
		goto bail;
	}

	// record the filename
	if( theURL ){
	  UserData theSourceIDUserData;
		err = NewUserData(&theSourceIDUserData);
		if( noErr == err ){
		  Handle theDataHandle = NULL;

			PtrToHand( (ConstStringPtr) theURL, &theDataHandle, strlen(theURL) );
			err = AddUserDataText( theSourceIDUserData, theDataHandle, TCSourceRefNameType, 1, langEnglish);
			if( noErr == err ){
				err = (OSErr) TCSetSourceRef( theTCMediaHandler, theTCSampleDescriptionH, theSourceIDUserData );
			}
			DisposeHandle(theDataHandle);
			DisposeUserData(theSourceIDUserData);
		}
		// not sure if errors here are blocking, or can be ignored!
		if( err != noErr ){
			Log( qtLogPtr, "AddTimeCodeTrack(): error %d recording URL \"%s\" in the TimeCode metadata!\n",
			    err, theURL
			);
		}
	}

	err = noErr;

	// convert the individual specified segments into something that can be added to the TC track (step 5):
	for( i = 0 ; i < N && err == noErr ; i++ ){
	  double frequency, rfreq;
	  double startTime = StartTimes[i];
	  char Info[512], *info = Info, *Lng = "en_GB", *lng = Lng;
#define TCSCALE	1.0
	  int freq;
		if( !frames[i] || durations[i] <= 0 ){
			Log( qtLogPtr, "AddTimeCodeTrack(): ignoring segment #%d of %gs and %lu frames\n",
			    i, durations[i], frames[i]
		);
			continue;
		}
		if( frequencies ){
			frequency = frequencies[i];
			if( frequency < 0 || NaNorInf(frequency) ){
				frequency = frequencies[i] = frames[i] / durations[i];
			}
		}
		else{
			frequency = frames[i] / durations[i];
		}
		rfreq = frequency;
		freq = (int) (frequency * TCSCALE + 0.5);
#ifndef DEBUG
		if( (i < 3) || i > (N - 3) )
#endif
		{
			Log( qtLogPtr,
			    "AddTimeCodeTrack(): adding %s TC track scale %gx; segment #%d start=%gs %gs/%lu frames %s %gHz\n",
			    (newtrack)? "to new" : "to", TCSCALE,
			    i, startTime/theFile->timeScale, durations[i], frames[i],
			    (frequencies)? "@" : "->", frequency
			);
		}
		if( fabs(frequency*TCSCALE - freq) > 0.1 / theFile->timeScale ){
			// the specified frequency is "sufficiently" non-integer to warrant a DropFrame timecode.
			theTCDef.flags = tc24HourMax|tcDropFrame;
			// 20110108: apparently the way to deal with non-integer frameRates is to select the
			// next higher integer number, but to let fTimeScale/frameDuration be the real frequency.
			freq = (int) ceil(frequency);
			// while Apple's documentation suggests the fTimeScale/frameDuration fraction should be
			// (approx.) the true frequency, it appears to work better if it's the next highest integer value:
			frequency = freq;
#ifndef DEBUG
			if( (i < 3) || i > (N - 3) )
#endif
			{
				Log( qtLogPtr, "Frequency %gHz: abs. decimal part %g > %g so DropFrame timecode\n",
						frequency, fabs(rfreq*TCSCALE-freq), 0.1/theFile->timeScale
				);
			}
		}
		else{
			theTCDef.flags = tc24HourMax;
		}
#if 0
		theTCDef.fTimeScale = (TimeValue)(theFile->timeScale * TCSCALE * frequency);
		theTCDef.frameDuration = (TimeValue) (theFile->timeScale /*/ frequency */);
		theTCDef.numFrames = (UInt8) (frequency * TCSCALE + 0.5);
#else
		theTCDef.fTimeScale = (TimeValue)(theFile->timeScale * TCSCALE );
		theTCDef.frameDuration = (TimeValue) (theFile->timeScale / frequency );
		theTCDef.numFrames = (UInt8) (freq);
#endif
		theTCDef.padding = 0;
#ifdef TC64bits
		theTCRec.mHours = (UInt16) (startTime / 3600);
		startTime -= theTCRec.mHours * 3600;
		theTCRec.mMinutes = (UInt16) (startTime / 60);
		startTime -= theTCRec.mMinutes * 60;
		theTCRec.mSeconds = (UInt16) startTime;
		startTime -= theTCRec.mSeconds;
//		theTCRec.mFrames = (UInt16) (startTime * frequency * TCSCALE);
		theTCRec.mFrames = (UInt16) (startTime * freq);
		snprintf( info, sizeof(Info), "TimeCode for \"%s\": %lu frames for %gs (%gHz) starting at %02u:%02u:%02u:%03u; timeScale=%d, frameDuration=%d\n",
		    ((theURL)? theURL : "<??>"), frames[i], durations[i], frequency,
		    theTCRec.mHours, theTCRec.mMinutes, theTCRec.mSeconds, theTCRec.mFrames,
		    (int) theTCDef.fTimeScale, (int) theTCDef.frameDuration
		);
#else
		theTCRec.t.hours = (UInt8) (startTime / 3600);
		startTime -= theTCRec.t.hours * 3600;
		theTCRec.t.minutes = (UInt8) (startTime / 60);
		startTime -= theTCRec.t.minutes * 60;
		theTCRec.t.seconds = (UInt8) startTime;
		startTime -= theTCRec.t.seconds;
//		theTCRec.t.frames = (UInt8) (startTime * frequency * TCSCALE);
		theTCRec.t.frames = (UInt8) (startTime * freq);
		snprintf( info, sizeof(Info), "TimeCode for \"%s\": %lu frames for %gs (%gHz) starting at %02u:%02u:%02u:%03u; timeScale=%d, frameDuration=%d\n",
		    ((theURL)? theURL : "<??>"), frames[i], durations[i], frequency,
		    theTCRec.t.hours, theTCRec.t.minutes, theTCRec.t.seconds, theTCRec.t.frames,
		    (int) theTCDef.fTimeScale, (int) theTCDef.frameDuration
		);
#endif
#ifndef DEBUG
		if( (i < 3) || i > (N - 3) )
#endif
		{
			Log( qtLogPtr, "AddTimeCodeTrack(): %s", info );
		}
		MetaDataHandler( theFile->theMovie, theTCTrack, akInfo, &info, &lng, "\n" );
		if( info != Info ){
			xfree(info);
		}
		if( lng != Lng ){
			xfree(lng);
		}
		totalDuration += durations[i];

		(**theTCSampleDescriptionH).descSize = sizeof(TimeCodeDescription);
		(**theTCSampleDescriptionH).dataFormat = kTimeCodeMediaType;
		(**theTCSampleDescriptionH).timeCodeDef = theTCDef;

		//**** Step 5b. - add a sample to the timecode track - each sample in a timecode track provides timecode information
		//                for a span of movie time; here, we add a single sample that spans the entire movie duration
		//	The sample data contains a single frame number that identifies one or more content frames that use the
		//  timecode; this value (a long integer) identifies the first frame that uses the timecode
#ifdef TC64bits
		err = TCTimeCodeTimeToFrameNumber( theTCMediaHandler, &theTCDef, &theTCRec, &theMediaSample );
#else
		err = TCTimeCodeToFrameNumber( theTCMediaHandler, &theTCDef, &theTCRec, &theMediaSample );
#endif
		if( err != noErr ){
			Log( qtLogPtr, "AddTimeCodeTrack(): TCTimeCodeToFrameNumber() or TCTimeCodeTimeToFrameNumber() error" );
			goto bail;
		}
		// the timecode media sample must be big-endian
#ifdef TC64bits
		theMediaSample = EndianS64_NtoB(theMediaSample);
#else
		theMediaSample = EndianS32_NtoB(theMediaSample);
#endif

		err = AddMediaSample2( theTCMedia, (UInt8 *)(&theMediaSample), sizeof(long),
						  (TimeValue64)(durations[i] * theFile->timeScale), 0,
						  (SampleDescriptionHandle) theTCSampleDescriptionH, 1, 0, NULL
		);
	}
	if( err != noErr ){
		EndMediaEdits(theTCMedia);
		Log( qtLogPtr, "Failure adding timecode sample(s) to timecode media (%d)\n", (int) err );
		goto bail;
	}
	else{
		err = EndMediaEdits(theTCMedia);
	}
	if( err != noErr ){
		goto bail;
	}
	{ TimeValue refDur = (TimeValue)(totalDuration * theFile->timeScale);
	  TimeValue mediaDur = GetMediaDuration(theTCMedia);
		// 20110304: the culprit for a long-standing mysterious failure of adding the TC media
		// to its track: a round error causing the obtained duration to be very slightly shorter
		// (1 or 2 TimeValues) from the calculated duration. Telling InsertMediaIntoTrack to use
		// the real, total duration (instead of something ever so slightly too long) is the solution...
		// I should have used from the onset!
		if( refDur != mediaDur ){
			Log( qtLogPtr, "Media expected duration %ld != obtained duration %ld", refDur, mediaDur );
		}
		// 20101116: the TimeCode track has to be aligned temporally with the track it corresponds to!
		err = InsertMediaIntoTrack( theTCTrack, trackStart, 0,
							  mediaDur, fixed1
		);
	}
		if( err != noErr ){
			goto bail;
		}

add_track_reference:
	//**** Step 6. - are we done yet? nope, create a track reference from the target track to the timecode track
	//               here we use theImage->theTrack as the target track
	err = AddTrackReference( theImage->theTrack, theTCTrack, kTrackReferenceTimeCode, NULL );
	if( err == noErr ){
	  char *lng = "en_GB";
	  char *trackName = "Timecode Track";
		if( theURL ){
			MetaDataHandler( theFile->theMovie, theTCTrack, akSource, (char**) &theURL, &lng, "\n" );
			MetaDataHandler( theFile->theMovie, theTCTrack, akTrack, (char**) &theURL, NULL, ", " );
		}
		// 20121115:
		MetaDataHandler( theFile->theMovie, theTCTrack, akDisplayName, (char**) &trackName, NULL, ", " );
		UpdateMovie( theFile->theMovie );

		if( newtrack ){
			TCinfo = new_TCTrackInfo( theTCTrack, N, StartTimes, durations, frequencies, frames, theURL );
			if( TCinfo ){
				TCinfo->cdr = theFile->TCInfoList;
				theFile->TCInfoList = TCinfo;
			}
		}
		theFile->theTCTrack = theTCTrack;
	}
bail:
	if( err != noErr && dataRef && dataRef != theFile->odataRef ){
		DisposeHandle( dataRef );
	}
	if( theTCSampleDescriptionH ){
		DisposeHandle( (Handle) theTCSampleDescriptionH );
	}
	if( err != noErr ){
		if( theTCMedia ){
			DisposeTrackMedia( theTCMedia );
		}
		if( theTCTrack && newtrack ){
			DisposeMovieTrack( theTCTrack );
		}
	}
	return (short) err;
}

// Isolate the TimeCode track(s) in the specified movie, either to become invisible or to
// render the other tracks invisible. If enable>0, TimeCode tracks are enabled, the others disabled.
// If enable==0, the reverse happens. If enable<0, the enable/disable state of none of the tracks is modified,
// but if tctracksZeroWidth is True, TimeCode track widths are set to 0 in order for them to be hidden
// but still retain their effect on the movie's temporal behaviour.
OSErr IsolateTCTracks( struct QI2MSourceFileRecord *theFile, QI2MSourceImage theImage,
				  int TCenable, int tctracksZeroWidth, int TXTenableToo )
{ long i, mTracks = GetMovieTrackCount(theFile->theMovie), n, tcN;
  Track mTrack = (Track) -1;
  Media srcMedia;
  OSErr err = noErr;

	for( n = 0, i = 1, tcN = 0 ; i <= mTracks && mTrack && err == noErr ; i++ ){
	  OSType mTrackType;
		mTrack = GetMovieIndTrack(theFile->theMovie, i );
		err = GetMoviesError();
		if( mTrack && err == noErr ){
		  short refcnt;
		  long attr;
			srcMedia = GetTrackMedia(mTrack);
			err = GetMoviesError();
			if( err != noErr ){
				srcMedia = nil;
			}
			else{
				GetMediaHandlerDescription(srcMedia, &mTrackType, nil, nil);
				err = GetMoviesError();
				if( err == noErr ){
				  Fixed tcWidth, tcHeight;
					if( mTrackType == kTimeCodeMediaType ){
						GetTrackDimensions( mTrack, &tcWidth, &tcHeight );
						tcN += 1;
					}
					if( TCenable >= 0 ){
						if( mTrackType == kTimeCodeMediaType
						   || (TXTenableToo && (mTrackType == TextMediaType))
						){
							if( TCenable && mTrackType == TextMediaType ){
							  Fixed mWidth, mHeight;
							  MatrixRecord theTextMatrix;
								SetTrackEnabled( mTrack, !TCenable );
								AnchorMovie2TopLeft( theFile->theMovie );
								GetMovieDimensions( theFile->theMovie, &mWidth, &mHeight );
								GetTrackMatrix( mTrack, &theTextMatrix );
								// text below the filmed image (including the TC track if present):
								TranslateMatrix( &theTextMatrix, 0, mHeight );
								SetTrackMatrix( mTrack, &theTextMatrix );
							}
							SetTrackEnabled( mTrack, TCenable );
						}
						else{
							SetTrackEnabled( mTrack, !TCenable );
						}
					}
					if( tctracksZeroWidth && (mTrackType == kTimeCodeMediaType) ){
						SetTrackDimensions( mTrack, Long2Fix(0), tcHeight );
					}
				}
			}
		}
	}
	if( !tcN && err == noErr && theImage && theImage->theTrack && TCenable > 0 && !tctracksZeroWidth ){
	  double startTime = 0, duration, fps;
	  long frameCount;
	  size_t frames;
	  TimeScale ts = theFile->timeScale;
		theFile->timeScale = GetTrackTimeScale(theImage->theTrack);
//		duration = ((double)GetTrackDuration(theImage->theTrack)) / ((double)theFile->timeScale);
		GetTrackStaticFrameRate( theImage->theTrack, &fps, &frameCount, &duration );
		if( frameCount > 0 ){
			frames = (size_t) frameCount;
		}
		else{
			frames = 0;
		}
		err = AddTimeCodeTrack( theFile, theImage, 0, 1, &startTime, &duration, &fps, &frames, NULL );
		if( err != noErr ){
			Log( qtLogPtr, "IsolateTCTracks() error adding new TimeCode track to \"%s\" (%d)\n",
			    theImage->theURL, err
			);
		}
		theFile->timeScale = ts;
	}
	return err;
}

OSErr initTextTrackWithMedia( QI2MSourceFile theFile, Track *textTrack, Media *textMedia,
							 Fixed defWidth, Fixed defHeight, const char *trackName,
							 TextDescriptionHandle *textDesc )
{ Rect textRect;
  RGBColor colour = {0xffff, 0xffff, 0xffff};
  Handle dataRef = NULL;
  OSType dataRefType;
  Fixed movieWidth, movieHeight;
  OSErr txtErr = noErr;

	if( !theFile || !theFile->theMovie ){
		return paramErr;
	}
	GetMovieDimensions( theFile->theMovie, &movieWidth, &movieHeight );
	if( movieWidth == 0 ){
		movieWidth = defWidth;
	}
	textRect.top = textRect.left = 0;
	textRect.right = FixRound(movieWidth);
	textRect.bottom = 16;
	*textTrack = NewMovieTrack( theFile->theMovie,
							movieWidth, defHeight, kNoVolume
	);
	if( theFile->odataRef ){
		dataRef = theFile->odataRef;
		dataRefType = theFile->odataRefType;
		txtErr = noErr;
	}
	else{
		txtErr = DataRefFromVoid( &dataRef, &dataRefType );
	}
	if( *textTrack && dataRef ){
		*textMedia = NewTrackMedia( *textTrack, TextMediaType,
				(theFile->timeScale > 0)? theFile->timeScale : GetMovieTimeScale(theFile->theMovie),
				dataRef, dataRefType
		);
	}
	else{
		*textMedia = NULL;
		Log( qtLogPtr, "Failure creating internal dataRef (%d) or textTrack (%d)\n", txtErr, GetMoviesError() );
		if( dataRef && dataRef != theFile->odataRef ){
			DisposeHandle(dataRef);
			dataRef = NULL;
		}
		if( *textTrack ){
			DisposeMovieTrack(*textTrack);
			*textTrack = NULL;
	}
	}
	if( *textMedia &&
	   (*textDesc = (TextDescriptionHandle)NewHandleClear(sizeof(TextDescription)))
	){
		(***textDesc).descSize = sizeof(TextDescription);
		(***textDesc).dataFormat = TextMediaType;
		(***textDesc).displayFlags = dfClipToTextBox|dfAntiAlias;
		(***textDesc).defaultTextBox = textRect;
		(***textDesc).textJustification = teCenter;
		(***textDesc).bgColor = colour;
	}
	else{
		Log( qtLogPtr, "Failure creating textMedia or SampleDescription (%d)\n", GetMoviesError() );
	}
	if( !*textMedia && *textTrack ){
		DisposeMovieTrack(*textTrack);
		*textTrack = nil;
	}
	else if( trackName ){
		MetaDataHandler( theFile->theMovie, *textTrack, akDisplayName, (char**) &trackName, NULL, ", " );
	}
	return txtErr;
}

OSErr AddChapterTitle( struct QI2MSourceFileRecord *theFile, Track refTrack, const char *name,
				  TimeValue time, TimeValue duration )
{ TimeValue chapTime;
  unsigned long strLength;
  OSErr txtErr;
  Boolean mediaOpen = FALSE;
	if( !theFile->theChapTrack ){
		txtErr = initTextTrackWithMedia( theFile, &theFile->theChapTrack, &theFile->theChapMedia,
					  Long2Fix(0), Long2Fix(0), "Chapter Track", &theFile->theChapDesc
			);
		if( txtErr == noErr ){
			txtErr = BeginMediaEdits(theFile->theChapMedia);
			if( txtErr != noErr ){
				theFile->theChapMedia = nil;
				Log( qtLogPtr, "error doing BeginMediaEdits(textMedia): %d\n", txtErr );
		}
	}
	if( txtErr != noErr ){
			Log( qtLogPtr, "AddChapterTitle(\"%s\",\"%s\"): error creating chapter title text track (%d)\n",
			    P2Cstr( (void*) theFile->theFileSpec.name ), name, txtErr
		);
		// just to be sure:
			theFile->theChapTrack = nil;
	}
		else{
			// chapter tracks need not be enabled, they're not to be seen
			SetTrackEnabled( theFile->theChapTrack, FALSE );
			{ char *fName = P2Cstr( theFile->theFileSpec.name );
				MetaDataHandler( theFile->theMovie, theFile->theChapTrack, akSource,
							 &fName, NULL, ", "
				);
	}
			}
		}

	// duration must be 1 for the moment!
	duration = 1;

	{ OSErr dErr;
		if( time < GetTrackDuration(theFile->theChapTrack) ){
			if( (txtErr = BeginMediaEdits(theFile->theChapMedia)) == noErr ){
				mediaOpen = TRUE;
			}
			dErr = DeleteTrackSegment( theFile->theChapTrack, time, duration );
			Log( qtLogPtr, "AddChapterTitle(\"%s\",%d,%d) 'make-place' deletion from %d to %d (err=%d)\n",
			    name, (int)time, (int)duration,
			    (int) time, (int) (time + duration), (int) dErr
			);
		}
	}
	if( theFile->theChapTrack && (mediaOpen || (txtErr = BeginMediaEdits(theFile->theChapMedia) == noErr)) ){
	  MediaHandler cmh = GetMediaHandler(theFile->theChapMedia);
		strLength = strlen(name);
		txtErr = TextMediaAddTextSample( cmh, (Ptr) name, strLength,
			   0, 0, 0, NULL, NULL,
			   (**(theFile->theChapDesc)).textJustification,
			   &((**(theFile->theChapDesc)).defaultTextBox),
			   (**(theFile->theChapDesc)).displayFlags, 0, 0, 0, NULL,
			   duration, &chapTime
		);
		if( txtErr == noErr ){
			if( (txtErr = EndMediaEdits(theFile->theChapMedia)) == noErr ){
				txtErr = InsertMediaIntoTrack( theFile->theChapTrack, time, chapTime,
								duration, fixed1
				);
				if( txtErr != noErr ){
					Log( qtLogPtr, "AddChapterTitle::InsertMediaIntoTrack(theChapTrack) -> %d\n", txtErr );
				}
				else{
					if( refTrack &&
					   GetTrackReference( theFile->theChapTrack, kTrackReferenceChapterList, 1 ) != refTrack
					){
						if( GetTrackReferenceCount( theFile->theChapTrack, kTrackReferenceChapterList ) ){
							txtErr = SetTrackReference( refTrack, theFile->theChapTrack,
												  kTrackReferenceChapterList, 1
							);
						}
						else{
							txtErr = AddTrackReference( refTrack, theFile->theChapTrack,
												  kTrackReferenceChapterList, NULL
							);
						}
						if( txtErr != noErr ){
							Log( qtLogPtr, "AddChapterTitle::SetTrackReference(theChapTrack) -> %d\n", txtErr );
						}
					}
					if( !txtErr ){
						Log( qtLogPtr, "Added chapter \"%s\" at t=%gs for %d frames\n",
						    name, ((double) time)/theFile->timeScale, (int) duration
						);
						UpdateMovie(theFile->theMovie);
					}
				}
			}
			else{
				Log( qtLogPtr, "AddChapterTitle::EndMediaEdits(theChapMedia) -> %d\n", txtErr );
			}
		}
		else{
			Log( qtLogPtr, "AddChapterTitle::TextMediaAddTextSample(theChapMediaHandler) -> %d\n", txtErr );
		}
	}
	return txtErr;
}

OSErr GetTrackTextAtTime( Track _fTrack, TimeValue _fTime, char **foundText,
					  TimeValue *sampleStartTime, TimeValue *sampleDuration )
{ Handle textHandle = (Handle) -1;
  TimeValue textTime, sampleTime = TrackTimeToMediaTime(_fTime, _fTrack);
  long textHSize, textSamples = 0;
  OSErr textErr;
	if( (textHandle = NewHandleClear(0)) ){
		textErr = GetMediaSample( GetTrackMedia(_fTrack), textHandle, 0, &textHSize,
					sampleTime, &textTime, NULL, NULL, NULL,
					0, &textSamples, NULL
		);
		if( textErr == noErr ){
		  char *text = (char*) *textHandle;
			// for text media samples, the returned handle is a 16-bit field followed
			// by the actual text data; textHSize is the full length (string + sizeof(short)
			// so allocating 1 byte extra we're safe.
			text = &text[sizeof(short)];
			if( foundText && *foundText == NULL ){
				if( (*foundText = calloc( textHSize + 1, sizeof(char) )) ){
					strncpy( *foundText, text, textHSize-sizeof(short) );
					// no need for the terminating nullbyte as we used calloc()
				}
				else{
					textErr = errno;
				}
			}
			// get the sample's "interesting" time value:
			GetTrackNextInterestingTime( _fTrack, nextTimeEdgeOK|nextTimeMediaSample,
								   sampleTime, -fixed1, &sampleTime, NULL
			);
			if( sampleStartTime ){
				*sampleStartTime = sampleTime;
				textErr = GetMoviesError();
			}
			if( sampleDuration ){
				// get the sample's "interesting duration":
				GetTrackNextInterestingTime( _fTrack, nextTimeEdgeOK|nextTimeMediaSample,
									   sampleTime, -fixed1, NULL, sampleDuration
				);
				textErr = GetMoviesError();
			}
		}
		DisposeHandle(textHandle);
	}
	else{
		textErr = (textHandle == (Handle)-1)? MemError() : paramErr;
	}
	return textErr;
}

OSErr GetMovieChapterTextAtTime( struct QI2MSourceFileRecord *theFile, TimeValue time, char **text )
{ TimeValue cTime;
  OSErr err = invalidTime;
	GetTrackNextInterestingTime( theFile->theChapTrack, nextTimeMediaSample|nextTimeEdgeOK,
						   time, fixed1, &cTime, NULL );
	if( cTime != -1 && time == cTime ){
		if( text ){
			*text = NULL;
			err = GetTrackTextAtTime( theFile->theChapTrack, cTime, text, NULL, NULL );
		}
		else{
			err = noErr;
		}
	}
	return err;
}



