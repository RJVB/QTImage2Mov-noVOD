/*
	File:			QTImage2Mov-direct.c
	
	Description &c:	See QTImage2Mov.c
*/

#ifdef _MSC_VER
#	define	_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES	1
#endif

#include "copyright.h"
IDENTIFY("QTImage2Mov Importer entry point for .jpgs and .vod files");

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#if ! defined(_WINDOWS) && !defined(WIN32) && !defined(_MSC_VER)
#	include <unistd.h>
#	include <libgen.h>
#	include <sys/param.h>
#else
#	define strncasecmp(a,b,n)	_strnicmp((a),(b),(n))
#endif

#if __APPLE_CC__
#	include <Carbon/Carbon.h>
#	include <QuickTime/QuickTime.h>
#else
	#include <ImageCodec.h>
	#include <TextUtils.h>
	#include <string.h>
	#include <Files.h>
	#include <Movies.h>
	#include <MediaHandlers.h>
#endif


#include "QTImage2Mov.h"
#include "QTImage2MovConstants.h"
#include "QTImage2MovVersion.h"

#include "timing.h"

#include "AddImageFrames.h"

// ---------------------------------------------------------------------------------
//		• ComponentDispatchHelper Definitions •
// ---------------------------------------------------------------------------------

#define MOVIEIMPORT_BASENAME()		QTImage2MovImportDirect_
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

int NoXML = 0;

// ---------------------------------------------------------------------------------
pascal ComponentResult QTImage2MovImportDirect_Open(QTImage2MovGlobals store, ComponentInstance self)
{ extern pascal ComponentResult QTImage2MovImport_Open(QTImage2MovGlobals store, ComponentInstance self);
  int NX = NoXML;
  ComponentResult r;
	NoXML = 1;
	r = QTImage2MovImport_Open(store, self);
	NoXML = NX;
	return r;
}

pascal ComponentResult QTImage2MovImportDirect_Close(QTImage2MovGlobals store, ComponentInstance self)
{ extern pascal ComponentResult QTImage2MovImport_Close(QTImage2MovGlobals store, ComponentInstance self);
  int NX = NoXML;
  ComponentResult r;
	NoXML = 1;
	r = QTImage2MovImport_Close(store, self);
	NoXML = NX;
	return r;
}

pascal long QTImage2MovImportDirect_Version(QTImage2MovGlobals store)
{ extern pascal long QTImage2MovImport_Version(QTImage2MovGlobals store);
  int NX = NoXML;
  ComponentResult r;
	r = QTImage2MovImport_Version(store);
	NoXML = NX;
	return r;
}

pascal ComponentResult QTImage2MovImportDirect_File(QTImage2MovGlobals store,
									const FSSpec		*theFile,
									Movie 			theMovie,
									Track 			targetTrack,
									Track 			*usedTrack,
									TimeValue 		atTime,
									TimeValue 		*addedDuration,
									long 			inFlags,
									long 			*outFlags)
{ extern ComponentResult QTImage2MovImport_File(QTImage2MovGlobals,  const FSSpec *, Movie, Track, Track	*, TimeValue, TimeValue *, long, long * );
  int NX = NoXML;
  ComponentResult	r;
	NoXML = 1;
	r = QTImage2MovImport_File(store, theFile, theMovie, targetTrack, usedTrack, atTime, addedDuration, inFlags, outFlags);
	NoXML = NX;
	return r;
}

pascal ComponentResult QTImage2MovImportDirect_DataRef(	QTImage2MovGlobals 	store,
												Handle 					dataRef,
												OSType 					dataRefType,
												Movie 					theMovie,
												Track 					targetTrack,
												Track 					*usedTrack,
												TimeValue 				atTime,
												TimeValue 				*addedDuration,
												long 					inFlags,
												long 					*outFlags
												)
{ extern pascal ComponentResult QTImage2MovImport_DataRef( QTImage2MovGlobals, Handle, OSType, Movie, Track, Track *,
											    TimeValue, TimeValue *, long, long * );
  int NX = NoXML;
  ComponentResult r;

	NoXML = 1;
	r = QTImage2MovImport_DataRef( store, dataRef, dataRefType, theMovie, targetTrack, usedTrack,
							 atTime, addedDuration, inFlags, outFlags	);
	NoXML = NX;
	return r;
}

pascal ComponentResult QTImage2MovImportDirect_GetMIMETypeList(QTImage2MovGlobals store, QTAtomContainer *mimeInfo)
{ extern pascal ComponentResult QTImage2MovImport_GetMIMETypeList(QTImage2MovGlobals store, QTAtomContainer *mimeInfo);
  int NX = NoXML;
  ComponentResult r;

	NoXML = 1;
	r = QTImage2MovImport_GetMIMETypeList( store, mimeInfo);
	NoXML = NX;
	return r;
}

// ---------------------------------------------------------------------------------
//		• QTImage2MovDirectRegister •
//
// Register our component manually, when running in linked mode
// ---------------------------------------------------------------------------------
#ifndef STAND_ALONE

extern void MyQTImage2MovDirectRegister(void);

void MyQTImage2MovDirectRegister(void)
{ ComponentDescription	theComponentDescription;
  Handle					nameHdl;
	#if !TARGET_API_MAC_CARBON
		ComponentRoutineUPP componentEntryPoint = NewComponentRoutineProc(QTImage2MovImportDirect_ComponentDispatch);
	#else
		ComponentRoutineUPP componentEntryPoint = NewComponentRoutineUPP((ComponentRoutineProcPtr)QTImage2MovImportDirect_ComponentDispatch);
	#endif

	PtrToHand("\pQI2MSource", &nameHdl, 8);
  	theComponentDescription.componentType = MovieImportType;
  	theComponentDescription.componentSubType = FOUR_CHAR_CODE('VOD ');
  	theComponentDescription.componentManufacturer = kAppleManufacturer;
  	theComponentDescription.componentFlags = canMovieImportFiles | canMovieImportInPlace | hasMovieImportMIMEList | canMovieImportDataReferences;
  	theComponentDescription.componentFlagsMask = 0;

	RegisterComponent(&theComponentDescription, componentEntryPoint, 0, nameHdl, 0, 0);
}

#endif
