/*!
	@file		QTImage2Mov.h

	Description: 	Slide Show Importer include file.
				Support for importing image sequences by RJVB 2007-2010
				Image sequence can be any video QuickTime can open, but also a file
				concatenating a series of jpeg images with describing metadata (.jpgs)
				or (ultimately) a Brigrade Electronics/Approtech .VOB file.

	Author:		Apple Developer Support, original code by Jim Batson of QuickTime engineering
				Extensions by RJVB 2007-2010

	Copyright: 	© Copyright 2001-2002 Apple Computer, Inc. All rights reserved.
				Portions by RJVB © Copyright 2007-2010 RJVB, INRETS
 */
/*
	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
				("Apple") in consideration of your agreement to the following terms, and your
				use, installation, modification or redistribution of this Apple software
				constitutes acceptance of these terms.  If you do not agree with these terms,
				please do not use, install, modify or redistribute this Apple software.

				In consideration of your agreement to abide by the following terms, and subject
				to these terms, Apple grants you a personal, non-exclusive license, under Apple’s
				copyrights in this original Apple software (the "Apple Software"), to use,
				reproduce, modify and redistribute the Apple Software, with or without
				modifications, in source and/or binary forms; provided that if you redistribute
				the Apple Software in its entirety and without modifications, you must retain
				this notice and the following text and disclaimers in all such redistributions of
				the Apple Software.  Neither the name, trademarks, service marks or logos of
				Apple Computer, Inc. may be used to endorse or promote products derived from the
				Apple Software without specific prior written permission from Apple.  Except as
				expressly stated in this notice, no other rights or licenses, express or implied,
				are granted by Apple herein, including but not limited to any patent rights that
				may be infringed by your derivative works or by other works in which the Apple
				Software may be incorporated.

				The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
				WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
				WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
				PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
				COMBINATION WITH YOUR PRODUCTS.

				IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
				CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
				GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
				ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION
				OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
				(INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
				ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

	Change History (most recent first):

	   <2>		3/22/02		SRK				Carbonized/Win32
	   <1>	 	4/01/01		Jim Batson		first file
*/

#ifndef _QTIMAGE2MOV_H

// ---------------------------------------------------------------------------------
//		• Includes •
// ---------------------------------------------------------------------------------

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
    #include <QuickTime/QuickTime.h>
#else
    #include <QuickTimeComponents.h>
#endif

#if TARGET_OS_MAC
#	include "ProgressDialog.h"
#endif

// ---------------------------------------------------------------------------------
//		• Structure and Enum Definitions •
// ---------------------------------------------------------------------------------
/*!
	enum defining all supported XML attributes, all elements confounded
 */
typedef enum xmlAttrs {
	attr_height = 1,	//!< optional height for element_slideshow and element_import
	attr_width,		//!< optional width for element_slideshow and element_import
	attr_asksave,		//!< element_slideshow and element_import: optional flag to present a Save-As dialog or not
	attr_autosave,		//!< element_slideshow and element_import: optional flag to export the resulting movie to an autonomous QT Movie
	attr_autosavename,	//!< if set, the name for the autosave movie file
	attr_src,			//!< compulsory source URL specifier for image, sequence and audio elements
	attr_freq,		//!< opt. frequence specifier for sequences
	attr_interval,		//!< opt. frame importing interval for sequences
	attr_maxframes,	//!< opt. max frame count import for sequences
	attr_starttime,	//!< opt. flag to heed sequence (wallclock) start time, or in-movie start time for chapters
	attr_duration,		//!< opt. chapter duration.
	attr_timepad,		//!< opt. time padding specifier for sequences
	attr_hidetc,		//!< opt. flag to hide a sequence's TimeCode track
	attr_hidets,		//!< opt. flag to hide a sequence's timeStamp track
	attr_newchapter,	//!< opt. flag to start a new chapter for the current sequence or image
	attr_channel,		//!< opt. channel specifier to isolate one view of a quad-camera sequence
	attr_hflip,		//!< opt. flag to mirror a sequence around its vertical midline
	attr_vmgi,		//!< opt. flag to use the VMGI structure while importing Brigage VOD sequences
	attr_transh,		//!< opt. horizontal translation for images and sequences
	attr_transv,		//!< opt. vertical translation for images and sequences
	attr_reltransh,	//!< opt. size-relative horizontal translation for images and sequences
	attr_reltransv,	//!< opt. size-relative vertical translation for images and sequences
	attr_dur,			//!< duration in seconds for images
	attr_mdur,		//!< duration in milliseconds for images
	attr_ismovie,		//!< opt. : image is in fact a movie
	attr_asmovie,		//!< opt. : import a sequence as a movie using QuickTime without doing our own extension-dependent processing
//	attr_type,
//	attr_scale,		//!< opt. scale specifier for sequences
	attr_description,	//!< opt. description for sequences
	attr_log,			//!< opt. flag to activate import logging for sequences
	attr_lang,		//!< opt. language specifier for description elements
	attr_txt,			//!< compulsory text for description elements
	attr_title,		//!< compulsory title for chapter elements
	attr_fcodec,		//!< codec that ffmpeg will use to transcode VOD mpg4 video, if relevant
	attr_fbitrate,		//!< bitrate that ffmpeg will use to transcode VOD mpg4 video, if relevant
	attr_fsplit,		//!< split input MPEG4 VOD file into 4 tracks
	SLIDESHOW_NUM_ATTRIBUTES,
	attr_NOTFOUND = SLIDESHOW_NUM_ATTRIBUTES
} xmlAttrs;

enum {
	attributeNotFound = -1
};

/*!
	the list of supported XML elements
 */
typedef enum xmlElems {
	element_slideshow = 1,	//!< historical root element
	element_import,		//!< preferred root element
	element_image,			//!< an image element
	element_sequence,		//!< a sequence element: a movie or some other image-sequence container
	element_audio,			//!< an audio element. Currently a single audio element is supported only.
	element_chapter,		//!< a chapter element
	element_description,	//!< a description element, applied at the movie level
	SLIDESHOW_NUM_ELEMENTS,
	element_NOTFOUND = SLIDESHOW_NUM_ELEMENTS
} xmlElems;


typedef struct {
	MovieImportComponent		theMovieImportComponent;
	FSSpec					theFileSpec;
	struct QI2MSourceFileRecord	*theFile;
} QTImage2MovGlobalsRecord, *QTImage2MovGlobals;


enum {
	kQI2MSourceImageKind = 'imag',
	kQI2MSourceAudioKind = 'audi',
	kQI2MSourceDescrKind = 'desc',
	kQI2MSourceChaptKind = 'chap',
};

// RJVB: added support for duration specifications in milliseconds
// added support for reading movies, and the special case 'sequence'.

/*!
	structure holding all relevant parameters for importing images and sequences
 */
typedef struct QI2MSourceImageRecord {
	// RJVB
	Ptr			theURL;
	double		duration, mduration;
	short		channel, hflip, clipflipDone,
				interval, ignoreRecordStartTime, timePadding, hideTC, hideTS, useVMGI;
	long			maxFrames, transH, transV;
	double		frequency, relTransH, relTransV;
	short		isMovie, isSequence, asMovie, newChapter;
	StringPtr		src;		//!< the file to be imported
	Track		theTrack;	//!< initialised during import: points to the track into which the image is stored.
						//!< Only used for 'images' that get imported into a single track!
						//!<

	// members used during import
	Fixed		discoveredWidth, displayWidth;	//!< complete and display width
	Fixed		discoveredHeight, displayHeight;	//!< complete and display height

	short		dataRefIndex;	//!< used by the various *AddSample* functions
	TimeValue		sampleTime;	//!< TimeValue at which this element was inserted into the current movie track
	StringPtr		description;	//!< element description
	StringPtr		ffmpCodec;	//!< Codec to be used by ffmpeg (if relevant)
	StringPtr		ffmpBitRate;	//!< bitrate to be used by ffmpeg (if relevant and codec != copy)
	Boolean		ffmpSplit;	//!< import MPEG4 VOD files split into 4 tracks, one for each camera
	Boolean		log;
} QI2MSourceImageRecord;
typedef	QI2MSourceImageRecord	*QI2MSourceImage;

typedef struct QI2MSourceMovieRecord {
	StringPtr	src;
	Boolean	dontScaleImagesToThisMovie;
} QI2MSourceMovieRecord, *QI2MSourceMovie;

/*!
	structure holding all relevant parameters for adding QuickTime chapter entries
 */
typedef struct QI2MSourceChapterRecord {
	StringPtr	title;
	double	startTime, duration;
} QI2MSourceChapterRecord, *QI2MSourceChapter;

/*!
	base element of the internal representation of the XML qi2m file
 */
typedef struct QI2MSourceElementRecord {
	struct QI2MSourceElementRecord	*nextElement;

	xmlElems						elementKind;

	union{
		QI2MSourceImageRecord		image;
		QI2MSourceMovieRecord		movie;
		QI2MSourceChapterRecord		chapterMark;
	};
	StringPtr						description, language;
} QI2MSourceElementRecord, *QI2MSourceElement;

typedef struct CSBackupHook {
#if TARGET_OS_MAC
	CFURLRef				urlRef;			//!< another reference to the open file
	Boolean				bakExcl,			//!< CoreBackup exclude state before opening
						bakExclPath;		//!< CoreBackup excludeByPath state before opening
#else
	size_t				dum;
#endif
} CSBackupHook;

/*!
	internal representation of the movie-under-construction
 */
typedef struct QI2MSourceFileRecord {
	FSSpec				theFileSpec;
	const char			*cwd, *path;	//!< current working directory: where the current input qi2m file lives
	long					movieHeight;
	long					movieWidth;
	Movie				theMovie;		//!< QuickTime movie object under construction
	struct {
		MovieProgressUPP	call;
		long				refcon;
	}					progressProc;	//!< reserved
	Handle				dataRef;		//!< Mac-style reference to the input qi2m file
	OSType				dataRefType;	//!< type of the dataRef - typically 'alis'
	char					*ofName;		//!< name of the output autoSave ref.movie if compiled with IMPORT_INTO_DESTREFMOV
	Handle				odataRef;		//!< Handle to the output autoSave ref.movie if compiled with IMPORT_INTO_DESTREFMOV
	OSType				odataRefType;	//!< type of the output odataRef
	DataHandler			odataHandler;	//!< the DataHandler corresponding to odataRef
	CSBackupHook			cbState;		//!< hook to exclude the open file from the CoreBackup service

	long					numImages;
	long					numAudioTracks;
	long					numTracks;
	int					askSave, autoSave;	//!< set through the attr_asksave and attr_autosave XML attributes
	StringPtr				autoSaveName;

	QI2MSourceElement		elements;		//!< linked list of QI2MSourceElement records
	long					numberMMElements, currentMMElement;

	// used during construction of movie
	Track				theImageTrack, currentTrack, theTCTrack, theChapTrack; //!< various track references
									//!< only theChapTrack refers to a unique (though optional) track
									//!< (containing the Chapter entries); the others are all either volatile
									//!< (theImageTrack) or refer to a current track that is kept updated.
									//!<
	Media				theChapMedia;	//!< media reference to the chapter track
	TextDescriptionHandle	theChapDesc;	//!< holds the reusable data for adding chapters
	struct {
		Media	Media;
		Handle	dataRef;
		OSType	dataRefType;
	}					theImage;		//!< the current image's dataRef and destination Media
	short				MustUseTrack,	//!< set if QuickTime passed us a destination track (importing into an existing movie)
						PosterIsSet;	//!< once we've set a poster frame
	TimeValue				startTime, Duration, prevDuration, timeScale;	//!< timing variables
	void					*TCInfoList;	//<! points to a database used for generating the TimeCode track(s)

//	QI2MSourceImage		thePreviousSource;		//!< source A for the transition
	void					*progressDialog;	//!< points to a progress dialog object if one is available.
	int					interruptRequested;
	QTImage2MovGlobals		theStore;		//!< nobody's business
} QI2MSourceFileRecord;
typedef	QI2MSourceFileRecord	*QI2MSourceFile;

// ---------------------------------------------------------------------------------
//		• Defines •
// ---------------------------------------------------------------------------------

// ---------------------------------------------------------------------------------
//		• Function Prototypes •
// ---------------------------------------------------------------------------------

char				*concat( const char *first, ... );
char				*tmpImportFileName( const char *theURL, const char *ext );
char				*P2Cstr( Str255 pstr );
char				*OSTStr( OSType type );
#if TARGET_OS_MAC
const char		*MacErrorString( OSErr err, const char **errComment );
#endif
OSErr			GetFilenameFromDataRef( Handle dataRef, OSType dataRefType, char *path, int maxLen, Str255 fileName );
OSErr			AddMacOSFileTypeDataRefExtension( Handle theDataRef, OSType theType );
int				DetermineOSType( Str255 theFilePString, char *theURL, OSType *type );

ComponentResult 	ReadQI2MSourceFile(QI2MSourceFile theFile);
ComponentInstance 	CreateXMLParserForQI2MSource();
ComponentResult	GetQI2MSourceAttributes(XMLAttribute *element, QI2MSourceFile theFile);
ComponentResult	GetAndSaveChapterMark(XMLElement *element, QI2MSourceFile theFile);
ComponentResult	GetAndSaveImage(XMLElement *element, QI2MSourceFile theFile);
ComponentResult	GetAndSaveSequence(XMLElement *element, QI2MSourceFile theFile);
ComponentResult	GetAndSaveAudioElement(XMLElement *element, QI2MSourceFile theFile);
ComponentResult	BuildMovie(QI2MSourceFile theQI2MSourceFile);
void 			DisposeQI2MSourceFile(QI2MSourceFile theFile);
void				DisposeAllElements(QI2MSourceFile theFile);
void				DisposeElementFields(QI2MSourceElement element);
void				AddQI2MSourceElement(QI2MSourceFile theFile, QI2MSourceElement theElement);
ComponentResult 	GetPStringAttribute(XMLAttributePtr attributes, xmlAttrs attributeType, StringPtr *theString);
ComponentResult 	GetStringAttribute(XMLAttributePtr attributes, xmlAttrs attributeType, StringPtr *theString);
ComponentResult 	GetIntegerAttribute(XMLAttributePtr attributes, xmlAttrs attributeType, long *theNumber);
ComponentResult 	GetShortAttribute(XMLAttributePtr attributes, xmlAttrs attributeType, short *theNumber);
//!
//!	NB NB NB: a "DoubleAttribute" is based on attributeValueKindCharString !!
//!
#ifndef	attributeValueKindDouble
#	define attributeValueKindDouble	attributeValueKindCharString
#endif
ComponentResult 	GetDoubleAttribute(XMLAttributePtr attributes, xmlAttrs attributeType, double *theNumber);
ComponentResult 	GetBooleanAttribute(XMLAttributePtr attributes, xmlAttrs attributeType, int *theBool);
long 			GetAttributeIndex(XMLAttributePtr attributes, xmlAttrs attributeType);
OSErr			AddImagesToMovie(QI2MSourceFile theFile, QI2MSourceImage theImage, TimeValue *theTime, TimeValue *theSampleTime, int allow_inMem);
OSErr			AddAudioToMovie(QI2MSourceFile theFile, QI2MSourceMovie theImage, TimeValue *theTime);
#ifndef max
	long			max(long num1, long num2);
#endif
Boolean 			isAbsoluteURL(char *theURL);
OSErr 			CreateDataRefFromURL(char* theURL, Handle baseDRef, OSType baseDRefType, Handle *dref, OSType *drefType);
Handle 			CreateHandleDataRef();
OSErr			URLtoDataRef( char *theURL, QI2MSourceFile theFile, Handle *dataRef, OSType *dataRefType );

#include "Logging.h"

#define _QTIMAGE2MOV_H
#endif
