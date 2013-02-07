/*
 *  QTils.h
 *  QTImage2Mov
 *
 *  Created by René J.V. Bertin on 20100723.
 *  Copyright 2010 INRETS / RJVB. All rights reserved.
 *
 */

#ifndef _TRACKUTILS_H

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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#if defined(_QTIMAGE2MOV_H) || defined(__QUICKTIME__) || defined(__QUICKTIMECOMPONENTS__)

extern OSErr AnchorMovie2TopLeft( Movie theMovie );

// define and install a rectangular clipping region to the current track to mask off all but the requested
// quadrant (channel). If requested, also flip the track horizontally.
extern void ClipFlipCurrentTrackToQuadrant( QI2MSourceFile theFile, short quadrant, short hflip );
extern OSErr QTStep_GetStartTimeOfFirstVideoSample (Movie theMovie, TimeValue *theTime);
extern void SetMovieStartTime( Movie theMovie, TimeValue startTime, int changeTimeBase );
extern OSErr MaybeShowMoviePoster( Movie theMovie );

// Create an 'atomic' dataRef/dataRefType pair, i.e. one that does not refer to an external data source
extern OSErr DataRefFromVoid( Handle *dataRef, OSType *dataRefType );

// Given the URL (filename) in URL, generate a dataRef/dataRefType pair that can be passed e.g. to
// NewMovieFromDataRef(). The URL is adapted as necessary on Mac OS X and MS Windows; a pointer
// to the (potentially changed) string is returned in <*URL>. Thus, the caller should free the URL string if
// no longer needed and when *URL has changed after return from DataRefFromURL().
extern OSErr DataRefFromURL( const char **URL, Handle *dataRef, OSType *dataRefType );
// create new media for theTrack in theFile, of the specified type, for in-memory content (i.e. not referring to any file)
extern OSErr NewImageMediaForMemory( QI2MSourceFile theFile, Track theTrack, OSType mediaType );
// create new media for theTrack in theFile, of the specified type and referring to theURL
extern OSErr NewImageMediaForURL( QI2MSourceFile theFile, Track theTrack, OSType mediaType, char *theURL );
extern OSErr CopyMovieTracks( Movie dstMovie, Movie srcMovie );
extern int CopyMovieMetaData( Movie dstMovie, Movie srcMovie );
extern int CopyTrackMetaData( Movie dstMovie, Track dstTrack, Movie srcMovie, Track srcTrack );
extern OSErr MovieGetVideoMediaAndMediaHandler( Movie inMovie, Track *inOutVideoTrack, Media *outMedia, MediaHandler *outMediaHandler );

// obtain a data reference to the requested file. That means we first need to construct the full
// pathname to the file, which entails guessing whether or not theURL is already a full path.
extern OSErr NewMovieFromURL( Movie *newMovie, short flags, short *id, const char *URL,
					    Handle *dataRef, OSType *dataRefType );
extern OSErr CreateMovieStorageFromURL( const char *theURL, OSType creator, ScriptCode scriptTag, long flags,
						   Handle *dataRef, OSType *dataRefType, CSBackupHook *cbHook,
						   DataHandler *outDataHandler, Movie *newMovie );
extern OSErr SaveMovieAsRefMov( const char *dstURL, QI2MSourceFile srcFile, Movie *theNewMovie );
extern OSErr FlattenMovieToURL( const char *dstURL, QI2MSourceFile srcFile, Movie *theNewMovie );

extern TimeScale GetTrackTimeScale( Track theTrack );
extern OSErr GetMediaStaticFrameRate( Media inMovieMedia, double *outFPS, long *frames, double *duration );
extern OSErr GetTrackStaticFrameRate( Track theTrack, double *outFPS, long *frames, double *duration );
extern OSErr GetMovieStaticFrameRate( Movie theMovie, double *outFPS, double *fpsTC, double *fpsMedia );

extern void GetMovieDimensions( Movie theMovie, Fixed *width, Fixed *height );

#endif


typedef enum AnnotationKeys { akAuthor='auth', akComment='cmmt', akCopyRight='cprt', akDisplayName='name',
	akInfo='info', akKeywords='keyw', akDescr='desc', akFormat='orif', akSource='oris',
	akSoftware='soft', akWriter='wrtr', akYear='year', akCreationDate='@day', akTrack='@trk'
} AnnotationKeys;


#if defined(_QTIMAGE2MOV_H) || defined(__QUICKTIME__) || defined(__QUICKTIMECOMPONENTS__)
extern OSErr MetaDataHandler( Movie theMovie, Track toTrack,
					    AnnotationKeys key, char **Value, char **Lang, char *separator );
#endif

struct QI2MSourceFileRecord;
struct QI2MSourceImageRecord;

extern short AddMetaDataStringToCurrentTrack( struct QI2MSourceFileRecord *theFile,
									AnnotationKeys key, const char *value, const char *lang );
extern short AddMetaDataStringToMovie( struct QI2MSourceFileRecord *theFile,
									AnnotationKeys key, const char *value, const char *lang );
extern short GetMetaDataStringFromCurrentTrack( struct QI2MSourceFileRecord *theFile,
								AnnotationKeys key, char **value, char **lang );
extern short GetMetaDataStringFromMovie( struct QI2MSourceFileRecord *theFile,
									  AnnotationKeys key, char **value, char **lang );

// add a TimeCode track to theFile, associated with the current ImageTrack and ImageMedia.
// The timecode specification can be broken up into N chunks with each the specified starting time, duration
// and number of frames. Usually a single chunk will be enough, except when frame rate varies during the movie;
// as it can when timePadding is in effect.
extern short AddTimeCodeTrack( struct QI2MSourceFileRecord *theFile, struct QI2MSourceImageRecord *theImage,
							long trackStart, int N, double *StartTimes,
							double *durations, double *frequencies, size_t *frames, const char *theURL );
#if defined(_QTIMAGE2MOV_H) || defined(__QUICKTIME__) || defined(__QUICKTIMECOMPONENTS__)
extern OSErr IsolateTCTracks( struct QI2MSourceFileRecord *theFile, QI2MSourceImage theImage, int TCenable,
						int tctracksZeroWidth, int TXTenableToo );
extern OSErr initTextTrackWithMedia( QI2MSourceFile theFile, Track *textTrack, Media *textMedia,
						Fixed defWidth, Fixed defHeight, const char *trackName,
						TextDescriptionHandle *textDesc );
extern OSErr AddChapterTitle( struct QI2MSourceFileRecord *theFile, Track refTrack, const char *name,
						TimeValue time, TimeValue duration );
extern OSErr GetMovieChapterTextAtTime( struct QI2MSourceFileRecord *theFile, TimeValue time, char **text );
#endif

/*!
	201011116: TimeCode track management: the data used to define a TC track are stored in a separate table
	that can be used to check if a given media source (theURL) already has TC track for a given period.
	If so, one only needs to add a reference for that TC track to the track in which the media is being imported.
 */
typedef struct TCTrackInfo {
	char *theURL;
	void *theTCTrack;
	int N, current;
	double *StartTimes, *durations, *frequencies;
	size_t *frames;
	void* (*dealloc)(void *);
	struct TCTrackInfo *cdr;
} TCTrackInfo;

extern void *dispose_TCTrackInfo( void *ptr );

#define _TRACKUTILS_H
#endif // _TRACKUTILS_H
