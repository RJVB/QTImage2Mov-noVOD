/*!
	@file		QTImage2Mov.c

	Description: 	QT Image 2 Movie Importer component routines.
				Support for importing image sequences by RJVB 2007-2010
				Image sequence can be any video QuickTime can open, but also a file
				concatenating a series of jpeg images with describing metadata (.jpgs)
				or a Brigrade Electronics/Approtech .VOB file.

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

	   <3>		6,7/xx/10		RJVB				Handling image sequences and VOD files
	   <2>		3/22/02		SRK				Carbonized/Win32
	   <1>	 	4/01/01		Jim Batson		first file
*/

#ifdef _MSC_VER
#	define	_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES	1
#endif

#include "winixdefs.h"

#include "copyright.h"
IDENTIFY("QTImage2Mov Importer entry point for qi2m files");
C_IDENTIFY(COPYRIGHT);

/*!
	Set IMPORT_INTO_DESTREFMOV in order to do the import procedure directly into the ref.movie
	that will be created when the QI2M option autoSave=True . This has the advantage of creating
	text and TimeCode tracks that are considered to be file-resident instead of in-memory, which
	means that other movies referring to the ref.movie we create will not copy those tracks, but
	refer to our ref.movie. Esp. the timeStamp track can grow to a considerable size for longer VOD
	recordings.
 */
#define IMPORT_INTO_DESTREFMOV

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#if ! defined(_WINDOWS) && !defined(WIN32) && !defined(_MSC_VER)
#	include <unistd.h>
#	include <libgen.h>
#	include <sys/param.h>
#else
#	include <time.h>
#endif

#if __APPLE_CC__
#	include <Carbon/Carbon.h>
#	include <QuickTime/QuickTime.h>
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
#	define basename(p)	lbasename((p))
#else
#	include <ImageCodec.h>
#	include <TextUtils.h>
#	include <string.h>
#	include <Files.h>
#	include "FixMath.h"
#	include <Movies.h>
#	include <MediaHandlers.h>
#	include <direct.h>
#	ifdef _MSC_VER
#		define strncasecmp(a,b,n)	_strnicmp((a),(b),(n))
#		define strdup(s)			_strdup((s))

		const char *basename( const char *url )
		{ const char *c = NULL;
			if( url ){
				if( (c =  strrchr( url, '\\' )) ){
					c++;
				}
				else{
					c = url;
				}
			}
			return c;
		}
#	endif // _MSC_VER
#endif

// This is the file that initialises the SS_Log function on MSWin:
#define HAS_LOG_INIT
#if TARGET_OS_WIN32
#	include "Dllmain.h"
#endif

#include "QTImage2Mov.h"
#include "QTImage2MovConstants.h"
#include "QTImage2MovVersion.h"

#include "NaN.h"
#include "timing.h"

#include "AddImageFrames.h"
#include "QTils.h"

#ifndef MIN
#	define	MIN(a,b) (((a)<(b))?(a):(b))
#endif /* MIN */

// ---------------------------------------------------------------------------------
//		• ComponentDispatchHelper Definitions •
// ---------------------------------------------------------------------------------

#define MOVIEIMPORT_BASENAME()		QTImage2MovImport_
#define MOVIEIMPORT_GLOBALS() 		QTImage2MovGlobals storage

#define CALLCOMPONENT_BASENAME()	MOVIEIMPORT_BASENAME()
#define	CALLCOMPONENT_GLOBALS()		MOVIEIMPORT_GLOBALS()

#define COMPONENT_UPP_PREFIX()		uppMovieImport
#define COMPONENT_DISPATCH_FILE		"QTImage2MovDispatch.h"
#define COMPONENT_SELECT_PREFIX()  	kMovieImport

#ifdef __APPLE_CC__
#	include <Components_macosx.k.h>				// StdComponent's .k.h
#else
#	include <Components.k.h>
#endif

#if __APPLE_CC__
    #include <QuickTime/QuickTimeComponents.k.h>
    #include <QuickTime/ComponentDispatchHelper.c>
#else
    #include <QuickTimeComponents.k.h>
    #include <ComponentDispatchHelper.c>	// make our dispatcher and cando
#endif

#ifndef GLUE2
#	define GLUE2(a,b)	a##b
#endif

ComponentCall(SetProgressProc)
ComponentCall(GetLoadState)

char *concat(const char *first, ...)
{ va_list ap;
  int n= 0, tlen= 0;
  char *c= (char*) first, *buf= NULL;
	va_start(ap, first);
	do{
		tlen+= strlen(c)+ 1;
		n+= 1;
	}
	while( (c= va_arg(ap, char*)) != NULL );
	va_end(ap);
	if( n== 0 || tlen== 0 ){
		return( NULL );
	}
	if( (buf= calloc( tlen, sizeof(char) )) ){
		c= (char*) first;
		va_start(ap, first);
		do{
			strcat( buf, c);
		}
		while( (c= va_arg(ap, char*)) != NULL );
		va_end(ap);
	}
	return( buf );
}

#if TARGET_OS_MAC
	char *tmpImportFileName( const char *theURL, const char *ext )
	{ extern char *concat( const char*, ... );
	  char *FName = NULL;
	  char *tmpdir = getenv("TMPDIR"), *tmpName = concat( basename(theURL), "-QI2M-XXXXXX", NULL );
		if( !tmpdir ){
			tmpdir = "/tmp";
		}
		if( tmpName ){
			FName = concat( tmpdir, "/", mktemp(tmpName), ".", ext, NULL );
			free(tmpName);
		}
		return FName;
	}
#elif defined(TARGET_OS_WIN32)
	extern char *tmpImportFileName( const char *theURL, const char *ext );
#endif

char *P2Cstr( Str255 pstr )
{ static char cstr[257];
  int len = (int) pstr[0];

	strncpy( cstr, (char*) &pstr[1], len );
	cstr[len] = '\0';
	return(cstr);
}

char *OSTStr(OSType type )
{ static uint32_t ltype;
	ltype = EndianU32_BtoN(type);
	return (char*) &ltype;
}

#if TARGET_OS_MAC
const char *MacErrorString( OSErr err, const char **errComment )
{ static const char *missing = "[undocumented]";
  const char *errString = GetMacOSStatusErrorString(err);
	if( errComment ){
		*errComment = GetMacOSStatusCommentString(err);
		if( !*errComment ){
			*errComment = missing;
		}
	}
	return (errString)? errString : missing;
}
#endif

#if defined(_WINDOWS) || defined(WIN32) || defined(_MSC_VER)
static char *ProgName()
{ static char progName[260] = "";
	// obtain programme name: cf. http://support.microsoft.com/kb/126571
	if( __argv && !*progName ){
	  char *pName = strrchr(__argv[0], '\\'), *ext = strrchr(__argv[0], '.'), *c, *end;
		c = progName;
		if( pName ){
			pName++;
		}
		else{
			pName = __argv[0];
		}
		end = &progName[sizeof(progName)-1];
		while( pName < ext && c < end ){
			*c++ = *pName++;
		}
		*c++ = '\0';
	}
	return progName;
}
#endif

// ---------------------------------------------------------------------------------
//		• QTImage2MovOpen •
//
// Handle the Standard Component Manager Open Request
//
// Set up the components global data storage
// ---------------------------------------------------------------------------------
pascal ComponentResult QTImage2MovImport_Open(QTImage2MovGlobals store, ComponentInstance self)
{ ComponentResult err;

#if defined(_WINDOWS) || defined(WIN32) || defined(_MSC_VER)
//	if( !qtLog_Initialised ){
//		qtLogPtr = Initialise_Log( "QT QI2MSource Importer Log", ProgName() );
//		qtLog_Initialised = 1;
//	}
#else
	SwitchCocoaToMultiThreadedMode();
#endif

	// Allocate and initialise all globals to 0 by calling NewPtrClear.
	store = (QTImage2MovGlobals) NewPtrClear(sizeof(QTImage2MovGlobalsRecord));
	err = MemError();
	if (err) goto bail;

	store->theMovieImportComponent = self;

	SetComponentInstanceStorage(self, (Handle)store);

bail:
	if(
#if defined(_WINDOWS) || defined(WIN32) || defined(_MSC_VER)
	   qtLog_Initialised
#else
#	ifdef _PC_LOG_ACTIVE
	   PCLogActive()
#	else
	   1
#	endif
#endif
	){
		Log( qtLogPtr, "QTImage2MovImport_Open(%p) -- initialised; store=0x%lx err=%d\n",
		    self, (unsigned long) store, (int) err
		);
#ifdef _NSLOGGERCLIENT_H
		NSLogFlushLog();
#endif
	}
	return err;
}

// ---------------------------------------------------------------------------------
//		• QTImage2MovClose •
//
// Handle the Standard Component Manager Close Request
//
// Clean up the components global data storage
// ---------------------------------------------------------------------------------
pascal ComponentResult QTImage2MovImport_Close(QTImage2MovGlobals store, ComponentInstance self)
{
#if TARGET_OS_WIN32
  extern void UnloadOurSelves();
#endif
	if(
#if defined(_WINDOWS) || defined(WIN32) || defined(_MSC_VER)
	   qtLog_Initialised
#else
#	ifdef _PC_LOG_ACTIVE
	   PCLogActive()
#	else
	   1
#	endif
#endif
	){
		Log( qtLogPtr, "QTImage2MovImport_Close(%p) -- closing down; disposing of store 0x%lx\n",
		    self, (unsigned long) store
		);
	}
	if( store ){
		SetComponentInstanceStorage( self, (Handle)NULL );
		DisposePtr((Ptr)store);
	}
#if TARGET_OS_WIN32
// Don't. It unloads us, but also the process that called us, which is not what we want.
//	UnloadOurSelves();
#endif
	return noErr;
}

// ---------------------------------------------------------------------------------
//		• QTImage2MovVersion •
//
// Handle the Standard Component Manager Version Request
//
// Return the components version information
// ---------------------------------------------------------------------------------
pascal long QTImage2MovImport_Version(QTImage2MovGlobals store)
{
#pragma unused(store)
	Log( qtLogPtr, "QTImage2MovImport_Version=%ld\n", kQTImage2MovVersion );
	return kQTImage2MovVersion;
}

// NoXML is defined in QTImage2Mov-direct.c, to force it to be linked in even when it's
// part of libImage2Mov...
extern int NoXML;

//!
//!		• QTImage2MovFile •
//!
//! Import data from a file
//!
//! The file in this case will be our XML file, which we will use to construct our movie
//!
pascal ComponentResult QTImage2MovImport_File(QTImage2MovGlobals store,
									const FSSpec		*theFile,
									Movie 			theMovie,
									Track 			targetTrack,
									Track 			*usedTrack,
									TimeValue 		atTime,
									TimeValue 		*addedDuration,
									long 			inFlags,
									long 			*outFlags)
{ ComponentResult	err;
  AliasHandle		alias;

	err = QTNewAlias(theFile, &alias, true);
	if (err) goto bail;

	if( theFile ){
		store->theFileSpec = *theFile;
		Log( qtLogPtr, "QTImage2MovImport_File(\"%s\" calling MovieImportDataRef\n",
			P2Cstr( (void*) store->theFileSpec.name )
		);
	}
	else{
		store->theFileSpec.name[0] = 0;
	}

	err = MovieImportDataRef(store->theMovieImportComponent,
							(Handle)alias,
							rAliasType,
							theMovie,
							targetTrack,
							usedTrack,
							atTime,
							addedDuration,
							inFlags,
							outFlags);

	DisposeHandle((Handle)alias);

bail:
	return err;
}

OSErr GetFilenameFromDataRef( Handle dataRef, OSType dataRefType, char *path, int maxLen, Str255 fileName )
{ DataHandler dh;
  OSErr err = invalidDataRef;
  ComponentResult result;
	if( dataRef ){
		if( path && dataRefType == AliasDataHandlerSubType ){
		  CFStringRef outPath = NULL;

			err = QTGetDataReferenceFullPathCFString(dataRef, dataRefType, (QTPathStyle)kQTNativeDefaultPathStyle, &outPath);
			if( err == noErr ){
#if __APPLE_CC__
				CFStringGetFileSystemRepresentation( outPath, path, maxLen );
#else
				CFStringGetCString( outPath, path, maxLen, CFStringGetSystemEncoding() );
#endif
#ifdef DEBUG
				if( *path ){
					Log( qtLogPtr, "dataRef path=\"%s\"\n", path );
				}
#endif
			}
		}
		if( fileName ){
			result = OpenAComponent( GetDataHandler( dataRef, dataRefType, kDataHCanRead), &dh );
			if( result == noErr ){
				result = DataHSetDataRef( dh, dataRef );
				if( result == noErr ){
					result = DataHGetFileName( dh, fileName );
				}
			}
			else{
				fileName[0] = 0;
			}
		}
	}
	return err;
}

OSErr CreateImportMovieStorage( QI2MSourceFile theFile, const char *suffix )
{ OSType ftype;
  int extpos = strlen(theFile->path);
  OSErr err2;
  char *ofName = NULL;
	if( theFile->autoSaveName ){
		theFile->ofName = strdup(theFile->autoSaveName);
	}
	else{
		ofName = strdup(theFile->path);
		if( ofName ){
			DetermineOSType( NULL, ofName, &ftype );
			switch( ftype ){
				case 'jpgs':
				case 'QI2M':
				case 'QTSL':
					extpos = strlen(theFile->path) - 5;
					break;
				case 'VOBS':
					extpos = strlen(theFile->path) - 4;
					break;
				default:
					extpos = strlen(theFile->path) - 1;
			}
			ofName[extpos] = '\0';
			if( theFile->ofName ){
				free(theFile->ofName);
			}
			theFile->ofName = concat( ofName, suffix, ".mov", NULL );
			free(ofName);
			ofName = NULL;
		}
	}

	if( theFile->ofName ){
		err2 = CreateMovieStorageFromURL( theFile->ofName, 'TVOD',
			   smCurrentScript, createMovieFileDeleteCurFile|createMovieFileDontCreateResFile,
			   &theFile->odataRef, &theFile->odataRefType, &theFile->cbState,
			   &theFile->odataHandler, &theFile->theMovie
		);
		Log( qtLogPtr, "CreateMovieStorageFromURL('%s') for creating the %s ref.movie returned %d\n",
		    theFile->ofName, (theFile->autoSave)? "autoSave" : "import", err2
		);
		if( err2 != noErr ){
			if( theFile->odataRef ){
				// 20121110
				DisposeHandle(theFile->odataRef);
				theFile->odataRef = NULL;
			}
			if( theFile->ofName ){
				free(theFile->ofName);
			}
		}
		else{
			// 20121110
			SetMovieDefaultDataRef( theFile->theMovie, theFile->odataRef, theFile->odataRefType );
//			SetMovieDefaultDataRef( theMovie, theFile->odataRef, theFile->odataRefType );
		}
	}
	else{
		err2 = MemError();
	}
	return err2;
}

#pragma mark --- ---
//!
//!		• QTImage2MovImport_DataRef •
//!
//! Import data from a data reference
//!
//! The data reference in this case will be our XML (".qi2m") data, which we will use to
//! construct our movie
//!
pascal ComponentResult QTImage2MovImport_DataRef(	QTImage2MovGlobals 	store,
												Handle 					dataRef,
												OSType 					dataRefType,
												Movie 					theMovie,
												Track 					targetTrack,
												Track 					*usedTrack,
												TimeValue 				atTime,
												TimeValue 				*addedDuration,
												long 					inFlags,
												long 					*outFlags )
{
#pragma unused(atTime, inFlags)

  ComponentResult err = noErr;
  QI2MSourceFile	theFile = nil;
  char path[1024];
  char cwd[1024+1];

#if defined(_WINDOWS) || defined(WIN32) || defined(_MSC_VER)
	if( !qtLog_Initialised ){
		qtLogPtr = Initialise_Log( "QT QI2MSource Importer Log", ProgName() );
		qtLog_Initialised = 1;
	}
#endif

	if (dataRef == nil || theMovie == nil )
	{
		err = paramErr;
		goto bail;
	}

	if (outFlags)
	{
		*outFlags = 0;
	}

#ifdef __APPLE_CC__
	if( !getwd( cwd ) )
#elif defined(_MSC_VER)
	if( !_getcwd( cwd, sizeof(cwd) ) )
#endif
	{
		Log( qtLogPtr, "Can't determine current working directory (%s)\n", cwd );
		cwd[0] = '\0';
	}
	// RJVB: try to obtain the filename and absolute path from the dataRef, as this may be needed later.
	{ Str255 fileName;
		path[0] = '\0';
		err = GetFilenameFromDataRef( dataRef, dataRefType, path, sizeof(path)/sizeof(char), fileName );
		if( store->theFileSpec.name[0] == 0 && fileName[0] && fileName[0] < 63 ){
			memcpy( store->theFileSpec.name, fileName, sizeof(store->theFileSpec.name) );
		}
	}

	// initialize
	theFile = (QI2MSourceFile)NewPtrClear(sizeof(QI2MSourceFileRecord));
	err = MemError();
	if (err) goto bail;

	// copy XML source data ref and other information needed during import
	err = HandToHand(&dataRef);
	if (err) goto bail;
	theFile->theFileSpec = store->theFileSpec;
	theFile->dataRef = dataRef;
	theFile->path = (const char*) strdup(path);

	theFile->dataRefType = dataRefType;
	// 20110323: theFile->theMovie used to be initialised here; moved to after the qi2m file parsing.

	theFile->startTime = -1;
	SetMovieProgressProc( theFile->theMovie, (MovieProgressUPP)-1L, 0);
	GetMovieProgressProc( theFile->theMovie, &theFile->progressProc.call, &theFile->progressProc.refcon );
//	Log( qtLogPtr, "progressProc @ 0x%p\n", theFile->progressProc.call );
//	MovieImportSetProgressProc( theFile->theMovie, (MovieProgressUPP)-1L, 0);
	if( (inFlags & movieImportMustUseTrack) == movieImportMustUseTrack ){
		// NB: not tested!!
		theFile->MustUseTrack = TRUE;
		theFile->currentTrack = theFile->theImageTrack = targetTrack;
		Log( qtLogPtr, "NB: importing into specified targetTrack; untested functionality!\n" );
	}
	if( *path ){
	  // change to the directory where the input file resides, so relative source specifications therein
	  // will actually work! This can be especially important when opening multiple qi2m files in different
	  // directories.
#ifdef __APPLE_CC__
		if( !chdir( dirname(path) ) ){
			theFile->cwd = (const char*) strdup( dirname(path) );
		}
#elif defined(_MSC_VER)
		{ char *c = strrchr( path, '\\' );
			if( c[-1] == ':' ){
			  // 20110106: STUPID STUPIP MSWin! _chdir() behaves like the age-old MSDos shell command:
			  // _chdir("d:\foo") changes into directory foo on disk d:, but
			  // _chdir("d:") does not change into the root of disk D: ; it also does not return an error.
			  // _chdir("d:\") is the command to change into the root of disk d:
				c++;
			}
			if( c && *c && c[1] != '\0' ){
			  char pc = *c;
				*c = '\0';
				if( !_chdir( path ) ){
					theFile->cwd = (const char*) strdup(path);
				}
				*c = pc;
			}
			else{
				if( !_chdir( path ) ){
					theFile->cwd = (const char*) strdup(path);
				}
			}
		}
#endif
	}
	if( !theFile->cwd && *cwd ){
		theFile->cwd = (const char*) strdup(cwd);
	}
	store->theFile = theFile;
	theFile->theStore= store;

	init_HRTime();

	theFile->numberMMElements = 0;

	// parse the QI2MSource file. The way to deal with extensions and try to parse the file as XML
	// isn't very clever.
	// MacErrors.h
	if( NoXML ){
		// we don't want to try to parse XML? Fine, simulate an appropriate QuickTime error, and if we are not
		// reading a jpgs or VOD file, that is the error that QuickTime will see
		err = codecUnimpErr;
	}
	else{
		// try to parse as if we're dealing with an XML file; on failure, check to see if it's a format
		// we know how to handle directly.
		err	= ReadQI2MSourceFile(theFile);
	}
	if( err ){
	  OSType dtype;
		// RJVB: theFile is not an XML slideshow file. Let's see if it's some other format that we support,
		// and for which we can construct a standard image QI2MSourceElement, as if we'd read it from XML:
		if( (*path || store->theFileSpec.name[0]) && DetermineOSType( store->theFileSpec.name, NULL,  &dtype )
		    && (dtype=='jpgs' || dtype == 'VOBS')
		){ QI2MSourceElement theTempImage = nil;
		  // prepare an image sequence element manually, so BuildMovie() can be called as if
		  // we just read an XML file.
			theTempImage = (QI2MSourceElement) NewPtrClear(sizeof(QI2MSourceElementRecord));
			err = MemError();
			if( theTempImage && err == noErr ){
				theTempImage->elementKind = kQI2MSourceImageKind;
				theTempImage->image.isMovie = 0;
				theTempImage->image.asMovie = 0;
				theTempImage->image.isSequence = 1;
				// import all frames!
				theTempImage->image.interval = 1;
				theTempImage->image.ignoreRecordStartTime = 0;
				theTempImage->image.timePadding = 0;
				theTempImage->image.hideTC = 0;
				theTempImage->image.useVMGI = 1;
				theTempImage->image.newChapter = 1;
				// import all channels
				theTempImage->image.channel = -1;
				theTempImage->image.hflip = 0;
				if( *path ){
					theTempImage->image.src = (StringPtr)NewPtr(2);
					err = MemError();
					theTempImage->image.theURL = (Ptr) NewPtr(strlen(path));
					err = MemError();
				}
				else{
					theTempImage->image.src = (StringPtr)NewPtr(store->theFileSpec.name[0]+1);
					err = MemError();
					theTempImage->image.theURL = NULL;
				}
				if( err == noErr ){
					if( theTempImage->image.theURL ){
						strcpy( theTempImage->image.theURL, path );
						theTempImage->image.src[0] = 0;
					}
					else{
						BlockMove( store->theFileSpec.name, theTempImage->image.src, store->theFileSpec.name[0]+1);
					}
					AddQI2MSourceElement(theFile, theTempImage);
					theFile->numberMMElements += 1;
					theFile->numImages += 1;
				}
				else{
					DisposeElementFields(theTempImage);
					DisposePtr( (Ptr) theTempImage);
				}
			}
		}
		else{
			goto bail;
		}
	}
#ifdef AUTOSAVE_REPLACES
	// 20101126: we do the import into a temporary movie. This allows us to replace the movie by the autoSave'd
	// reference movie, for instance.
	theFile->theMovie = NewMovie(0);
#else
#	ifdef IMPORT_INTO_DESTREFMOV
	if( theFile->autoSave ){
		CreateImportMovieStorage( theFile, "" );
	}
#	endif
	if( !theFile->theMovie ){
		theFile->theMovie = theMovie;
	}
#endif

	// build a movie
	err	= BuildMovie(theFile);

	if( err ){
		goto bail;
	}

bail:
#if TARGET_OS_WIN32
	if( err == userCanceledErr ){
		// 20101110: an import error may leave the user with a background process (QT Player...) without
		// a way to interact with it other than by opening another movie through the Explorer. Thus, we
		// arrange to display the movie as far as it has been imported...
		err = noErr;
	}
#endif //TARGET_OS_WIN32

	if( theFile ){
		if( theFile->Duration == 0 || theFile->numTracks == 0 ){
			Log( qtLogPtr, "Movie duration=%g, no. of tracks=%ld\n",
			    ((double)theFile->Duration)/((double)theFile->timeScale), theFile->numTracks
			);
			DisposeMovie( theFile->theMovie );
			theFile->theMovie = nil;
			DisposeMovie( theMovie );
			theMovie = nil;
			if( err == noErr ){
				err = (theFile->Duration==0)? invalidDuration : noMovieFound;
			}
		}

		if( theFile->theMovie && theMovie != theFile->theMovie ){
			// 20101126: Now insert the newly created movie into the destination movie:
			SetMovieTimeScale( theMovie, GetMovieTimeScale(theFile->theMovie) );
			err= InsertMovieSegment( theFile->theMovie, theMovie, 0, GetMovieDuration(theFile->theMovie), 0 );
			Log( qtLogPtr, "Inserting '%s' into import Movie container (err=%d)\n", theFile->ofName, err );
			if( err == noErr ){
				// more stuff InsertMovieSegment() does NOT copy:
				SetMovieStartTime( theMovie, GetMoviePosterTime(theFile->theMovie), (theFile->numberMMElements == 1) );
				SetMoviePosterTime( theMovie, GetMoviePosterTime(theFile->theMovie) );
				CopyMovieSettings( theFile->theMovie, theMovie );
				CopyMovieUserData( theFile->theMovie, theMovie, kQTCopyUserDataReplace );
				CopyMovieMetaData( theMovie, theFile->theMovie );
				{ const char *bN = basename(theFile->ofName);
				  OSErr mdErr;
					if( (mdErr = MetaDataHandler( theMovie, NULL, akDisplayName, (char**) &bN, NULL, ", " )) != noErr ){
						Log( qtLogPtr, "Error %d setting new movie DisplayName to '%s'\n",
						    mdErr, bN
						);
					}
				}
				UpdateMovie(theMovie);
				MaybeShowMoviePoster(theMovie);
				UpdateMovie(theMovie);
			}
			DisposeMovie( theFile->theMovie );
		}
		*outFlags = movieImportResultComplete;
	}
	if( theMovie && err == noErr ){
		if( addedDuration ){
			*addedDuration = GetMovieDuration(theMovie);
		}
		if( theFile->numTracks > 1 ){
			*outFlags |= movieImportResultUsedMultipleTracks;
		}
		else if( usedTrack ){
			*usedTrack = GetMovieIndTrack( theMovie, 1 );
		}
		{ unsigned long hints = hintsPlayingEveryFrame/* hintsHighQuality*/;
			SetMoviePlayHints( theMovie, hints, 0xFFFFFFFF );
		}
	}

	if( *cwd ){
#ifdef __APPLE_CC__
		chdir( cwd );
#elif defined(_MSC_VER)
		_chdir( cwd );
#endif
	}
	if( theFile ){
		DisposeQI2MSourceFile(theFile);
	}

	if( err != noErr ){
#if TARGET_OS_MAC
	  const char *errComment;
	  const char *errString = MacErrorString(err, &errComment);
		Log( qtLogPtr, "QTImage2MovImport_DataRef() returns error %d: '%s' (%s)\n",
		    (int) err, errString, errComment
		);
#else
		Log( qtLogPtr, "QTImage2MovImport_DataRef() returns error %d\n", (int) err );
#endif
	}

	return err;
}

pascal ComponentResult QTImage2MovImport_GetLoadState(QTImage2MovGlobals store, long *state)
{
	*state = kMovieLoadStateComplete;
	Log( qtLogPtr, "QTImage2MovImport_GetLoadState called\n" );
	return noErr;
}

pascal ComponentResult QTImage2MovImport_SetProgressProc(QTImage2MovGlobals store, MovieProgressUPP proc, long refcon )
{
	Log( qtLogPtr, "QTImage2MovImport_SetProgressProc called\n" );
	if( store && store->theFile ){
		if( proc == NULL || proc == (MovieProgressUPP) -1 ){
			return paramErr;
		}
		else{
			store->theFile->progressProc.call = proc;
			store->theFile->progressProc.refcon = refcon;
			return noErr;
		}
	}
	else{
		return dataNotOpenForRead;
	}
}

// ---------------------------------------------------------------------------------
//		• QTImage2MovGetMIMETypeList •
//
// Returns a list of MIME types supported by the movie import component.
//
// ---------------------------------------------------------------------------------
pascal ComponentResult QTImage2MovImport_GetMIMETypeList(QTImage2MovGlobals store, QTAtomContainer *mimeInfo)
{
	ComponentResult err = noErr;

	if (mimeInfo == nil)
	{
		err = paramErr;
	}
	else
	{
		err	= GetComponentResource((Component)store->theMovieImportComponent,
							   kMimeInfoMimeTypeTag, kQTImage2MovThingRes, (Handle *)mimeInfo);
	}

    return err;
}

#pragma mark ---- ----
static ComponentResult Check4XMLError( QI2MSourceFile theFile, ComponentInstance xmlParser, ComponentResult err )
{
	if( err != noErr ){
	  ComponentResult err2;
	  long line;
	  char desc[257];
		desc[0] = 0;
		err2 = XMLParseGetDetailedParseError( xmlParser, &line, (StringPtr) desc );
		// desc will be a Pascal string, so null-terminate it at the proper location:
		desc[ desc[0]+1 ] = '\0';
		Log( qtLogPtr, "Error parsing XML file \"%s\" at line %ld: '%s' (error code %d)\n",
			P2Cstr( (void*) theFile->theFileSpec.name ), line, &desc[1], (int) err
		);
//#if TARGET_OS_WIN32
#	ifdef LOG_FRANCAIS
		PostYesNoDialog( "Erreur XML", P2Cstr( (void*) theFile->theFileSpec.name ) );
#	else
		PostYesNoDialog( "XML error", P2Cstr( (void*) theFile->theFileSpec.name ) );
#	endif
//#endif
	}
	return err;
}

char *element_errorStr( xmlElems elem )
{ static char *emsg = "";
#ifdef LOG_FRANCAIS
#	define EMSG "erreur dans élément "
#else
#	define EMSG "error in element "
#endif
	switch( elem ){
		case element_slideshow:
			emsg = EMSG "<slideshow/>";
			break;
		case element_import:
			emsg = EMSG "<import/>";
			break;
		case element_image:
			emsg = EMSG "<image/>";
			break;
		case element_sequence:
			emsg = EMSG "<sequence/>";
			break;
		case element_audio:
			emsg = EMSG "<audio/>";
			break;
		case element_chapter:
			emsg = EMSG "<chapter/>";
			break;
		case element_description:
			emsg = EMSG "<description/>";
			break;
		default:
			emsg = EMSG "??";
			break;
	}
	return emsg;
}

//!
//! ---------------------------------------------------------------------------------
//!		• ReadQI2MSourceFile •
//!
//! Parse the XML file and construct a list of all specified input objects with their attributes
//! ---------------------------------------------------------------------------------
//!
ComponentResult ReadQI2MSourceFile(QI2MSourceFile theFile)
{
	ComponentResult 	err 		= noErr;
	ComponentInstance	xmlParser 	= nil;
	XMLDoc				document 	= nil;
	long				index;
	xmlElems			elementType;
	double			buildStartTime;

	buildStartTime = HRTime_tic();

	// create a QI2MSource parser
	xmlParser = CreateXMLParserForQI2MSource();

	if( xmlParser == nil ){
		err = qtXMLApplicationErr;
		goto bail;
	}

	// do the parse; flags added by RJVB:
	err = XMLParseDataRef(xmlParser, theFile->dataRef, theFile->dataRefType,
			  xmlParseFlagAllowUppercase|xmlParseFlagAllowUnquotedAttributeValues|elementFlagPreserveWhiteSpace,
			  &document
	);
	// RJVB: check for errors instead of allowing Quicktime to crash:
	if( Check4XMLError( theFile, xmlParser, err ) ){
		goto bail;
	}

	if( (document->rootElement.identifier == element_slideshow
		|| document->rootElement.identifier == element_import)
		&& document->rootElement.contents
	){
		// get the slideshow element attributes
		err = GetQI2MSourceAttributes(document->rootElement.attributes, theFile);
		if( err != noErr ){
			Log( qtLogPtr, "Error %d getting XML root attributes\n", (int) err );
//#if TARGET_OS_WIN32
			PostYesNoDialog( element_errorStr(document->rootElement.identifier),
						 P2Cstr( (void*) theFile->theFileSpec.name )
			);
//#endif
			goto bail;
		}

		// scan through the contents looking for images
		for (index = 0; document->rootElement.contents[index].kind != xmlContentTypeInvalid; index++)
		{
			if (document->rootElement.contents[index].kind == xmlContentTypeElement)
			{
				elementType = document->rootElement.contents[index].actualContent.element.identifier;

				switch( elementType ){
					case element_image:{
						// parse the image
						err = GetAndSaveImage(&(document->rootElement.contents[index].actualContent.element), theFile);
						if( err == noErr ){
							theFile->numberMMElements += 1;
						}
						break;
					}
						// RJVB
					case element_sequence:{
						// parse the image
						err = GetAndSaveSequence(&(document->rootElement.contents[index].actualContent.element), theFile);
						if( err == noErr ){
							theFile->numberMMElements += 1;
						}
						break;
					}
					case element_audio:{
						err = GetAndSaveAudioElement(&(document->rootElement.contents[index].actualContent.element), theFile);
						if( err == noErr ){
							theFile->numberMMElements += 1;
						}
						break;
					}
					case element_chapter:{
						err = GetAndSaveChapterMark(&(document->rootElement.contents[index].actualContent.element), theFile);
						if( err == noErr ){
							theFile->numberMMElements += 1;
						}
						break;
					}
					case element_description:{
					  XMLAttributePtr attributes = document->rootElement.contents[index].actualContent.element.attributes;
					  QI2MSourceElement tmp = (QI2MSourceElement)NewPtrClear(sizeof(QI2MSourceElementRecord));
						if( (err = MemError()) ){
							goto bail;
						}
						tmp->elementKind = kQI2MSourceDescrKind;
						err = GetStringAttribute(attributes, attr_lang, &(tmp->language));
						if( err == attributeNotFound ){
							err = noErr;
						}
						if( !err ){
							err = GetStringAttribute(attributes, attr_txt, &(tmp->description));
						}
						if( !err ){
							// add the element to our list
							AddQI2MSourceElement(theFile, tmp);
						}
						break;
					}
					default:
						// noop;
						break;
				}
				if( err != noErr ){
					Log( qtLogPtr, "Error %d getting XML element #%ld attributes\n", (int) err, index+1 );
//#if TARGET_OS_WIN32
					PostYesNoDialog( element_errorStr(elementType),
								 P2Cstr( (void*) theFile->theFileSpec.name )
					);
//#endif
					goto bail;
				}
			}
		}
	}
	else{
		err = invalidMovie;
	}

bail:
	if( document ){
		XMLParseDisposeXMLDoc(xmlParser, document);
	}

	if( xmlParser ){
		CloseComponent(xmlParser);
	}

#ifdef TIMING
	Log( qtLogPtr, "ReadQI2MSourceFile() took %gs\n", HRTime_tic() - buildStartTime );
#endif
	return err;
}

//!
//! ---------------------------------------------------------------------------------
//!		• CreateXMLParserForQI2MSource •
//!
//! Locate the XML parser component
//! Add the specified elements to the parser so that it can read and parse our kind
//! of XML files - the QI2M specification.
//! ---------------------------------------------------------------------------------
//!
ComponentInstance CreateXMLParserForQI2MSource()
{
	ComponentInstance	xmlParser = OpenDefaultComponent(xmlParseComponentType, xmlParseComponentSubType);
	UInt32			elementID, attributeID;

	if (xmlParser)
	{
		XML_ADD_ELEMENT(slideshow);
			XML_ADD_ATTRIBUTE_AND_VALUE(width, attributeValueKindInteger, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(height, attributeValueKindInteger, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(asksave, attributeValueKindBoolean, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(autosave, attributeValueKindBoolean, nil);
			XML_ADD_ATTRIBUTE(autosavename);

		XML_ADD_ELEMENT(import);
			XML_ADD_ATTRIBUTE_AND_VALUE(width, attributeValueKindInteger, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(height, attributeValueKindInteger, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(asksave, attributeValueKindBoolean, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(autosave, attributeValueKindBoolean, nil);
			XML_ADD_ATTRIBUTE(autosavename);

		XML_ADD_ELEMENT(image);
			XML_ADD_ATTRIBUTE(src);
			XML_ADD_ATTRIBUTE_AND_VALUE(dur, attributeValueKindDouble, nil);
			// RJVB: support for millisecond durations. This made sense when durations
			// were integer values (in the original SlideShowImporter code), and
			// is maintained for "backward" compatibility.
			XML_ADD_ATTRIBUTE_AND_VALUE(mdur, attributeValueKindDouble, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(ismovie, attributeValueKindInteger, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(transh, attributeValueKindInteger, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(transv, attributeValueKindInteger, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(reltransh, attributeValueKindDouble, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(reltransv, attributeValueKindDouble, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(newchapter, attributeValueKindBoolean, nil);

		// RJVB: XML element to define an image sequence source:
		XML_ADD_ELEMENT(sequence);
			XML_ADD_ATTRIBUTE(src);
			XML_ADD_ATTRIBUTE_AND_VALUE(asmovie, attributeValueKindBoolean, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(freq, attributeValueKindDouble, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(interval, attributeValueKindInteger, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(maxframes, attributeValueKindInteger, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(starttime, attributeValueKindBoolean, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(timepad, attributeValueKindBoolean, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(hidetc, attributeValueKindBoolean, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(hidets, attributeValueKindBoolean, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(newchapter, attributeValueKindBoolean, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(channel, attributeValueKindInteger, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(hflip, attributeValueKindBoolean, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(vmgi, attributeValueKindBoolean, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(transh, attributeValueKindInteger, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(transv, attributeValueKindInteger, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(reltransh, attributeValueKindDouble, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(reltransv, attributeValueKindDouble, nil);
			XML_ADD_ATTRIBUTE(fcodec);
			XML_ADD_ATTRIBUTE(fbitrate);
			XML_ADD_ATTRIBUTE_AND_VALUE(fsplit, attributeValueKindBoolean, nil);
			XML_ADD_ATTRIBUTE(description);
			XML_ADD_ATTRIBUTE_AND_VALUE(log, attributeValueKindBoolean, nil);

		XML_ADD_ELEMENT(audio);
			XML_ADD_ATTRIBUTE(src);

		XML_ADD_ELEMENT(chapter);
			XML_ADD_ATTRIBUTE(title);
			XML_ADD_ATTRIBUTE_AND_VALUE(starttime, attributeValueKindDouble, nil);
			XML_ADD_ATTRIBUTE_AND_VALUE(duration, attributeValueKindDouble, nil);

		XML_ADD_ELEMENT(description);
			XML_ADD_ATTRIBUTE(txt);
			XML_ADD_ATTRIBUTE(lang);
	}

	return xmlParser;
}

//!
//! ---------------------------------------------------------------------------------
//!		• GetQI2MSourceAttributes •
//!
//! Get the optional attributes of the root element: movie width and height,
//! saving parameters.
//! ---------------------------------------------------------------------------------
//!
ComponentResult GetQI2MSourceAttributes(XMLAttribute *attributes, QI2MSourceFile theFile)
{
	ComponentResult err = noErr;

	// save root attribute values (both required)
	// RJVB: no, neither is required, as we know how to obtain width and height from the added content!
	err = GetIntegerAttribute(attributes, attr_width, &theFile->movieWidth);
	if( err == noErr || err == attributeNotFound ){
		err = GetIntegerAttribute(attributes, attr_height, &theFile->movieHeight);
	}

	if( err == noErr || err == attributeNotFound ){
	  ComponentResult e;
		theFile->autoSave = FALSE;
		err = GetBooleanAttribute(attributes, attr_autosave, &theFile->autoSave);
		e = GetStringAttribute( attributes, attr_autosavename, &theFile->autoSaveName );
	}
	if( err == noErr || err == attributeNotFound ){
		// autoSave defaults to False, so askSave will default to True, unless
		// the user activated autoSave. This complements the fact that the askSave function
		//  overrides the autoSave function.
		theFile->askSave = !theFile->autoSave;
		err = GetBooleanAttribute(attributes, attr_asksave, &theFile->askSave);
	}
	if( err == attributeNotFound ){
		err = noErr;
	}

    return err;
}

//!
//! ---------------------------------------------------------------------------------
//!		• AddQI2MSourceElement •
//!
//! Add a QI2MSourceElementRecord record to our queue of elements
//! ---------------------------------------------------------------------------------
//!
void AddQI2MSourceElement(QI2MSourceFile theFile, QI2MSourceElement theElement)
{
	if (theFile->elements == nil)
	{
		theFile->elements = theElement;
	}
	else
	{
		QI2MSourceElement temp = theFile->elements;

		while (temp && temp->nextElement)
			temp = (QI2MSourceElement)(temp->nextElement);

		temp->nextElement = theElement;
	}
}

ComponentResult GetAndSaveChapterMark( XMLElement *element, QI2MSourceFile theFile )
{ ComponentResult err = noErr;
  XMLAttributePtr	attributes = element->attributes;
  QI2MSourceElement theTempImage = nil;

	// allocate and initialize using NewPtrClear
	theTempImage = (QI2MSourceElement)NewPtrClear(sizeof(QI2MSourceElementRecord));
	err = MemError();
	if (err) goto bail;

	theTempImage->elementKind = kQI2MSourceChaptKind;

	set_NaN( theTempImage->chapterMark.startTime );
	err = GetDoubleAttribute(attributes, attr_starttime, &(theTempImage->chapterMark.startTime) );
	if( err && err != attributeNotFound ){
		goto bail;
	}
	set_NaN( theTempImage->chapterMark.duration );
	err = GetDoubleAttribute(attributes, attr_duration, &(theTempImage->chapterMark.duration) );
	if( err && err != attributeNotFound ){
		goto bail;
	}

	// now read the only obligatory tag:
	if( (err = GetStringAttribute(attributes, attr_title, &(theTempImage->chapterMark.title))) ){
		goto bail;
	}

	// add the element to our list
	AddQI2MSourceElement(theFile, theTempImage);

	theFile->numImages++;

bail:

	if( err && theTempImage ){
		DisposeAllElements(theFile);
	}

	return err;
}


//!
//! ---------------------------------------------------------------------------------
//!		• GetAndSaveImage •
//!
//!  Build a QI2MSourceElementRecord record for the current image and add it to our list
//! ---------------------------------------------------------------------------------
//!
ComponentResult GetAndSaveImage(XMLElement *element, QI2MSourceFile theFile)
{
	ComponentResult err = noErr;
	XMLAttributePtr	attributes = element->attributes;
	QI2MSourceElement theTempImage = nil;

	// allocate and initialize using NewPtrClear
	theTempImage = (QI2MSourceElement)NewPtrClear(sizeof(QI2MSourceElementRecord));
	err = MemError();
	if (err) goto bail;

	theTempImage->elementKind = kQI2MSourceImageKind;

	// save image values
	// RJVB: an image can actually be a movie if the files tells us to treat it as such:
	theTempImage->image.isMovie = 0;
	{ long isMovie = FALSE;
		err = GetIntegerAttribute(attributes, attr_ismovie, &isMovie );
		if( err && err != attributeNotFound ){
			goto bail;
		}
		theTempImage->image.isMovie = isMovie;
	}

	theTempImage->image.transH = 0;
	err = GetIntegerAttribute(attributes, attr_transh, &(theTempImage->image.transH) );
	if( err && err != attributeNotFound ){
		goto bail;
	}
	theTempImage->image.transV = 0;
	err = GetIntegerAttribute(attributes, attr_transv, &(theTempImage->image.transV) );
	if( err && err != attributeNotFound ){
		goto bail;
	}
	theTempImage->image.relTransH = 0;
	err = GetDoubleAttribute(attributes, attr_reltransh, &(theTempImage->image.relTransH) );
	if( err && err != attributeNotFound ){
		goto bail;
	}
	theTempImage->image.relTransV = 0;
	err = GetDoubleAttribute(attributes, attr_reltransv, &(theTempImage->image.relTransV) );
	if( err && err != attributeNotFound ){
		goto bail;
	}
	err = noErr;

	// movies do not need a duration specifier:
	if( !theTempImage->image.isMovie ){
		err = GetDoubleAttribute(attributes, attr_mdur, &(theTempImage->image.mduration));
		if( err == attributeNotFound ){
		  // RJVB: millisecond specifications override second specifications, but if absent, go for seconds.
			theTempImage->image.mduration= 0;
			err = GetDoubleAttribute(attributes, attr_dur, &(theTempImage->image.duration));
		}
	}
	if (err) goto bail;

	err = GetPStringAttribute(attributes, attr_src, &(theTempImage->image.src));
	if (err) goto bail;

	theTempImage->image.newChapter = FALSE;
	{ int newChapter = theTempImage->image.newChapter;
		err = GetBooleanAttribute(attributes, attr_newchapter, &newChapter );
		if( err && err != attributeNotFound ){
			goto bail;
		}
		theTempImage->image.newChapter = newChapter;
	}

	// add the element to our list
	AddQI2MSourceElement(theFile, theTempImage);

	theFile->numImages++;
	err = noErr;

bail:

	if (err && theTempImage)
	{
		DisposeAllElements(theFile);
	}

	return err;
}

//!
//! ---------------------------------------------------------------------------------
//!		• GetAndSaveSequence •
//!
//!  Build a QI2MSourceElementRecord record for the current image sequence and add it to our list
//! Added by RJVB. An image sequence is just a special case of an image, which does not
//! require a duration specification, but will have other parameters.
//! ---------------------------------------------------------------------------------
//!
ComponentResult GetAndSaveSequence(XMLElement *element, QI2MSourceFile theFile)
{ ComponentResult err = noErr;
  XMLAttributePtr	attributes = element->attributes;
  QI2MSourceElement theTempImage = nil;

	// allocate and initialize using NewPtrClear
	theTempImage = (QI2MSourceElement)NewPtrClear(sizeof(QI2MSourceElementRecord));
	err = MemError();
	if (err) goto bail;

	theTempImage->elementKind = kQI2MSourceImageKind;

	theTempImage->image.isMovie = 0;
	{ int asMovie = FALSE;
		err = GetBooleanAttribute(attributes, attr_asmovie, &asMovie );
		if( err && err != attributeNotFound ){
			goto bail;
		}
		theTempImage->image.isMovie = theTempImage->image.asMovie = asMovie;
	}
//	theTempImage->image.isSequence = !theTempImage->image.isMovie;
	theTempImage->image.isSequence = 1;

	theTempImage->image.frequency = -1;
	err = GetDoubleAttribute(attributes, attr_freq, &(theTempImage->image.frequency) );
	if( err && err != attributeNotFound ){
		goto bail;
	}

	theTempImage->image.interval = 1;
	err = GetShortAttribute(attributes, attr_interval, &(theTempImage->image.interval) );
	if( err && err != attributeNotFound ){
		goto bail;
	}

	theTempImage->image.maxFrames = -1;
	err = GetIntegerAttribute(attributes, attr_maxframes, &(theTempImage->image.maxFrames) );
	if( err && err != attributeNotFound ){
		goto bail;
	}

	theTempImage->image.ignoreRecordStartTime = FALSE;
	{ int starttime = !theTempImage->image.ignoreRecordStartTime;
		err = GetBooleanAttribute(attributes, attr_starttime, &starttime );
		if( err && err != attributeNotFound ){
			goto bail;
		}
		theTempImage->image.ignoreRecordStartTime = !starttime;
	}
	theTempImage->image.timePadding = FALSE;
	{ int timePadding = theTempImage->image.timePadding;
		err = GetBooleanAttribute(attributes, attr_timepad, &timePadding );
		if( err && err != attributeNotFound ){
			goto bail;
		}
		theTempImage->image.timePadding = timePadding;
	}
	theTempImage->image.hideTC = FALSE;
	{ int hideTC = theTempImage->image.hideTC;
		err = GetBooleanAttribute(attributes, attr_hidetc, &hideTC );
		if( err && err != attributeNotFound ){
			goto bail;
		}
		theTempImage->image.hideTC = hideTC;
	}
	theTempImage->image.hideTS = FALSE;
	{ int hideTS = theTempImage->image.hideTS;
		err = GetBooleanAttribute(attributes, attr_hidets, &hideTS );
		if( err && err != attributeNotFound ){
			goto bail;
		}
		theTempImage->image.hideTS = hideTS;
	}

	theTempImage->image.useVMGI = TRUE;
	{ int useVMGI = theTempImage->image.useVMGI;
		err = GetBooleanAttribute(attributes, attr_vmgi, &useVMGI );
		if( err && err != attributeNotFound ){
			goto bail;
		}
		theTempImage->image.useVMGI = useVMGI;
	}

	theTempImage->image.channel = -1;
	err = GetShortAttribute(attributes, attr_channel, &(theTempImage->image.channel) );
	if( err && err != attributeNotFound ){
		goto bail;
	}

	theTempImage->image.hflip = 0;
	{ int flip = 0;
		err = GetBooleanAttribute(attributes, attr_hflip, &flip );
		if( err && err != attributeNotFound ){
			goto bail;
		}
		else{
			theTempImage->image.hflip = flip;
		}
	}

	theTempImage->image.transH = 0;
	err = GetIntegerAttribute(attributes, attr_transh, &(theTempImage->image.transH) );
	if( err && err != attributeNotFound ){
		goto bail;
	}
	theTempImage->image.transV = 0;
	err = GetIntegerAttribute(attributes, attr_transv, &(theTempImage->image.transV) );
	if( err && err != attributeNotFound ){
		goto bail;
	}
	theTempImage->image.relTransH = 0;
	err = GetDoubleAttribute(attributes, attr_reltransh, &(theTempImage->image.relTransH) );
	if( err && err != attributeNotFound ){
		goto bail;
	}
	theTempImage->image.relTransV = 0;
	err = GetDoubleAttribute(attributes, attr_reltransv, &(theTempImage->image.relTransV) );
	if( err && err != attributeNotFound ){
		goto bail;
	}

	theTempImage->image.newChapter = FALSE;
	{ int newChapter = theTempImage->image.newChapter;
		err = GetBooleanAttribute(attributes, attr_newchapter, &newChapter );
		if( err && err != attributeNotFound ){
			goto bail;
		}
		theTempImage->image.newChapter = newChapter;
	}

	// the following three  attributes are relevant only when importing MPG4 VOD files
	err = GetStringAttribute(attributes, attr_fcodec, &(theTempImage->image.ffmpCodec));
	if( err && err != attributeNotFound ){
		goto bail;
	}
	if( err != attributeNotFound ){
		err = GetStringAttribute(attributes, attr_fbitrate, &(theTempImage->image.ffmpBitRate));
		if( err && err != attributeNotFound ){
			goto bail;
		}
	}
	{ int fsplit = 0;
		err = GetBooleanAttribute(attributes, attr_fsplit, &fsplit );
		if( err && err != attributeNotFound ){
			goto bail;
		}
		theTempImage->image.ffmpSplit = fsplit;
	}
	if( theTempImage->image.ffmpSplit ){
		if( !theTempImage->image.ffmpCodec ){
			if( (theTempImage->image.ffmpCodec = (StringPtr)NewPtr(6)) ){
				strcpy( (char*) theTempImage->image.ffmpCodec, "mjpeg" );
			}
		}
		if( !theTempImage->image.ffmpBitRate ){
			if( (theTempImage->image.ffmpBitRate = (StringPtr)NewPtr(5)) ){
				strcpy( (char*) theTempImage->image.ffmpBitRate, "500k" );
			}
		}
		else{
		  double rate;
		  char *frate = (char*) theTempImage->image.ffmpBitRate, *c, suffix = '\0', buf[64];
			// the delicate part: divide the requested bitrate by 4
			c = frate;
			while( *c && (isdigit(*c) || *c == '.') ){
				c++;
			}
			if( *c ){
				suffix = *c;
				*c = '\0';
			}
			if( sscanf( frate, "%lf", &rate ) > 0 ){
				rate /= 4;
				snprintf( buf, sizeof(buf), "%g%c", rate, suffix );
				if( (theTempImage->image.ffmpBitRate = (StringPtr)NewPtr(strlen(buf) + 1)) ){
					strcpy( (char*) theTempImage->image.ffmpBitRate, buf );
					Log( qtLogPtr, "GetAndSaveSequence(): requested bitrate '%s%c' becomes '%s' in fsplit mode",
					    frate, suffix, buf );
					DisposePtr(frate);
				}
			}
		}
	}

	err = GetStringAttribute(attributes, attr_description, &(theTempImage->image.description));
	if( err && err != attributeNotFound ){
		goto bail;
	}

	{ int log = 0;
		err = GetBooleanAttribute(attributes, attr_log, &log );
		if( err && err != attributeNotFound ){
			goto bail;
		}
		theTempImage->image.log = log;
	}

	err = GetPStringAttribute(attributes, attr_src, &(theTempImage->image.src));
	if (err) goto bail;

	// add the element to our list
	AddQI2MSourceElement(theFile, theTempImage);

	theFile->numImages++;

bail:

	if (err && theTempImage)
	{
		DisposeAllElements(theFile);
	}

	return err;
}


// ---------------------------------------------------------------------------------
//		• GetAndSaveAudioElement •
//
//  Build a QI2MSourceElementRecord record for the current audio element and add it to
//  our list
// ---------------------------------------------------------------------------------
ComponentResult	GetAndSaveAudioElement(XMLElement *element, QI2MSourceFile theFile)
{
	ComponentResult 	err 			= noErr;
	XMLAttributePtr		attributes 		= element->attributes;
	QI2MSourceElement	theTempSound 	= nil;

	// allocate and initialize using NewPtrClear
	theTempSound = (QI2MSourceElement)NewPtrClear(sizeof(QI2MSourceElementRecord));
	err = MemError();
	if (err) goto bail;

	// save image values
	theTempSound->elementKind = kQI2MSourceAudioKind;

	err = GetPStringAttribute(attributes, attr_src, &(theTempSound->movie.src));
	if (err) goto bail;

	// add the element to our list
	AddQI2MSourceElement(theFile, theTempSound);

	theFile->numAudioTracks++;

bail:
	if (err && theTempSound)
	{
		DisposeAllElements(theFile);
	}

	return err;
}

//!
//! RJVB 20100623: from http://www.cocoadev.com/index.pl?FSMakeFSSpec
//
OSStatus PathToFSSpec( char *path, FSSpec *outSpec)
{ OSStatus err = -1;
#if !defined(_WINDOWS) && !defined(WIN32) && !defined(_MSC_VER)
  FSRef ref;
  Boolean isDirectory;
  char *c = NULL;

	if( outSpec ){
		// First, try to create an FSRef for the full path
		err = FSPathMakeRef( (UInt8 *) path, &ref, &isDirectory );

		if( err == noErr ){
		  // It's a directory or a file that exists; convert directly into an FSSpec:
			err = FSGetCatalogInfo( &ref, kFSCatInfoNone, NULL, NULL, outSpec, NULL );
		}
#ifdef DEBUG
		else{
			Log( qtLogPtr, "FSPathMakeRef(\"%s\") failed: %d\n", path, (int) err );
		}
#endif
	}
	else{
		err = paramErr;
	}
	if( c ){
		free(c);
	}
#else
	if( outSpec ){
		err = NativePathNameToFSSpec( path, outSpec, 0L );
#ifdef DEBUG
		if( err != noErr ){
			Log( qtLogPtr, "NativePathNameToFSSpec(\"%s\") failed: %d\n", path, (int) err );
		}
#endif
	}
	else{
		err = paramErr;
	}
#endif
	return( err );
}

//!
//! From TN1195:
////
//!
//! AddMacOSFileTypeDataRefExtension
//! Add a Macintosh file type as a data reference extension.
//!
//! A Macintosh file type data extension is an atom whose
//! data is a 4-byte OSType.
//!
OSErr AddMacOSFileTypeDataRefExtension( Handle theDataRef, OSType theType )
{ unsigned long myAtomHeader[2];
  OSType myType;
  OSErr myErr = noErr;
	myAtomHeader[0] = EndianU32_NtoB( sizeof(myAtomHeader) + sizeof(theType) );
	myAtomHeader[1] = EndianU32_NtoB( kDataRefExtensionMacOSFileType );
	myType = EndianU32_NtoB(theType);
	myErr = PtrAndHand( myAtomHeader, theDataRef, sizeof(myAtomHeader) );
	if( myErr == noErr ){
		myErr = PtrAndHand( &myType, theDataRef, sizeof(myType) );
	}
	return(myErr);
}

//!
//! RJVB simple function to guess a file type from a file's extension.
//! This is a brain-dead approach, but much simpler and quicker than determining
//! the type from the file contents.
//! The type is returned as a Macintosh OSType, which is a 4-byte long that can be
//! represented as a four-letter code which will be appropriate on big-endian systems.
//
int DetermineOSType( Str255 theFilePString, char *theURL, OSType *type )
{ int ok = 1;
  static OSType ptype = -1;
  int len;
  char *fname;
	if( !type ){
		 // this will avoid debug output in all cases as a side-effect.
		type = &ptype;
	}
	if( theFilePString ){
		len = theFilePString[0];
		fname = (char*) &theFilePString[1];
	}
	else{
		if( (fname = theURL) ){
			len = strlen(theURL);
		}
	}
	if( fname ){
		if( strncasecmp( &fname[len-4], ".png", 4 ) == 0 ){
			*type = kQTFileTypePNG;
		}
		else if( strncasecmp( &fname[len-4], ".jpg", 4 ) == 0
			    || strncasecmp( &fname[len-5], ".jpeg", 5) == 0
		){
			*type = kQTFileTypeJPEG;
		}
		else if( strncasecmp( &fname[len-4], ".tif", 4 ) == 0
			    || strncasecmp( &fname[len-5], ".tiff", 5) == 0
		){
			*type = kQTFileTypeTIFF;
		}
		else if( strncasecmp( &fname[len-4], ".mov", 4 ) == 0
			    || strncasecmp( &fname[len-4], ".mp4", 4) == 0
			    || strncasecmp( &fname[len-4], ".wmv", 4) == 0
			    || strncasecmp( &fname[len-4], ".avi", 4) == 0
			    || strncasecmp( &fname[len-4], ".asf", 4) == 0
			    || strncasecmp( &fname[len-4], ".mpg", 4) == 0
			    || strncasecmp( &fname[len-5], ".mpeg", 5) == 0
		){
			*type = kQTFileTypeMovie;
		}
		else if( strncasecmp( &fname[len-4], ".wav", 4 ) == 0 ){
			*type = kQTFileTypeWave;
		}
		else if( strncasecmp( &fname[len-5], ".jpgs", 5 ) == 0 ){
			*type = 'jpgs';
		}
		else if( strncasecmp( &fname[len-5], ".qi2m", 5 ) == 0 ){
			*type = 'QI2M';
		}
		else if( strncasecmp( &fname[len-5], ".qtsl", 5 ) == 0 ){
			*type = 'QTSL';
		}
		else if( strncasecmp( &fname[len-4], ".vod", 4 ) == 0 ){
			*type = 'VOBS';
		}
		else{
			*type = 0;
			ok = 0;
		}
		if( *type != ptype ){
			ptype = *type;
		}
	}
	else{
		ptype = *type = 0;
		ok = 0;
	}
	return( ok );
}

//! From TN1195:
////
//! AddFilenamingExtension
//! Add a filenaming extension to a data reference.
//! If theStringPtr is NULL, add a 0-length filename.
//!
//! A filenaming extension is a Pascal string.
//! DataType guessing added by RJVB
//!
OSErr AddFilenamingExtension( Handle theDataRef, Str255 theFileName, char *name, int addDataRefType )
{ unsigned char myChar = 0;
  OSErr myErr = noErr;
  OSType type;
	if( theFileName == NULL || theFileName[0] == 0 ){
		if( name ){
		  Str255 fname;
		  int len = strlen(name);
			if( len < 255 ){
				fname[0] = len;
				strcpy( (char*) &fname[1], name );
			}
			else{
				fname[0] = 255;
				strncpy( (char*) &fname[1], name, 255 );
			}
			myErr = PtrAndHand( fname, theDataRef, fname[0] + 1);
#ifdef DEBUG
			Log( qtLogPtr, "AddFileNamingExtension( name=\"%s\"[%s] )\n", P2Cstr(fname), name );
#endif
			if( addDataRefType && DetermineOSType( fname, NULL, &type ) ){
			  OSErr err2;
				if( (err2 = AddMacOSFileTypeDataRefExtension( theDataRef, type )) ){
					Log( qtLogPtr, "Error adding type '%.4s' to %s (%d)\n",
						OSTStr(type), P2Cstr(fname), (int) err2 );
				}
			}
		}
		else{
			myErr = PtrAndHand(&myChar, theDataRef, sizeof(myChar));
#ifdef DEBUG
			Log( qtLogPtr, "AddFileNamingExtension( theFileName=NULL, name=NULL )\n" );
#endif
		}
	}
	else{
		myErr = PtrAndHand(theFileName, theDataRef, theFileName[0] + 1);
#ifdef DEBUG
		Log( qtLogPtr, "AddFileNamingExtension( theFileName=\"%s\" )\n", P2Cstr(theFileName) );
#endif
		if( addDataRefType && DetermineOSType( theFileName, NULL, &type ) ){
		  OSErr err2;
			if( (err2 = AddMacOSFileTypeDataRefExtension( theDataRef, type )) ){
				Log( qtLogPtr, "Error adding type '%.4s' to %s (%d)\n",
					OSTStr(type), P2Cstr(theFileName), (int) err2 );
			}
		}
	}
	return(myErr);
}

//!
//! RJVB obtain a dataRef from an URL - which in this case will be a filename or absolute path.
//! Uses QuickTime's QTNewDataReferenceFromFSSpec() if an FSSPec can be obtained from the URL, otherwise
//! the more generic but slower CreateDataRefFromURL() is called (see below).
//!
#ifndef _MSC_VER
inline
#endif
OSErr URLtoDataRef( char *theURL, QI2MSourceFile theFile, Handle *dataRef, OSType *dataRefType )
{ OSErr err;
  FSSpec URLSpec;
	err = PathToFSSpec( theURL, &URLSpec );
	if( err == noErr ){
		err = QTNewDataReferenceFromFSSpec( &URLSpec, 0, dataRef, dataRefType );
		AddFilenamingExtension( *dataRef, NULL, theURL, 1 );
	}
	else{
		URLSpec.name[0] = 0;
	}
	if( err != noErr ){
		err = CreateDataRefFromURL(theURL, theFile->dataRef, theFile->dataRefType, dataRef, dataRefType);
	}
	if( err == noErr ){
	  char path[1024];
		GetFilenameFromDataRef( *dataRef, *dataRefType, path, sizeof(path)/sizeof(char), NULL );
	}
	return err;
}

// RJVB: define TIMING to determine various execution times. On MS Windows, _SS_LOG_ACTIVE must be defined
// and the SS_Log facility installed.
#undef TIMING

#ifdef TIMING
	double totTime;
#endif

#pragma mark ---- ----

//!
//! ---------------------------------------------------------------------------------
//!		• BuildMovie •
//!
//!	Steps
//!	- Add all the images to a single video track
//!
//! - add audio element
//! ---------------------------------------------------------------------------------
//!
ComponentResult BuildMovie(QI2MSourceFile theFile)
{
	ComponentResult		err 		= noErr;
	QI2MSourceElement 	theElement 	= nil;
	TimeValue 			theSampleTime 	= 0;
	Fixed 				maxWidth = 0, displayWidth = 0;
	Fixed 				maxHeight = 0, displayHeight = 0;
	short				minLayer = 0;
	double				buildStartTime;
#if TARGET_OS_WIN32
	int progState = 0, progStates[] = {1, 2, 3, 2};
	// 20101118: On MSWindows, QuickTime somehow makes a 2nd attempt if an import returns an error.
	// it isn't exactly nice to ask the user twice if he wants to cancel the import if he says yes the 1st
	// time, so we'll have to remember the last file and error type that occurred.
	// Of course this means that if user tries to import the same file another time, without correction, without
	// quitting QuickTime or importing another file first, the import will be aborted when we encounter the error,
	// without further querying.
	static char *lastErrorFile = NULL;
	static OSErr lastError = noErr;
#endif

#ifdef DEBUG
	if( theFile->theFileSpec.name[0] ){
		Log( qtLogPtr, STRING(MOVIEIMPORT_BASENAME())"BuildMovie(\"%s\")\n", P2Cstr( (void*) theFile->theFileSpec.name ) );
	}
#endif

#ifdef TIMING
	totTime = 0;
#endif
	theFile->progressDialog = CreateProgressDialog( (const char*) P2Cstr(theFile->theFileSpec.name) );
	if( theFile->progressDialog ){
		SetProgressDialogMaxValDiv100( theFile->progressDialog, theFile->numberMMElements );
	}
	else{
		Log( qtLogPtr, "BuildMovie(): failure creating progress dialog!" );
	}
	theFile->currentMMElement = 0;

	DetermineOSType( NULL, NULL, NULL );
	buildStartTime = HRTime_tic();

		theFile->Duration = 0;
		theFile->prevDuration = 0;
		// add the images to a single track -- theFile->theImageTrack
		theFile->numTracks = 0;
//		InvokeMovieProgressUPP( theFile->theMovie, movieProgressOpen, progressOpImportMovie, 0, theFile->progressProc.refcon, theFile->progressProc.call );
		for (theElement = theFile->elements; theElement; theElement = (QI2MSourceElement)(theElement->nextElement))
		{

			switch( (long) theElement->elementKind )
			{
				case kQI2MSourceImageKind:
					// add the image to the movie
					SetProgressDialogValue( theFile->progressDialog, (double) theFile->currentMMElement, TRUE );
					err = SetProgressDialogStopRequested( theFile->progressDialog );
					if( err == noErr ){
						err = AddImagesToMovie(theFile, &theElement->image, &theFile->Duration, &theSampleTime, 1);
					}
//					theFile->thePreviousSource = &(theElement->image);
					theFile->currentMMElement += 1;

					if( err ){
//#if TARGET_OS_WIN32
					  char msg[1024];
#	ifdef LOG_FRANCAIS
						snprintf( msg, sizeof(msg), "Continuer après l'erreur (%d) qui s'est produite?", err );
#	else
						snprintf( msg, sizeof(msg), "Continue after the error (%d) that occurred?", err );
#	endif
						if( theFile->numImages == 1
#	if TARGET_OS_WIN32
						   || (lastErrorFile && strcmp(lastErrorFile, theElement->image.theURL)==0 && lastError == err)
#	endif
						   || !PostYesNoDialog( msg, theElement->image.theURL )
						){
							goto bail;
						}
						else{
							err = noErr;
						}
//#else
//						if( theFile->numImages == 1 ){
//							goto bail;
//						}
//						else{
//							err = noErr;
//						}
//#endif
					}

					if ( FixRound(theElement->image.discoveredWidth) > FixRound(maxWidth) ){
						maxWidth = theElement->image.discoveredWidth;
					}
					if ( FixRound(theElement->image.discoveredHeight) > FixRound(maxHeight) ){
						maxHeight = theElement->image.discoveredHeight;
					}
					if( FixRound(theElement->image.displayWidth) > FixRound(displayWidth) ){
						displayWidth = theElement->image.displayWidth;
					}
					if( FixRound(theElement->image.displayHeight) > FixRound(displayHeight) ){
						displayHeight = theElement->image.displayHeight;
					}
					break;

				case kQI2MSourceDescrKind:
					AddMetaDataStringToMovie( theFile, akDescr, (const char*) theElement->description, (const char*) theElement->language );
					break;

				case kQI2MSourceChaptKind:
					AddChapterTitle( theFile, theFile->theTCTrack,
						 (const char*) theElement->chapterMark.title,
						 (NaN(theElement->chapterMark.startTime))? theFile->Duration
							: (TimeValue) (theElement->chapterMark.startTime * theFile->timeScale + 0.5),
						 1
					);
					break;

				case kQI2MSourceAudioKind:
					break;
				default:
					break;
			}
		}

//		if ((theFile->movieWidth == 0) || (theFile->movieHeight == 0))
		{ Fixed 	oldWidth, oldHeight;
		  Track 	nextVideoTrack 	= nil;
		  long 	trackIndex 		= 1;

			  // 20101114: this is code slightly adapted from SlideShowImporter: I'm not sure
			  // what exactly the purpose is...
			if( theFile->theImageTrack ){
				GetTrackDimensions(theFile->theImageTrack, &oldWidth, &oldHeight);
				SetTrackDimensions(theFile->theImageTrack, (0 == theFile->movieWidth) ? maxWidth : oldWidth , (0 == theFile->movieHeight) ? maxHeight : oldHeight);
				minLayer = GetTrackLayer( theFile->theImageTrack );
			}
			else{
				oldWidth = maxWidth;
				oldHeight = maxHeight;
			}

			// spin over other video tracks and set their dimensions, too

			while( nil != (nextVideoTrack = GetMovieIndTrackType(theFile->theMovie, trackIndex, VideoMediaType, movieTrackMediaType)) ){
				if( nextVideoTrack != theFile->theImageTrack && theFile->theImageTrack ){
				  short l = GetTrackLayer(nextVideoTrack);
					GetTrackDimensions(theFile->theImageTrack, &oldWidth, &oldHeight);
#if 0 // translating the track should probably not be done!!
					{ MatrixRecord m;
					  Fixed tw, th;
						GetTrackMatrix( nextVideoTrack, &m );
						GetTrackDimensions( nextVideoTrack, &tw, &th );
#ifdef DEBUG
						Log( qtLogPtr, "[[%6d %6d %6d]\n", FixRound(m.matrix[0][0]), FixRound(m.matrix[0][1]), FixRound(m.matrix[0][2]) );
						Log( qtLogPtr, " [%6d %6d %6d]\n", FixRound(m.matrix[1][0]), FixRound(m.matrix[1][1]), FixRound(m.matrix[1][2]) );
						Log( qtLogPtr, " [%6d %6d %6d]] track %ld type %d %dx%d -> %dx%d\n",
							    FixRound(m.matrix[2][0]), FixRound(m.matrix[2][1]), FixRound(m.matrix[2][2]),
							    trackIndex, GetMatrixType(&m),
							    FixRound(tw), FixRound(th),
							    FixRound(oldWidth), FixRound(oldHeight)
						);
#endif
						TranslateMatrix( &m, Long2Fix(0), Long2Fix(0) );
						SetTrackMatrix( nextVideoTrack, &m );
					}
#endif // 0
					SetTrackDimensions(nextVideoTrack, oldWidth, oldHeight);
					if( l < minLayer ){
						minLayer = l;
					}
				}
				trackIndex += 1;
			}
		}

		// Now look for audio elements
		for (theElement = theFile->elements; theElement; theElement = (QI2MSourceElement)(theElement->nextElement))
		{
			switch( (long) theElement->elementKind )
			{
				case kQI2MSourceAudioKind:
				{
					TimeValue addedTime = 0;

					SetProgressDialogValue( theFile->progressDialog, (double) theFile->currentMMElement, TRUE );
					err = SetProgressDialogStopRequested( theFile->progressDialog );
					if( !err ){
						err = AddAudioToMovie(theFile, &theElement->movie, &addedTime );
						theFile->currentMMElement += 1;
					}
					goto finish;
					break;
				}
				default:
					break;
			}
		}

finish:
#if 0
// 20110119: don't move all TimeCode tracks to the bottom ... it could well be that there's a Text track there nowadays.
// If ever this (re)introduces anomalous behaviour, we'll have to check and account for Text track presence...
		{ long trackIndex = 1;
		  Track tcTrack;
		  Fixed oldWidth, oldHeight;
			while( (tcTrack = GetMovieIndTrackType( theFile->theMovie, trackIndex, TimeCodeMediaType, movieTrackMediaType)) ){
				GetTrackDimensions( tcTrack, &oldWidth, &oldHeight);
				if( oldWidth != Long2Fix(0) ){
					SetTrackDimensions( tcTrack, displayWidth, oldHeight );
					{ MatrixRecord theTrackMatrix;
						SetTrackMatrix( tcTrack, nil );
						GetTrackMatrix( tcTrack, &theTrackMatrix );
						TranslateMatrix( &theTrackMatrix, 0, displayHeight );
						SetTrackMatrix( tcTrack, &theTrackMatrix );
					}
					SetTrackLayer( tcTrack, minLayer - 1 );
				}
				trackIndex += 1;
			}
		}
#endif

		// remove the chapter track if it remained empty
		if( theFile->theChapTrack && GetTrackDuration(theFile->theChapTrack) == 0 ){
			DisposeMovieTrack( theFile->theChapTrack );
			theFile->theChapTrack = nil;
		}

		AnchorMovie2TopLeft( theFile->theMovie );
		{ time_t now = time(NULL);
			AddMetaDataStringToMovie( theFile, akCreationDate, ctime(&now), NULL );
		}
		SetMovieStartTime( theFile->theMovie, GetMoviePosterTime(theFile->theMovie), (theFile->numberMMElements == 1) );
		MaybeShowMoviePoster( theFile->theMovie );
		{ double fps, mFps, tcFps;
			GetMovieStaticFrameRate( theFile->theMovie, &fps, &tcFps, &mFps );
			Log( qtLogPtr, "BuildMovie(): resulting movie is %gHz (media: %g, TimeCode: %g)\n", fps, mFps, tcFps );
		}
		{ unsigned long hints = /*hintsPlayingEveryFrame*/ hintsHighQuality;
			SetMoviePlayHints( theFile->theMovie, hints, 0xFFFFFFFF );
		}

#ifdef TIMING
		Log( qtLogPtr, STRING(MOVIEIMPORT_BASENAME())"BuildMovie(%s) took %gs (%g cumulative times)\n",
			P2Cstr(theFile->theFileSpec.name), HRTime_tic() - buildStartTime, totTime
		);
#else
		Log( qtLogPtr, STRING(MOVIEIMPORT_BASENAME())"BuildMovie(%s) took %gs\n",
			P2Cstr(theFile->theFileSpec.name), HRTime_tic() - buildStartTime
		);
#endif
		if( theFile->Duration ){
		  OSErr err2;
			if( theFile->askSave ){
			  Handle odataRef;
			  OSType odataRefType;
			  long flags = createMovieFileDeleteCurFile|showUserSettingsDialog;
			  char oname[1024] = "";
				if( theFile->theFileSpec.name[0] && theFile->dataRefType == AliasDataHandlerSubType ){
//					URLtoDataRef( char *theURL, QI2MSourceFile theFile, Handle *dataRef, OSType *dataRefType )
					odataRef = theFile->dataRef;
					odataRefType = theFile->dataRefType;
					flags |= movieFileSpecValid;
#ifdef DEBUG
					Log( qtLogPtr, "askSave: using dataRef corresponding to file name spec\n" );
#endif
				}
				else{
					odataRef = nil;
					odataRefType = AliasDataHandlerSubType;
#ifdef DEBUG
					Log( qtLogPtr, "askSave: using nil dataRef\n" );
#endif
				}
				HRTime_tic();
				SetMovieProgressProc( theFile->theMovie, (MovieProgressUPP)-1L, 0);
				// 20111124: we don't try to exclude the target file from being backed up here
				// for the simple reason we don't know the name/file the user will choose!
				err2 = ConvertMovieToDataRef( theFile->theMovie,     /* identifies movie */
					nil,                /* all tracks */
					odataRef, odataRefType,
					MovieFileType, 'TVOD',
					createMovieFileDeleteCurFile
					| showUserSettingsDialog
					,
					0
				);
				GetFilenameFromDataRef( odataRef, odataRefType, oname, sizeof(oname)/sizeof(char), NULL );
				Log( qtLogPtr, "ConvertMovieToDataRef(\"%s\"->\"%s\") (askSave) took %gs, returned %d\n",
					theFile->path, oname, HRTime_toc(), (int) err2
				);
			}
			else{
#ifdef IMPORT_INTO_DESTREFMOV
				if( theFile->odataHandler ){
					UpdateMovie(theFile->theMovie);
					MaybeShowMoviePoster(theFile->theMovie);
					UpdateMovie(theFile->theMovie);
					err2 = AddMovieToStorage( theFile->theMovie, theFile->odataHandler );
					if( err2 == noErr ){
						err2 = CloseMovieStorage(theFile->odataHandler);
					}
					else{
						CloseMovieStorage(theFile->odataHandler);
					}
#if TARGET_OS_MAC
					if( theFile->cbState.urlRef ){
						CSBackupSetItemExcluded( theFile->cbState.urlRef,
										    theFile->cbState.bakExcl, theFile->cbState.bakExclPath );
						CFRelease(theFile->cbState.urlRef);
					}
#endif
					theFile->odataHandler = NULL;
					Log( qtLogPtr, "Finalising destination ref movie '%s' %sreturned %d\n",
						theFile->ofName, (theFile->autoSave)? "(autoSave) " : "", (int) err2
					);
					// 20121110:
					// DisposeMovie(theFile->theMovie) isn't necessary because we've already done CloseMovieStorage ???
					DisposeHandle(theFile->odataRef);
					// 20121110: now we need to get a new/refreshed handle on the destination ref movie:
					theFile->theMovie = NULL;
					err2 = NewMovieFromURL( &theFile->theMovie, newMovieActive, NULL, theFile->ofName,
										  &theFile->odataRef, &theFile->odataRefType );
					if( err2 != noErr ){
						Log( qtLogPtr, "Error reopening destination ref movie '%s' (%d)\n",
							theFile->ofName, (int) err2
						);
						theFile->theMovie = NULL;
						err = err2;
					}
					// we no longer need the dataRef and keeping it would cause confusion, so dispose of it.
					DisposeHandle(theFile->odataRef);
					theFile->odataRef = NULL;
				}
				else
#endif
				if( theFile->autoSave ){
				  char *fname = NULL;
#	ifdef AUTOSAVE_REPLACES
				  Movie refMovie = nil;
#	endif
					HRTime_tic();
					if( theFile->autoSaveName ){
						fname = strdup(theFile->autoSaveName);
					}
					else if( theFile->path ){
					  OSType ftype;
					  int extpos = strlen(theFile->path);
						fname = strdup(theFile->path);
						DetermineOSType( NULL, fname, &ftype );
						switch( ftype ){
							case 'jpgs':
							case 'QI2M':
							case 'QTSL':
								extpos = strlen(theFile->path) - 5;
								break;
							case 'VOBS':
								extpos = strlen(theFile->path) - 4;
								break;
							default:
								extpos = strlen(theFile->path) - 1;
						}
						strcpy( &fname[extpos], ".mov" );
					}
					if( fname ){
#	ifdef AUTOSAVE_REPLACES
						err2 = SaveMovieAsRefMov( fname, theFile, &refMovie );
						free(fname);
						if( refMovie ){
							DisposeMovie( theFile->theMovie );
							theFile->theMovie = refMovie;
							Log( qtLogPtr,
							    "SaveMovieAsRefMov() (autoSave+Open) took %gs, returned %d\n",
							    HRTime_toc(), (int) err2 );
						}
						else{
							Log( qtLogPtr,
							    "SaveMovieAsRefMov() (autoSave) took %gs (Open failed), returned %d\n",
							    HRTime_toc(), (int) err2 );
						}
#	else
						err2 = SaveMovieAsRefMov( fname, theFile, NULL );
						Log( qtLogPtr, "SaveMovieAsRefMov(\"%s\") (autoSave) took %gs, returned %d\n",
								fname, HRTime_toc(), (int) err2
						);
						free(fname);
#	endif
					}
					else{
						Log( qtLogPtr, "autoSave impossible: filename unknown!\n" );
					}
				}
			}
		}
bail:
#ifdef IMPORT_INTO_DESTREFMOV
	if( theFile->odataHandler ){
	 OSErr err2;
		// we only ought to get here if an error occurred.
		if( err == noErr ){
			UpdateMovie(theFile->theMovie);
			MaybeShowMoviePoster(theFile->theMovie);
			UpdateMovie(theFile->theMovie);
			err2 = AddMovieToStorage( theFile->theMovie, theFile->odataHandler );
			if( err2 == noErr ){
				err2 = CloseMovieStorage(theFile->odataHandler);
			}
			else{
				CloseMovieStorage(theFile->odataHandler);
			}
			theFile->odataHandler = NULL;
			DisposeHandle(theFile->odataRef);
			theFile->odataRef = NULL;
			Log( qtLogPtr, "Finalising destination (failed) ref movie '%s' (autoSave) returned %d\n",
				theFile->ofName, (int) err2
			);
		}
		else{
		  OSErr err3;
			err2 = CloseMovieStorage(theFile->odataHandler);
			theFile->odataHandler = NULL;
			err3 = DeleteMovieStorage( theFile->odataRef, theFile->odataRefType );
			DisposeHandle(theFile->odataRef);
			theFile->odataRef = NULL;
//			errno = 0;
//			unlink( theFile->ofName ); err3 = errno;
			Log( qtLogPtr, "Finalising and unlinking destination (failed) ref movie '%s' (autoSave) returned %d/%d\n",
				theFile->ofName, (int) err2, err3
			);
		}
	}
#endif
	theFile->progressDialog = CloseProgressDialog( theFile->progressDialog );
	theFile->progressDialog = NULL;
#if TARGET_OS_WIN32
	if( lastErrorFile ){
		free(lastErrorFile);
		lastErrorFile = NULL;
	}
	if( theElement && theElement->elementKind == kQI2MSourceImageKind ){
		lastErrorFile = strdup(theElement->image.theURL);
		lastError = err;
	}
#endif
#ifdef _NSLOGGERCLIENT_H
	NSLogFlushLog();
#endif
	return err;
}

// from TN1195
// create a dataRef (i.e. a handle) to a generic pointer.
Handle createPointerReferenceHandle(void *data, Size dataSize)
{ PointerDataRefRecord ptrDataRefRec;
  Handle dataRef = NULL;
  OSErr err = noErr;
	ptrDataRefRec.data = data;
	ptrDataRefRec.dataLength = dataSize;
	// create a data reference handle for our data
	err = PtrToHand( &ptrDataRefRec, &dataRef, sizeof(PointerDataRefRecord));
	return dataRef;
}

//!
//! ---------------------------------------------------------------------------------
//!		• AddImagesToMovie •
//!
//! Adds a single image sequence, movie or image to the destination movie.
//! ---------------------------------------------------------------------------------
//!
OSErr AddImagesToMovie(QI2MSourceFile theFile, QI2MSourceImage theImage, TimeValue *theTime, TimeValue *theSampleTime, int allow_inMem )
{
	FSSpec				URLSpec;
	ComponentResult 		err = noErr;
	ComponentInstance		ci = nil;
	Movie				refMovie = nil;
	Track				refTrack = nil;	//, theTrack = nil;
	Media				refMedia;	//, theMedia;
	SampleDescriptionHandle	desc = nil;
	long					dataOffset, size;
	OSType				mediaType;
	TimeValue				duration, insertionTime = *theTime;
	OSType				dataRefType;
	Handle				dataRef = nil;
	Ptr					theURL = nil;
	short				newDataRefIndex = 0;
#ifdef TIMING
	double				startTime, endTime;
	int					startLine;
#endif

	// copy the url, which is a C string, possibly from theImage->src, which is a Pascal string
	if( theImage->theURL ){
		theURL = theImage->theURL;
	}
	else{
		theURL = NewPtr((theImage->src)[0] + 1);
		if (theURL == nil) goto bail;
		BlockMove(theImage->src + 1, theURL, (theImage->src)[0]);
		theURL[(theImage->src)[0]] = '\0';
		theImage->theURL = theURL;
	}
	if( !theFile->path ){
		theFile->path = (const char*) strdup(theURL);
	}

	if( theImage->isSequence && !theImage->asMovie ){
	  OSType ftype;
	  IMG_FILE_IO jfio;
		DetermineOSType( NULL, theURL, &ftype );
		memset( &jfio, 0, sizeof(jfio) );
		switch( ftype ){
			default:
				Log( qtLogPtr, "AddImagesToMovie(): unsupported sequence format for \"%s\", trying as movie\n",
					theURL
				);
				// fall through:
			case kQTFileTypeMovie:
				theImage->isMovie = 1;
				if( !theFile->timeScale ){
					// delay setting the timescale:
					theFile->timeScale = -1;
				}
				break;
			case 'jpgs':{
#ifdef IMPORT_JPGS
			  JPGSSources vs;
				// a very simple image sequence format. Currently hardwired
				// for jpeg images, but could be extended to other compression types.
				jfio.open = (void*) _open_JPGSFile;
				jfio.close = _close_JPGSFile;
				jfio.rewind = _rewind_JPGSFile;
				jfio.eof = (void*) _eof_JPGSFile;
				jfio.error = (void*) _error_JPGSFile;
				jfio.ReadNextIMGFrame = (void*) ReadNextIMGFrame_JPGS;
				jfio.log = theImage->log;
				jfio.sourcePtr = &vs;
				theImage->discoveredWidth = theImage->discoveredHeight = 0;
				err = Add_imgFrames_From_File( theFile, theImage, theURL, &jfio, -1, theTime, &insertionTime );
#else
				err = featureUnsupported;
#endif
				goto bail;
				break;
			}
#pragma mark ----AddImagesToMovie-add imgFrames_From_File----
			case 'VOBS':{
#ifdef IMPORT_VOD
			  VODSources vs;
				memset( &vs, 0, sizeof(vs) );
				jfio.open = (void*) _open_VODFile;
				jfio.close = _close_VODFile;
				jfio.rewind = _rewind_VODFile;
				jfio.eof = (void*) _eof_VODFile;
				jfio.error = (void*) _error_VODFile;
				jfio.ReadNextIMGFrame = (void*) ReadNextIMGFrame_VOD;
				// if an image frequency was specified in the QI2M file, use it
				// to avoid scanning the VOD file to estimate the maximum rate.
				jfio.frameRate = vs.default_frameRate = theImage->frequency;
				vs.useVMGI = theImage->useVMGI;
				jfio.log = theImage->log;
				jfio.sourcePtr = &vs;
				if( !theFile->timeScale ){
					if( theImage->frequency > 0 ){
						SetMovieTimeScale( theFile->theMovie, (TimeScale)(theImage->frequency * 1000 + 0.5) );
						theFile->timeScale = GetMovieTimeScale(theFile->theMovie);
					}
				}
				theImage->discoveredWidth = theImage->discoveredHeight = 0;
				err = Add_imgFrames_From_File( theFile, theImage, theURL, &jfio,
							theImage->maxFrames, theTime, &insertionTime
				);
#else
				err = featureUnsupported;
#endif
				goto bail;
				break;
			}
		}
	}

	if( !theFile->timeScale ){
		theFile->timeScale = (theImage->isSequence)? -1 : GetMovieTimeScale(theFile->theMovie);
	}

	// get the data ref and try to open the movie
	err = PathToFSSpec( theURL, &URLSpec );
	if( err == noErr ){
		if( !theFile->theFileSpec.name[0] ){
			theFile->theFileSpec = URLSpec;
		}
#ifdef TIMING
		startLine = __LINE__ + 1;
		startTime = HRTime_tic();
#endif
		err = QTNewDataReferenceFromFSSpec( &URLSpec, 0, &dataRef, &dataRefType );
		AddFilenamingExtension( dataRef, NULL,  theURL, 1 );
#ifdef TIMING
		endTime = HRTime_tic() - startTime;
		Log( qtLogPtr, "\t%s lines %d-%d took %gs\n", __FILE__, startLine, __LINE__-1, endTime );
		totTime += endTime;
#endif
	}
	else{
		URLSpec.name[0] = 0;
#ifdef DEBUG
		Log( qtLogPtr, "Could not convert \"%s\" to FSSpec (%d)\n", theURL, (int) err );
#endif
	}
	if( err != noErr ){
#ifdef DEBUG
		Log( qtLogPtr, "Couldn't QTNewDataReferenceFromFSSpec() for \"%s\" (%d)\n", theURL, (int) err );
#endif
		URLSpec.name[0] = 0;
#ifdef TIMING
		startLine = __LINE__ + 1;
		startTime = HRTime_tic();
#endif
		err = CreateDataRefFromURL(theURL, theFile->dataRef, theFile->dataRefType, &dataRef, &dataRefType);
		if( err != noErr ){
		  const char *lURL = theURL;
			err = DataRefFromURL( &lURL, &dataRef, &dataRefType );
			if( lURL != theURL ){
				free( (void*) lURL);
			}
		}
#ifdef TIMING
		endTime = HRTime_tic() - startTime;
		Log( qtLogPtr, "\t%s lines %d-%d took %gs\n", __FILE__, startLine, __LINE__-1, endTime );
		totTime += endTime;
#endif
		if( err == noErr && !theFile->theImageTrack ){
		  char path[1024];
		  Str255 fileName;
			GetFilenameFromDataRef( dataRef, dataRefType, path, sizeof(path)/sizeof(char), fileName );
		}
	}
	if (err != noErr ){
		Log( qtLogPtr, "Can't create dataRef from URL %s (%d)\n", theImage->theURL, (int) err );
		goto bail;
	}
#ifdef DEBUG
	else{
		Log( qtLogPtr, "%s (refType %.4s/%.4s) -> Ref %p->%p=0x%lx\n",
		    theURL, OSTStr(theFile->dataRefType), OSTStr(dataRefType),
		    dataRef, *dataRef, *((unsigned long*) *dataRef)
		);
	}
#endif
	if( URLSpec.name[0] == 0 ){
#ifdef TIMING
		startLine = __LINE__ + 1;
		startTime = HRTime_tic();
#endif
		err = NewMovieFromDataRef(&refMovie, 0, nil, dataRef, dataRefType);
#ifdef TIMING
		endTime = HRTime_tic() - startTime;
		Log( qtLogPtr, "\t%s lines %d-%d took %gs\n", __FILE__, startLine, __LINE__-1, endTime );
		totTime += endTime;
#endif
	}
	else{
	  short resId = 0, resRefNum;
#ifdef TIMING
		startLine = __LINE__ + 1;
		startTime = HRTime_tic();
#endif
		err = OpenMovieFile( &URLSpec, &resRefNum, fsRdPerm );
		if( err ){
			Log( qtLogPtr, "Cannot OpenMovieFile(%s) - %d/%d\n", theURL, (int) err, GetMoviesError() );
		}
		else{
			err = NewMovieFromFile( &refMovie, resRefNum, &resId, nil, newMovieDontResolveDataRefs, nil );
			CloseMovieFile(resRefNum);
		}
#ifdef TIMING
		endTime = HRTime_tic() - startTime;
		Log( qtLogPtr, "\t%s lines %d-%d took %gs\n", __FILE__, startLine, __LINE__-1, endTime );
		totTime += endTime;
#endif
	}
	if (err || refMovie == nil){
		Log( qtLogPtr, "Can't open movie for %s (%d)\n", theImage->theURL, GetMoviesError() );
		goto bail;
	}

	if( theImage->isMovie ){
	  double fps, mFps, tcFps;
		GetMovieStaticFrameRate( refMovie, &fps, &tcFps, &mFps );
		Log( qtLogPtr, "%s: %gHz (media: %g, TimeCode: %g)\n", theImage->theURL, fps, mFps, tcFps );
	}

	// get the first track's media
	refTrack = GetMovieIndTrack(refMovie, 1);
	refMedia = GetTrackMedia(refTrack);

	// allocate sample description
	desc = (SampleDescriptionHandle)NewHandle(0);
	err = MemError();
	if (err) goto bail;

	// get the ref media sample reference
#ifdef TIMING
	startLine = __LINE__ + 1;
	startTime = HRTime_tic();
#endif
	err = GetMediaSampleReference(refMedia, &dataOffset, &size, 0, nil, nil, desc, nil, 1, nil, 0);
	GetMediaHandlerDescription(refMedia, &mediaType, nil, nil);
#ifdef TIMING
	endTime = HRTime_tic() - startTime;
//	Log( qtLogPtr, "\t%s lines %d-%d took %gs\n", __FILE__, startLine, __LINE__-1, endTime );
	totTime += endTime;
#endif
	if( (err && !theImage->isMovie) /* || mediaType != VideoMediaType */ ){
		Log( qtLogPtr, "%s: Cannot SampleReference (%d); mediaType %lx ?= VideoMediaType (%lx)\n",
			    theImage->theURL, (int) err, (unsigned long) mediaType, (unsigned long) VideoMediaType
		);
		goto bail;
	}

	if( theImage->isMovie ){
#pragma mark ----AddImagesToMovie-add movie----
		duration = GetMovieDuration(refMovie);
		{ long tracks = GetMovieTrackCount(theFile->theMovie), srcTracks = GetMovieTrackCount(refMovie);
			if( srcTracks > 0 && theFile->timeScale <= 0 ){
				SetMovieTimeScale( theFile->theMovie, GetMovieTimeScale(refMovie) );
				theFile->timeScale = GetMovieTimeScale(refMovie);
			}
			if( theImage->hflip || theImage->channel > 0 ){
			  Track cTrack = theFile->currentTrack, refTrack;
			  long idx = 1;
				while( (refTrack = GetMovieIndTrackType( refMovie, idx, VideoMediaType, movieTrackMediaType|movieTrackEnabledOnly)) ){
					theFile->currentTrack = refTrack;
					ClipFlipCurrentTrackToQuadrant( theFile, theImage->channel-1, theImage->hflip );
					idx += 1;
				}
				theFile->currentTrack = cTrack;
				theImage->clipflipDone = TRUE;
			}
			if( theFile->numImages == 1 || theImage->asMovie ){
				// InsertMovieSegment() inserts tracks and/or copies into existing track(s) as it seems fit
				// so it is not really what we want... unless we're only adding from a single source. In that
				// case, InsertMovieSegment has the big advantage of copying any metadata too.
				err= InsertMovieSegment( refMovie, theFile->theMovie, 0, duration, *theTime );
				UpdateMovie( theFile->theMovie );
				if( err ){
					Log( qtLogPtr, "%s: error inserting movie segment: %d\n", theImage->theURL, GetMoviesError() );
					goto bail;
				}
				else{
				  Rect bounds;
					// 20101217: we really need to set the discovered dimensions, if not only to avoid
					// setting any TimeCode tracks to 0 width later on...
					GetMovieNaturalBoundsRect(refMovie, &bounds );
					theImage->discoveredWidth = Long2Fix( abs(bounds.right - bounds.left) );
					theImage->discoveredHeight = Long2Fix( abs(bounds.bottom - bounds.top) );
					if( GetMovieTrackCount(theFile->theMovie) == srcTracks ){
					  // in this case we can be reasonably sure that the movie's 1st video track has received
					  // media from the source movie.
						theImage->theTrack = theFile->currentTrack = GetMovieIndTrackType(theFile->theMovie,
												1, VideoMediaType, movieTrackMediaType|movieTrackEnabledOnly
						);
						theFile->theImage.Media = GetTrackMedia(theFile->currentTrack);
					}
					else{
					  // we really have no idea in what track(s) the source movie was imported...
					  // but if we are to select a channel and/or do a flip, we need a current track!
						if( theImage->hflip || (theImage->channel >= 1 && theImage->channel <= 4) ){
						  Track t;
						  long idx = 1;
							// find the last enabled video track.
							while( (t = GetMovieIndTrackType(theFile->theMovie, idx, VideoMediaType, movieTrackMediaType|movieTrackEnabledOnly)) ){
								theImage->theTrack = theFile->currentTrack = t;
								idx += 1;
							}
							theFile->theImage.Media = GetTrackMedia(theFile->currentTrack);
						}
						else{
							theFile->currentTrack = NULL;
						}
					}
				}
			}
			else{
			// we loop over all tracks in refMovie, and copy the video tracks into the destination movie
			// creating new tracks with associated media as we go, unless MustUseTrack==True (behaviour
			// in that case is unknown...)
			  long i, n;
			  Track srcTrack = (Track) -1;
			  Media srcMedia;
			  Handle cdataRef;
			  OSType cdataRefType;
				theFile->theImage.Media = nil;
#if NOT_GETMEDIADATAREF
				if( (err = URLtoDataRef( theImage->theURL, theFile, &cdataRef, &cdataRefType )) != noErr ){
					Log( qtLogPtr, "AddImagesToMovie(): cannot get dataRef for \"%s\" (%d)\n", theImage->theURL, err );
					cdataRef = nil;
					cdataRefType = 0;
					err = noErr;
				}
#endif
				theImage->discoveredWidth = theImage->discoveredHeight = 0;
				for( n = 0, i = 1; i <= srcTracks && srcTrack; i++ ){
				  OSType srcTrackType;
					if( (srcTrack = GetMovieIndTrack/*Type*/(refMovie, i/*, VideoMediaType, movieTrackMediaType*/ )) ){
					  short refcnt;
					  long attr;
						if( (srcMedia = GetTrackMedia(srcTrack)) ){
							GetMediaHandlerDescription(srcMedia, &srcTrackType, nil, nil);
							GetMediaDataRefCount( srcMedia, &refcnt );
							if( (err = GetMediaDataRef( srcMedia, MIN(1,refcnt), &cdataRef, &cdataRefType, &attr )) ){
								Log( qtLogPtr, "Error retrieving MediaDataRef for %s track %ld with %hd datarefs (%d)\n",
									theImage->theURL, i, refcnt, (int) err
								);
								cdataRef = nil, cdataRefType = 0;
								err = noErr;
							}
						}
					}
					if( srcTrack && srcMedia ){
						if( !theFile->MustUseTrack ){
						  Fixed w, h;
							GetTrackDimensions( srcTrack, &w, &h );
							if( w > theImage->discoveredWidth ){
								theImage->discoveredWidth = w;
							}
							if( h > theImage->discoveredHeight ){
								theImage->discoveredHeight = h;
							}
#if 1
							err = AddEmptyTrackToMovie( srcTrack, theFile->theMovie, cdataRef, cdataRefType, &theFile->theImageTrack );
							if( err == noErr && theFile->theImageTrack ){
								theImage->theTrack = theFile->theImageTrack;
								theFile->theImage.Media = GetTrackMedia(theFile->theImageTrack);
								err = GetMoviesError();
							}
#else
							theImage->theTrack = theFile->theImageTrack = NewMovieTrack( theFile->theMovie,
										w, h, GetTrackVolume(srcTrack)
							);
#endif
						}
						if( theFile->theImageTrack ){
							if( !theFile->theImage.Media ){
								// dataRef,dataRefType refer to refMovie on disk, but passing them in gives
								// a couldNotResolveDataRef on QT 7.6.6/Win32?!
								theFile->theImage.Media = NewTrackMedia( theFile->theImageTrack, srcTrackType,
											theFile->timeScale, cdataRef, cdataRefType
								);
								err = GetMoviesError();
							}
							if( theFile->theImage.Media && err == noErr ){
								BeginMediaEdits(theFile->theImage.Media);
								err = InsertTrackSegment( srcTrack, theFile->theImageTrack,
										0, GetTrackDuration(srcTrack), *theTime
								);
								EndMediaEdits(theFile->theImage.Media);
								// no need to call InsertMediaIntoTrack here (or the media gets added twice?!)
//								InsertMediaIntoTrack( theFile->theImageTrack, *theTime,
//										0, theFile->timeScale * GetTrackDuration(srcTrack) / GetMovieTimeScale(refMovie),
//										fixed1
//								);
								// 20101118: copy over track metadata!
								if( err == noErr ){
									CopyTrackMetaData( theFile->theMovie, theFile->theImageTrack, refMovie, srcTrack );
									if( srcTrackType == VideoMediaType ){
										// update the currentTrack pointer (which we keep pointing to Video material only, here)
										theFile->currentTrack = theFile->theImageTrack;
									}
									else{
										// update the current Media reference. From here on, this will be used only in determining
										// the static frame rate that will be stored in an additional metadata entry. Sensical only
										// for video material.
										if( theFile->currentTrack ){
											theFile->theImage.Media = GetTrackMedia(theFile->currentTrack);
										}
										else{
											theFile->theImage.Media = nil;
										}
									}
								}
							}
							else{
								Log( qtLogPtr, "%s: error creating '%.4s' media for destination track %ld (%d)\n",
									theImage->theURL, OSTStr(cdataRefType), tracks + 1 + i, (int) err
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
							if( !theFile->MustUseTrack ){
								DisposeMovieTrack( theFile->theImageTrack );
								theImage->theTrack = theFile->theImageTrack = nil;
							}
							Log( qtLogPtr, "%s: error inserting track %ld into destination track %ld (%d)\n",
								theImage->theURL, i, tracks + 1 + i, (int) err
							);
							goto bail;
						}
						else if( !theFile->MustUseTrack ){
							theFile->theImageTrack = nil;
						}
						n += 1;
						UpdateMovie( theFile->theMovie );
					}
				}
				if( err == noErr ){
					CopyMovieSettings( refMovie, theFile->theMovie );
					CopyMovieUserData( refMovie, theFile->theMovie, kQTCopyUserDataReplace );
					CopyMovieMetaData( theFile->theMovie, refMovie );
				}
			}
			if( GetMovieTrackCount(theFile->theMovie) > tracks ){
				theFile->numTracks = GetMovieTrackCount(theFile->theMovie);
				if( !theFile->PosterIsSet ){
					SetMoviePosterTime( theFile->theMovie, GetMoviePosterTime(refMovie) );
					theFile->PosterIsSet = TRUE;
				}
				if( !theImage->ignoreRecordStartTime ){
				  TimeRecord mTime;
					GetMovieTime( theFile->theMovie, &mTime );
					mTime.value.hi = 0;
					mTime.value.lo = GetMoviePosterTime(refMovie);
					SetMovieTime( theFile->theMovie, &mTime );
				}
				MaybeShowMoviePoster(theFile->theMovie);
				UpdateMovie( theFile->theMovie );
			}
		}
#ifdef DEBUG
		Log( qtLogPtr, "%s %s: offset=%lu duration=%lu sampleTime=%lu\n",
			    (theImage->isMovie)? "Movie" : "Image", theImage->theURL, *theTime, duration, *theSampleTime
		);
#endif
		if( !theFile->currentTrack ){
			theFile->currentTrack = GetMovieIndTrack( theFile->theMovie, theFile->numTracks );
		}
//		GetTrackDimensions( theFile->currentTrack, &theImage->discoveredWidth, &theImage->discoveredHeight );
		if( !theFile->MustUseTrack ){
			// any previous image track ends here, the next image added will start its own track.
			theFile->theImageTrack = nil;
		}
	}
	else{ // ! isMovie
#pragma mark ----AddImagesToMovie-add image----
		theImage->discoveredWidth = Long2Fix((**(ImageDescriptionHandle)desc).width);
		theImage->discoveredHeight = Long2Fix((**(ImageDescriptionHandle)desc).height);

		// add a media sample reference to our movie
		// RJVB: support for millisecond duration spec that overrides seconds:
		if( theImage->mduration> 0 ){
			TimeValue d= ((TimeValue)(theImage->mduration * 0.001 * theFile->timeScale + 0.5));
			duration = max(1, d);
		}
		else if( theImage->duration > 0 ){
			duration = (TimeValue) ( theImage->duration * theFile->timeScale + 0.5 );
		}
		else{
			duration = 1;
		}

		if( nil == theFile->theImageTrack ){
			// create a new track; we should never get here if theFile->MustUseTrack!
			if(theFile->movieWidth == 0 ){
				theFile->movieWidth = theImage->discoveredWidth >> 16;
			}
			if( theFile->movieHeight == 0 ){
				theFile->movieHeight = theImage->discoveredHeight >> 16;
			}
#	ifdef DEBUG
			Log( qtLogPtr, "%s (%.4s): discovered size: %dx%d -> %dx%d, using %dx%d\n", theImage->theURL,
				OSTStr((**(ImageDescriptionHandle)desc).cType),
				(**(ImageDescriptionHandle)desc).width, (**(ImageDescriptionHandle)desc).height,
				FixRound(theImage->discoveredWidth), FixRound(theImage->discoveredHeight),
				FixRound((theFile->movieWidth << 16)), FixRound((theFile->movieHeight << 16))
			);
#	endif
			theImage->theTrack = theFile->theImageTrack = NewMovieTrack(theFile->theMovie,
									theFile->movieWidth ? (theFile->movieWidth << 16) : theImage->discoveredWidth,
									theFile->movieHeight ? (theFile->movieHeight << 16) : theImage->discoveredHeight, 0);
			if (theFile->theImageTrack == nil) goto bail;
			theFile->numTracks += 1;

			// create a track media
			theFile->theImage.Media = NewTrackMedia(theFile->theImageTrack, VideoMediaType, theFile->timeScale, nil, 0);
			if (theFile->theImage.Media == nil) goto bail;
			theFile->currentTrack = theFile->theImageTrack;
		}
		if( dataRef ){
			// add data ref to media for this image's file. This may return an already added data ref.
#ifdef TIMING
			startLine = __LINE__ + 1;
			startTime = HRTime_tic();
#endif
			err = AddMediaDataRef(theFile->theImage.Media, &newDataRefIndex, dataRef, dataRefType);
#ifdef TIMING
			endTime = HRTime_tic() - startTime;
//			Log( qtLogPtr, "\t%s lines %d-%d took %gs\n", __FILE__, startLine, __LINE__-1, endTime );
			totTime += endTime;
#endif
			if (err) goto bail;

			theImage->dataRefIndex = (**desc).dataRefIndex = newDataRefIndex;

			// •• we DO NOT ever write to the track so this is OK
			// by not calling the BeginMediaEdits/EndMediaEdits pair
			// we are adding only references
			//	err	= BeginMediaEdits(theMedia);
			//	if (err) goto bail;
#ifdef TIMING
			startLine = __LINE__ + 1;
			startTime = HRTime_tic();
#endif
			err = AddMediaSampleReference(theFile->theImage.Media, dataOffset, size, duration, desc, 1, 0, theSampleTime);
#ifdef TIMING
			endTime = HRTime_tic() - startTime;
			Log( qtLogPtr, "\t%s lines %d-%d took %gs\n", __FILE__, startLine, __LINE__-1, endTime );
			totTime += endTime;
#endif
			if (err) goto bail;
		}
		else{
			// This is how theSampleTime appears to grow:
			*theSampleTime += theFile->prevDuration;
		}
#ifdef DEBUG
		Log( qtLogPtr, "%s %s: offset=%lu duration=%lu sampleTime=%lu\n",
			    (theImage->isMovie)? "Movie" : "Image", theImage->theURL, *theTime, duration, *theSampleTime );
#endif
	//	err = EndMediaEdits(theMedia);
	//	if (err) goto bail;
	//	err	= InsertMediaIntoTrack(theTrack, *theTime, 0, duration, FixDiv(Long2Fix(GetMediaDuration(theMedia)), Long2Fix(duration)));
#ifdef TIMING
		startLine = __LINE__ + 1;
		startTime = HRTime_tic();
#endif
		err	= InsertMediaIntoTrack(theFile->theImageTrack, *theTime, *theSampleTime, duration, fixed1);
#ifdef TIMING
		endTime = HRTime_tic() - startTime;
//		Log( qtLogPtr, "\t%s lines %d-%d took %gs\n", __FILE__, startLine, __LINE__-1, endTime );
		totTime += endTime;
#endif
		if (err) goto bail;
	}

// inserted:

	theImage->sampleTime = *theSampleTime;
	theFile->prevDuration = duration;
	// increment our movie time
	*theTime += duration;

	// 20101118: add some metadata here
	{ struct stat st;
	  char mdbuf[512], *mdbufptr;
	  size_t blen;
		if( !theFile->currentTrack ){
			// we'll be adding metadata to the current track, which we set to something reasonable
			// here, if not yet set. If it remains NULL, the metadata will be added to the Movie instead.
			theFile->currentTrack = theFile->theImageTrack;
		}
		blen = snprintf( mdbuf, sizeof(mdbuf), "%s", theImage->theURL );
		MetaDataHandler( theFile->theMovie, theFile->currentTrack, akSource, &theImage->theURL, NULL, "\n" );
		if( !stat( theImage->theURL, &st ) ){
		  struct tm *tm;
			tm = localtime( &st.st_mtime );
			if( tm ){
				blen += snprintf( &mdbuf[blen], sizeof(mdbuf)-blen, ": %s", asctime(tm) );
				if( mdbuf[blen-1] == '\n' ){
					mdbuf[blen-1] = '\0';
					blen -= 1;
				}
			}
		}
		blen += snprintf( &mdbuf[blen], sizeof(mdbuf)-blen, "; %gs", ((double)duration)/theFile->timeScale );
		if( theFile->theImage.Media ){
		  double fps;
		  long frames;
			if( GetMediaStaticFrameRate( theFile->theImage.Media, &fps, &frames, NULL ) == noErr ){
				blen += snprintf( &mdbuf[blen], sizeof(mdbuf)-blen, " %ld frames @ %gHz", frames, fps );
			}
		}
		mdbufptr = mdbuf;
		MetaDataHandler( theFile->theMovie, theFile->currentTrack, akInfo, &mdbufptr, NULL, "\n" );
#ifdef DEBUG
		{ char *istr = NULL;
			if( MetaDataHandler( theFile->theMovie, theFile->currentTrack, akInfo, &istr, NULL, "\n" ) == noErr
			   && istr
			){
				Log( qtLogPtr, "%s: Info=\"%s\"\n", theImage->theURL, istr );
				free(istr);
			}
		}
#endif
	}

#pragma mark ----AddImagesToMovie-bail----
bail:

	theImage->displayWidth = theImage->discoveredWidth;
	theImage->displayHeight = theImage->discoveredHeight;

	if( err == noErr ){
		if( theImage->isSequence ){
			if( theImage->hideTC ){
				IsolateTCTracks( theFile, theImage, -1, TRUE, FALSE );
			}
			if( !theImage->clipflipDone ){
				// 20100727: MDR304 uses channel 1 (top-left) - 4 (bottom-right), so subtract 1 to go to quadrants
				ClipFlipCurrentTrackToQuadrant( theFile, theImage->channel-1, theImage->hflip );
			}
			if( GetMoviesError() == noErr ){
				if( theImage->channel >= 1 && theImage->channel <=4 ){
					theImage->displayWidth = FixDiv(theImage->discoveredWidth, Long2Fix(2) );
					theImage->displayHeight = FixDiv(theImage->discoveredHeight, Long2Fix(2) );
				}
				else if( theImage->channel == 5 ){
					// disable all tracks that aren't TimeCode tracks
					 IsolateTCTracks( theFile, theImage, TRUE, theImage->hideTC, FALSE );
				}
				else if( theImage->channel == 6 ){
					IsolateTCTracks( theFile, theImage, TRUE, TRUE, TRUE );
				}
			}
			else{
				Log( qtLogPtr, "%s: couldn't hflip and/or channel-select the current track (%u)\n",
				    theImage->theURL, GetMoviesError()
				);
			}
		}
		if( theImage->transH || theImage->transV ){
		  MatrixRecord m;
			GetTrackMatrix( theFile->currentTrack, &m );
			TranslateMatrix( &m, Long2Fix(theImage->transH), Long2Fix(theImage->transV) );
			SetTrackMatrix( theFile->currentTrack, &m );
		}
		if( theImage->relTransH || theImage->relTransV ){
		  MatrixRecord m;
		  double dx, dy;
			GetTrackMatrix( theFile->currentTrack, &m );
			dx = FixRound(theImage->displayWidth) * theImage->relTransH + 0.5;
			dy = FixRound(theImage->displayHeight) * theImage->relTransV + 0.5;
			TranslateMatrix( &m, FloatToFixed(dx), FloatToFixed(dy) );
			SetTrackMatrix( theFile->currentTrack, &m );
		}
		if( theImage->description ){
			AddMetaDataStringToCurrentTrack( theFile, akDescr, (const char*) theImage->description, NULL );
		}
		AddMetaDataStringToCurrentTrack( theFile, akDisplayName, (const char*) P2Cstr(theImage->src), NULL );
		if( theImage->newChapter ){
			// avoid overwriting an existing chapter (e.g. a startGPS marker!)
			if( GetMovieChapterTextAtTime( theFile, insertionTime, NULL ) == noErr ){
				insertionTime += 1;
			}
			AddChapterTitle( theFile,
						 (theFile->theTCTrack)? theFile->theTCTrack : theFile->currentTrack,
						 basename(theImage->theURL), insertionTime, 1
			);
		}
	}

	if( dataRef ){
		DisposeHandle(dataRef);
	}
	if( refTrack ){
		DisposeMovieTrack(refTrack);
	}
	if( refMovie ){
		DisposeMovie(refMovie);
	}

	if( desc ){
		DisposeHandle((Handle)desc);
	}

	if( ci ){
		CloseComponent(ci);
	}

	return err;
}


// ---------------------------------------------------------------------------------
//		• AddAudioToMovie •
//
//  Open our audio source file as a movie, then add the audio track from this source
//  movie to our destination movie.
//
// ---------------------------------------------------------------------------------
OSErr AddAudioToMovie(QI2MSourceFile theFile, QI2MSourceMovie theMovie, TimeValue *theTime)
{
	ComponentResult 		err 			= noErr;
	ComponentInstance		ci 				= nil;
	Movie					refMovie;
	SampleDescriptionHandle	desc 			= nil;
	OSType					dataRefType;
	Handle					dataRef 		= nil;
	Ptr						theURL 			= nil;

	// copy the url
	theURL = NewPtr((theMovie->src)[0] + 1);
	if (theURL == nil) goto bail;
	BlockMove(theMovie->src + 1, theURL, (theMovie->src)[0]);
	theURL[(theMovie->src)[0]] = '\0';

	// get the data ref and try to open the movie
	err = CreateDataRefFromURL(theURL, theFile->dataRef, theFile->dataRefType, &dataRef, &dataRefType);
	err = NewMovieFromDataRef(&refMovie, newMovieDontResolveDataRefs, nil, dataRef, dataRefType);
	if (err || refMovie == nil) goto bail;

	// add the movie to the track
	if (theMovie->dontScaleImagesToThisMovie)
	{
		SetMovieSelection(refMovie, 0, GetMovieDuration(refMovie));
		AddMovieSelection(theFile->theMovie, refMovie);
	}
	else
	{
		ScaleMovieSegment(theFile->theMovie, 0, GetMovieDuration(theFile->theMovie), GetMovieDuration(refMovie));
		SetMovieSelection(refMovie, 0, GetMovieDuration(refMovie));
		AddMovieSelection(theFile->theMovie, refMovie);
	}

	// increment our movie time
	*theTime = 0;

bail:
	if (dataRef)
	{
		DisposeHandle(dataRef);
	}

	if (desc)
	{
		DisposeHandle((Handle)desc);
	}

	if (ci)
	{
		CloseComponent(ci);
	}

	return err;
}


// ---------------------------------------------------------------------------------
//		• CreateDataRefFromURL •
//
//  Create a URL data reference
//
// ---------------------------------------------------------------------------------

OSErr CreateDataRefFromURL(char* theURL, Handle baseDRef, OSType baseDRefType, Handle *dref, OSType *drefType)
{
	ComponentInstance	ci 				= 0;
	Handle				absoluteDRef 	= NULL;
	OSErr				err				= noErr;

	*dref = NULL;

	err	= PtrToHand(theURL, dref, (SInt32)(strlen(theURL) + 1));
	if (err) goto Bail;

	*drefType = URLDataHandlerSubType;

	if( (err= OpenAComponent(GetDataHandler(baseDRef, baseDRefType, kDataHCanRead), &ci)) != noErr)
		goto Bail;

	if( (err= DataHSetDataRefWithAnchor(ci, baseDRef, URLDataHandlerSubType, *dref)) != noErr)
		goto Bail;

	if( (err= DataHGetDataRefAsType(ci, baseDRefType, &absoluteDRef)) != noErr)
		goto Bail;

	DisposeHandle(*dref);
	*dref = absoluteDRef;
	{ OSErr err2;
		err2 = AddFilenamingExtension( *dref, NULL, theURL, 1 );
	}

	*drefType = baseDRefType;

Bail:
	if (NULL != ci)
	{
		CloseComponent(ci);
	}

	return	err;
}

//!
//! ---------------------------------------------------------------------------------
//!		• DisposeQI2MSourceFile •
//!
//! Delete the slide show file object and all its elements
//! ---------------------------------------------------------------------------------
//!
void DisposeQI2MSourceFile(QI2MSourceFile theFile)
{
	// delete the entire element list
	DisposeAllElements(theFile);

	if( theFile->dataRef){
		DisposeHandle(theFile->dataRef);
	}
	if( theFile->cwd ){
		free( (void*) theFile->cwd);
	}
	if( theFile->path ){
		free( (void*) theFile->path );
	}
	if( theFile->ofName ){
		free( (void*) theFile->ofName );
	}
	if( theFile->autoSaveName ){
		DisposePtr( (Ptr) theFile->autoSaveName );
	}
	while( theFile->TCInfoList ){
		theFile->TCInfoList = dispose_TCTrackInfo( theFile->TCInfoList );
	}
	// remove the QTImage2MovGlobalsRecord reference to ourselves:
	theFile->theStore->theFile = NULL;
	theFile->theStore = NULL;

	// delete the file object
	DisposePtr((Ptr)theFile);
}


//!
//! ---------------------------------------------------------------------------------
//!		• DisposeElementFields •
//!
//! Dispose the image element fields
//! ---------------------------------------------------------------------------------
//!
void DisposeElementFields(QI2MSourceElement element)
{
	if (element)
	{
		switch( (long) element->elementKind )
		{
			case kQI2MSourceImageKind:
			{
				if( element->image.src ){
					DisposePtr((Ptr)element->image.src);
				}
				if( element->image.ffmpCodec ){
					DisposePtr((Ptr)element->image.ffmpCodec);
				}
				if( element->image.ffmpBitRate ){
					DisposePtr((Ptr)element->image.ffmpBitRate);
				}
				if( element->image.description ){
					DisposePtr((Ptr)element->image.description);
				}
				if( element->image.theURL ){
					DisposePtr( (Ptr)element->image.theURL );
				}
				element->image.src = nil;
				break;
			}
			case kQI2MSourceChaptKind:
				if( element->chapterMark.title ){
					DisposePtr( (Ptr)element->chapterMark.title );
				}
				break;
			case kQI2MSourceDescrKind:
				if( element->description ){
					DisposePtr((Ptr)element->description);
				}
				if( element->language ){
					DisposePtr((Ptr)element->language);
				}
				break;

			default:
				break;
		}
	}
}


//!
//! ---------------------------------------------------------------------------------
//!		• DisposeAllElements •
//!
//! Dispose all elements from the slide show file
//! ---------------------------------------------------------------------------------
//!
void DisposeAllElements(QI2MSourceFile theFile)
{
	if (theFile->elements)
	{
		QI2MSourceElement element, temp;

		element = theFile->elements;

		// delete all the elements
		while (element)
		{
			temp = element;

			element = (QI2MSourceElement)(element->nextElement);

			DisposeElementFields(temp);
			DisposePtr((Ptr)temp);
		}

		theFile->elements = nil;
	}
}

// ---------------------------------------------------------------------------------
//		• GetPStringAttribute •
//
// Get a Pascal string attribute
// ---------------------------------------------------------------------------------
ComponentResult GetPStringAttribute(XMLAttributePtr attributes, xmlAttrs attributeType, StringPtr *theString)
{
	ComponentResult err = noErr;
	long 			attributeIndex, stringLength;

	// get the attribute index in the array
	attributeIndex = GetAttributeIndex(attributes, attributeType);

	// get the value
	if (attributeIndex != attributeNotFound && (attributes[attributeIndex].valueKind == attributeValueKindCharString))
	{
		// allocate the string
		stringLength = strlen((Ptr)(attributes[attributeIndex].valueStr));
		*theString = (StringPtr)NewPtr(stringLength + 1);
		err = MemError();
		if (err) goto bail;

		// copy the string value
		BlockMove(attributes[attributeIndex].valueStr, *theString + 1, stringLength);
		(*theString)[0] = stringLength;
	}
	else
	{
		err = attributeNotFound;
	}
bail:

	return err;
}

// ---------------------------------------------------------------------------------
//		• GetStringAttribute •
//
// Get a C string attribute
// ---------------------------------------------------------------------------------
ComponentResult GetStringAttribute(XMLAttributePtr attributes, xmlAttrs attributeType, StringPtr *theString)
{
	ComponentResult err = noErr;
	long 			attributeIndex, stringLength;

	// get the attribute index in the array
	attributeIndex = GetAttributeIndex(attributes, attributeType);

	// get the value
	if (attributeIndex != attributeNotFound && (attributes[attributeIndex].valueKind == attributeValueKindCharString))
	{
		// allocate the string
		stringLength = strlen((Ptr)(attributes[attributeIndex].valueStr));
		*theString = (StringPtr)NewPtr(stringLength + 1);
		err = MemError();
		if (err) goto bail;

		// copy the string value
		BlockMove(attributes[attributeIndex].valueStr, *theString, stringLength);
		(*theString)[stringLength] = '\0';
	}
	else
	{
		err = attributeNotFound;
	}
bail:

		return err;
}

// ---------------------------------------------------------------------------------
//		• GetIntegerAttribute •
//
// Get the integer attribute
// ---------------------------------------------------------------------------------
ComponentResult GetIntegerAttribute(XMLAttributePtr attributes, xmlAttrs attributeType, long *theNumber)
{
	ComponentResult err = noErr;
	long 			attributeIndex;

	// get the attribute index in the array
	attributeIndex = GetAttributeIndex(attributes, attributeType);

	// get the value
	if (attributeIndex != attributeNotFound && (attributes[attributeIndex].valueKind & attributeValueKindInteger) && theNumber)
	{
		*theNumber = attributes[attributeIndex].value.number;
	}
	else
	{
		err = attributeNotFound;
	}

	return err;
}

// ---------------------------------------------------------------------------------
//		• GetShortAttribute •
//
// Get the short attribute
// ---------------------------------------------------------------------------------
ComponentResult GetShortAttribute(XMLAttributePtr attributes, xmlAttrs attributeType, short *theNumber)
{
	ComponentResult err = noErr;
	long 			attributeIndex;

	// get the attribute index in the array
	attributeIndex = GetAttributeIndex(attributes, attributeType);

	// get the value
	if (attributeIndex != attributeNotFound && (attributes[attributeIndex].valueKind & attributeValueKindInteger) && theNumber)
	{
		*theNumber = (short) attributes[attributeIndex].value.number;
	}
	else
	{
		err = attributeNotFound;
	}

	return err;
}

/*!
	replaces all commas flanked by digits on both sides by a dot
 */
void Convert_French_Numerals( char *rbuf )
{ char *c = &rbuf[1];
  int i, n = strlen(rbuf)-1;
	for( i = 1 ; i < n ; i++, c++ ){
		if( *c == ',' && (isdigit(c[-1]) || isdigit(c[1])) ){
			*c = '.';
		}
	}
}

// ---------------------------------------------------------------------------------
//		• GetDoubleAttribute •
//
// Get the double attribute
// ---------------------------------------------------------------------------------
ComponentResult GetDoubleAttribute(XMLAttributePtr attributes, xmlAttrs attributeType, double *theNumber)
{
	ComponentResult err = noErr;
	long 			attributeIndex;

	// get the attribute index in the array
	attributeIndex = GetAttributeIndex(attributes, attributeType);

	// get the value
	if( attributeIndex != attributeNotFound
	   && (attributes[attributeIndex].valueKind == attributeValueKindDouble) && theNumber
	){
	  char *number = (char*) attributes[attributeIndex].valueStr;
		if( strncasecmp( number, "NaN", 3 ) == 0 ){
			set_NaN(*theNumber);
		}
		else if( strncasecmp( number, "Inf", 3 ) == 0 ){
			set_Inf(*theNumber,1);
		}
		else if( strncasecmp( number, "-Inf", 4 ) == 0 ){
			set_Inf(*theNumber,-1);
		}
		else{
			Convert_French_Numerals(number);
			sscanf( number, "%lf", theNumber );
		}
	}
	else
	{
		err = attributeNotFound;
	}

	return err;
}

// ---------------------------------------------------------------------------------
//		• GetBooleanAttribute •
//
// Get the Boolean attribute
// ---------------------------------------------------------------------------------
ComponentResult GetBooleanAttribute(XMLAttributePtr attributes, xmlAttrs attributeType, int *theBool)
{
	ComponentResult err = noErr;
	long 			attributeIndex;

	// get the attribute index in the array
	attributeIndex = GetAttributeIndex(attributes, attributeType);

	// get the value
	if (attributeIndex != attributeNotFound && (attributes[attributeIndex].valueKind & attributeValueKindBoolean) && theBool)
	{
		*theBool = attributes[attributeIndex].value.boolean;
	}
	else
	{
		err = attributeNotFound;
	}

	return err;
}

// ---------------------------------------------------------------------------------
//		• GetAttributeIndex •
//
// Get the index of the attribute
// ---------------------------------------------------------------------------------
long GetAttributeIndex(XMLAttributePtr attributes, xmlAttrs attributeType)
{
	long index = 0;

	if (!attributes)
	{
		index = attributeNotFound;
		goto bail;
	}

	while ((attributes[index]).identifier != xmlIdentifierInvalid && (attributes[index]).identifier != attributeType)
	{
		index++;
	}

	if ((attributes[index]).identifier == xmlIdentifierInvalid)
	{
		index = attributeNotFound;
	}

bail:

	return index;
}

// ---------------------------------------------------------------------------------
//		• max •
// ---------------------------------------------------------------------------------
#ifndef max
long max(long num1, long num2)
{
	if (num1 > num2)
	{
		return num1;
	}
	else
	{
		return num2;
	}
}
#endif

//!
//! ---------------------------------------------------------------------------------
//!		• CreateHandleDataRef •
//!
//! Make a handle data reference, i.e. a dataRef that points to an in-memory resource
//! ---------------------------------------------------------------------------------
//!
Handle CreateHandleDataRef()
{
	ComponentResult	err 	= noErr;
	Handle 			dataRef = nil;
	long			dataAtom[2];

	dataRef	= NewHandleClear(sizeof(Handle) + sizeof(char));
	err		= MemError();

	if( dataRef && err == noErr ){
		dataAtom[0]	= EndianU32_NtoB(sizeof(long) + sizeof(OSType));
		dataAtom[1]	= EndianU32_NtoB(FOUR_CHAR_CODE('data'));

		err	= PtrAndHand(dataAtom, dataRef, sizeof(long) + sizeof(OSType));

		if (err)
		{
			DisposeHandle(dataRef);
			dataRef = nil;
		}
	}

	return dataRef;
}

// ---------------------------------------------------------------------------------
//		• QTImage2MovRegister •
//
// Register our component manually, when running in linked mode
// ---------------------------------------------------------------------------------
#ifndef STAND_ALONE

extern void MyQTImage2MovRegister(void);

void MyQTImage2MovRegister(void)
{
	ComponentDescription	theComponentDescription;
	Handle					nameHdl;
	#if !TARGET_API_MAC_CARBON
		ComponentRoutineUPP componentEntryPoint = NewComponentRoutineProc(QTImage2MovImport_ComponentDispatch);
	#else
		ComponentRoutineUPP componentEntryPoint = NewComponentRoutineUPP((ComponentRoutineProcPtr)QTImage2MovImport_ComponentDispatch);
	#endif

	PtrToHand("\pQI2MSource", &nameHdl, 8);
  	theComponentDescription.componentType = MovieImportType;
  	theComponentDescription.componentSubType = FOUR_CHAR_CODE('QI2M');
  	theComponentDescription.componentManufacturer = kAppleManufacturer;
	// 20111123: added movieImporterIsXMLBased
  	theComponentDescription.componentFlags = canMovieImportFiles | canMovieImportInPlace
		| hasMovieImportMIMEList | canMovieImportDataReferences | movieImporterIsXMLBased;
  	theComponentDescription.componentFlagsMask = 0;

	RegisterComponent(&theComponentDescription, componentEntryPoint, 0, nameHdl, 0, 0);
}

#endif
