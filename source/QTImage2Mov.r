/*
	File:		QTImage2Mov.r

	Description: 	Slide Show Importer component routines.
				Support for importing image sequences by RJVB 2007-2010
				Image sequence can be any video QuickTime can open, but also a file
				concatenating a series of jpeg images with describing metadata (.jpgs)
				or (ultimately) a Brigrade Electronics/Approtech .VOB file.

	Author:		Apple Developer Support, original code by Jim Batson of QuickTime engineering
				Extensions by RJVB 2007-2010

	Copyright: 	© Copyright 2001-2002 Apple Computer, Inc. All rights reserved.
				Portions by RJVB © Copyright 2007-2010 RJVB, INRETS

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

		<3>		02/15/02    SRK		adapted for use with SGPanel sample
		<2>	 	11/17/00	ERA		updating for PPC and X
		<1>	 	11/28/99	JSAM	first file
*/




#define SystemSevenOrLater	1

/*
    thng_RezTemplateVersion:
        0 - original 'thng' template    <-- default
        1 - extended 'thng' template	<-- used for multiplatform things
        2 - extended 'thng' template including resource map id
*/

#define thng_RezTemplateVersion 1

/*
    cfrg_RezTemplateVersion:
        0 - original					<-- default
        1 - extended
*/

#define cfrg_RezTemplateVersion 1


#if TARGET_REZ_CARBON_MACHO
    #include <Carbon/Carbon.r>
    #include <QuickTime/QuickTime.r>
	#undef __CARBON_R__
	#undef __CORESERVICES_R__
	#undef __CARBONCORE_R__
	#undef __COMPONENTS_R__
#else
    #include "ConditionalMacros.r"
    #include "MacTypes.r"
    #include "Components.r"
    #include "QuickTimeComponents.r"
    #include "ImageCompression.r"
    #include "CodeFragments.r"
    #include "Dialogs.r"
    #include "Icons.r"
#endif


#include "QTImage2MovVersion.h"
#include "QTImage2MovConstants.h"

#define	kQTImage2MovShortFileTypeNamesRes	'RB'
#define	kQTImage2MovNameRes					'RA'
#define	kQTImage2MovInfoRes					'RB'
#define	kVOD2MovShortFileTypeNamesRes	'Rb'
#define	kVOD2MovNameRes					'Ra'
#define	kVOD2MovInfoRes					'Rb'
#define	kJPGS2MovShortFileTypeNamesRes	'JG'
#define	kJPGS2MovNameRes					'JP'
#define	kJPGS2MovInfoRes					'JG'

// 20111123: added movieImporterIsXMLBased
#if TARGET_REZ_WIN32
// 20111123: added cmpThreadSafe
#	define kQTImage2MovFlags \
	(movieImportSubTypeIsFileExtension | canMovieImportFiles | canMovieImportInPlace | hasMovieImportMIMEList \
	 | canMovieImportDataReferences | movieImporterIsXMLBased | cmpThreadSafe)
#else
#	define kQTImage2MovFlags \
	(movieImportSubTypeIsFileExtension | canMovieImportFiles | canMovieImportInPlace | hasMovieImportMIMEList \
	 | canMovieImportDataReferences | movieImporterIsXMLBased | cmpThreadSafe)
#endif

#define kVOD2MovFlags	kQTImage2MovFlags
#define kJPGS2MovFlags	kQTImage2MovFlags

#define kTheComponentType 			'eat '
#define kTheComponentSubType		'QI2M'
#define kTheComponentManufacturer	'RJVB'


// These flags specify information about the capabilities of the component


// Component Manager Thing 'thng'
resource 'thng' (kQTImage2MovThingRes) {
	kTheComponentType,						// Type
	kTheComponentSubType,					// Subtype
	kTheComponentManufacturer,				// Manufacturer
#if TARGET_REZ_MAC_68K
	kQTImage2MovFlags,					// Component flags
	0,										// Component flags Mask
	kTheComponentType,						// Code Type
	kQTImage2MovThingRes,				// Code ID
#else
	0,										// - use componentHasMultiplePlatforms
	0,
	0,
	0,
#endif
	'strn', 								// Name Type
	kQTImage2MovNameRes,				// Name ID
	'stri',									// Info Type
	kQTImage2MovInfoRes,				// Info ID
	0,										// Icon Type
	0,										// Icon ID
#if TARGET_REZ_MAC_68K || TARGET_REZ_WIN32	// Version
	kQTImage2MovVersion,
#else
	kQTImage2MovVersionPPC,
#endif
	componentHasMultiplePlatforms + 		// Registration flags
	componentDoAutoVersion,
	0,										// Resource ID of Icon Family
	{
#if TARGET_OS_MAC							// COMPONENT PLATFORM INFORMATION ----------------------
		#if TARGET_REZ_CARBON_CFM					// COMPONENT PLATFORM INFORMATION ----------------------
			kQTImage2MovFlags,			// Component Flags
			'cfrg',								// Special Case: data-fork based code fragment
			128,	 								/* Code ID usage for CFM components:
													0 (kCFragResourceID) - This means the first member in the code fragment;
														Should only be used when building a single component per file. When doing so
														using kCFragResourceID simplifies things because a custom 'cfrg' resource is not required
													n - This value must match the special 'cpnt' qualifier 1 in the custom 'cfrg' resource */
			platformPowerPCNativeEntryPoint,	// Platform Type (response from gestaltComponentPlatform or failing that, gestaltSysArchitecture)
		#elif TARGET_REZ_CARBON_MACHO
			#if !(TARGET_REZ_MAC_PPC || TARGET_REZ_MAC_X86)
				#error "Platform architecture not defined, TARGET_REZ_MAC_PPC and/or TARGET_REZ_MAC_X86 must be defined!"
			#endif
			#if TARGET_REZ_MAC_PPC
				kQTImage2MovFlags,
				'dlle',								// Code Resource type - Entry point found by symbol name 'dlle' resource
				kQTImage2MovThingRes,					// ID of 'dlle' resource
				platformPowerPCNativeEntryPoint,			// PPC
			#endif
			#if TARGET_REZ_MAC_X86
				kQTImage2MovFlags,
				'dlle',
				kQTImage2MovThingRes,
				platformIA32NativeEntryPoint,				// INTEL
			#endif
		#elif TARGET_REZ_MAC_PPC
			kQTImage2MovFlags,
			kTheComponentType,									// Code Type
			kQTImage2MovThingRes,						// Code ID
			platformPowerPC,
		#elif TARGET_REZ_MAC_68K
			kQTImage2MovFlags,
			kTheComponentType,
			kQTImage2MovThingRes,
			platform68k,
		#else
			#error "At least one TARGET_REZ_XXX_XXX platform must be defined."
		#endif
#endif
#if TARGET_OS_WIN32
	kQTImage2MovFlags,
	'dlle',
	kQTImage2MovThingRes,
	platformWin32,
#endif
	},
};

resource 'strn' (kQTImage2MovNameRes) {
	"QI2MSource"
};

resource 'stri' (kQTImage2MovInfoRes) {
	"Imports a QI2MSource XML file."
};

// Component Name
resource 'STR ' (kQTImage2MovThingRes) {
	"Images -> Movie Import Component"
};

resource 'STR#' (kQTImage2MovShortFileTypeNamesRes) {
	{
	"QI2M"
	}
};


resource 'thnr' (kQTImage2MovThingRes)
{
	{
	kMimeInfoMimeTypeTag, 1, 0,
	kMimeInfoMimeTypeTag, kQTImage2MovThingRes, 0
	}
};

resource 'mime' (kQTImage2MovThingRes) {
	{
	kMimeInfoMimeTypeTag,      1, "video/x-qt-img2mov";
	kMimeInfoFileExtensionTag, 1, "qi2m";
	kMimeInfoDescriptionTag,   1, "QT Images to Movie";
	};
};

#if TARGET_REZ_CARBON_CFM
// Custom extended code fragment resource
// CodeWarrior will correctly adjust the offset and length of each
// code fragment when building a MacOS Merge target
resource 'cfrg' (0) {
	{
		extendedEntry {
			kPowerPCCFragArch,					// archType
			kIsCompleteCFrag,					// updateLevel
			kNoVersionNum,						// currentVersion
			kNoVersionNum,						// oldDefVersion
			kDefaultStackSize,					// appStackSize
			kNoAppSubFolder,					// appSubFolderID
			kImportLibraryCFrag,				// usage
			kDataForkCFragLocator,				// where
			kZeroOffset,						// offset
			kCFragGoesToEOF,					// length
			"Images -> Movie Import Component",	// member name

			// Start of extended info.

			'cpnt',								// libKind (not kFragComponentMgrComponent == 'comp' as you might expect)
			"\0x00\0x80",						// qualifier 1 - hex 0x0080 (128) matches Code ID in 'thng'
			"",									// qualifier 2
			"",									// qualifier 3
			"Images -> Movie Import Component",	// intlName, localised
		};
	};
};
#endif

#if	TARGET_REZ_CARBON_MACHO || TARGET_REZ_WIN32
// Code Entry Point for Mach-O and Windows
	resource 'dlle' (kQTImage2MovThingRes) {
		"QTImage2MovImport_ComponentDispatch"
	};
#endif

resource 'vers' (128)
	{
	0x01,
	0x00,
	alpha,
	1,
	0,
	"1.0",
	"QI2MSource Importer 1.0"
	};

/////////////////////------ section for opening .VOD files:

// Component Manager Thing 'thng'
resource 'thng' (kVOD2MovThingRes) {
	kTheComponentType,						// Type
	'VOD ',								// Subtype
	kTheComponentManufacturer,				// Manufacturer
#if TARGET_REZ_MAC_68K
	kQTImage2MovFlags,					// Component flags
	0,										// Component flags Mask
	kTheComponentType,						// Code Type
	kVOD2MovThingRes,				// Code ID
#else
	0,										// - use componentHasMultiplePlatforms
	0,
	0,
	0,
#endif
	'strn', 								// Name Type
	kVOD2MovNameRes,				// Name ID
	'stri',									// Info Type
	kVOD2MovInfoRes,				// Info ID
	0,										// Icon Type
	0,										// Icon ID
#if TARGET_REZ_MAC_68K || TARGET_REZ_WIN32	// Version
	kQTImage2MovVersion,
#else
	kQTImage2MovVersionPPC,
#endif
	componentHasMultiplePlatforms + 		// Registration flags
	componentDoAutoVersion,
	0,										// Resource ID of Icon Family
	{
#if TARGET_OS_MAC							// COMPONENT PLATFORM INFORMATION ----------------------
		#if TARGET_REZ_CARBON_CFM					// COMPONENT PLATFORM INFORMATION ----------------------
			kVOD2MovFlags,			// Component Flags
			'cfrg',								// Special Case: data-fork based code fragment
			128,	 								/* Code ID usage for CFM components:
													0 (kCFragResourceID) - This means the first member in the code fragment;
														Should only be used when building a single component per file. When doing so
														using kCFragResourceID simplifies things because a custom 'cfrg' resource is not required
													n - This value must match the special 'cpnt' qualifier 1 in the custom 'cfrg' resource */
			platformPowerPCNativeEntryPoint,	// Platform Type (response from gestaltComponentPlatform or failing that, gestaltSysArchitecture)
		#elif TARGET_REZ_CARBON_MACHO
			#if !(TARGET_REZ_MAC_PPC || TARGET_REZ_MAC_X86)
				#error "Platform architecture not defined, TARGET_REZ_MAC_PPC and/or TARGET_REZ_MAC_X86 must be defined!"
			#endif
			#if TARGET_REZ_MAC_PPC
				kVOD2MovFlags,
				'dlle',								// Code Resource type - Entry point found by symbol name 'dlle' resource
				kVOD2MovThingRes,					// ID of 'dlle' resource
				platformPowerPCNativeEntryPoint,			// PPC
			#endif
			#if TARGET_REZ_MAC_X86
				kVOD2MovFlags,
				'dlle',
				kVOD2MovThingRes,
				platformIA32NativeEntryPoint,				// INTEL
			#endif
		#elif TARGET_REZ_MAC_PPC
			kVOD2MovFlags,
			kTheComponentType,									// Code Type
			kVOD2MovThingRes,						// Code ID
			platformPowerPC,
		#elif TARGET_REZ_MAC_68K
			kVOD2MovFlags,
			kTheComponentType,
			kVOD2MovThingRes,
			platform68k,
		#else
			#error "At least one TARGET_REZ_XXX_XXX platform must be defined."
		#endif
#endif
#if TARGET_OS_WIN32
	kVOD2MovFlags,
	'dlle',
	kVOD2MovThingRes,
	platformWin32,
#endif
	},
};

resource 'strn' (kVOD2MovNameRes) {
	"VOD file"
};

resource 'stri' (kVOD2MovInfoRes) {
	"Imports a Brigade VOD file."
};

// Component Name
resource 'STR ' (kVOD2MovThingRes) {
	"VOD -> Movie Import Component"
};

resource 'STR#' (kVOD2MovShortFileTypeNamesRes) {
	{
	"VOD"
	}
};

resource 'thnr' (kVOD2MovThingRes)
{
	{
	kMimeInfoMimeTypeTag, 1, 0,
	kMimeInfoMimeTypeTag, kVOD2MovThingRes, 0
	}
};

resource 'mime' (kVOD2MovThingRes) {
	{
		kMimeInfoMimeTypeTag,      1, "video/brigade-vod";
		kMimeInfoFileExtensionTag, 1, "vod ";
		kMimeInfoDescriptionTag,   1, "Brigade VOD video";
	};
};

#if	TARGET_REZ_CARBON_MACHO || TARGET_REZ_WIN32
// Code Entry Point for Mach-O and Windows
	resource 'dlle' (kVOD2MovThingRes) {
		"QTImage2MovImportDirect_ComponentDispatch"
	};
#endif

//------ section for opening .JPGS files:

// Component Manager Thing 'thng'
resource 'thng' (kJPGS2MovThingRes) {
	kTheComponentType,						// Type
	'JPGS',								// Subtype
	kTheComponentManufacturer,				// Manufacturer
#if TARGET_REZ_MAC_68K
	kQTImage2MovFlags,					// Component flags
	0,										// Component flags Mask
	kTheComponentType,						// Code Type
	kJPGS2MovThingRes,				// Code ID
#else
	0,										// - use componentHasMultiplePlatforms
	0,
	0,
	0,
#endif
	'strn', 								// Name Type
	kJPGS2MovNameRes,				// Name ID
	'stri',									// Info Type
	kJPGS2MovInfoRes,				// Info ID
	0,										// Icon Type
	0,										// Icon ID
#if TARGET_REZ_MAC_68K || TARGET_REZ_WIN32	// Version
	kQTImage2MovVersion,
#else
	kQTImage2MovVersionPPC,
#endif
	componentHasMultiplePlatforms + 		// Registration flags
	componentDoAutoVersion,
	0,										// Resource ID of Icon Family
	{
	#if TARGET_OS_MAC							// COMPONENT PLATFORM INFORMATION ----------------------
		#if TARGET_REZ_CARBON_CFM					// COMPONENT PLATFORM INFORMATION ----------------------
			kJPGS2MovFlags,			// Component Flags
			'cfrg',								// Special Case: data-fork based code fragment
			128,	 								/* Code ID usage for CFM components:
			0 (kCFragResourceID) - This means the first member in the code fragment;
			Should only be used when building a single component per file. When doing so
			using kCFragResourceID simplifies things because a custom 'cfrg' resource is not required
			n - This value must match the special 'cpnt' qualifier 1 in the custom 'cfrg' resource */
			platformPowerPCNativeEntryPoint,	// Platform Type (response from gestaltComponentPlatform or failing that, gestaltSysArchitecture)
		#elif TARGET_REZ_CARBON_MACHO
			#if !(TARGET_REZ_MAC_PPC || TARGET_REZ_MAC_X86)
				#error "Platform architecture not defined, TARGET_REZ_MAC_PPC and/or TARGET_REZ_MAC_X86 must be defined!"
			#endif
			#if TARGET_REZ_MAC_PPC
				kJPGS2MovFlags,
				'dlle',								// Code Resource type - Entry point found by symbol name 'dlle' resource
				kJPGS2MovThingRes,					// ID of 'dlle' resource
				platformPowerPCNativeEntryPoint,			// PPC
			#endif
			#if TARGET_REZ_MAC_X86
				kJPGS2MovFlags,
				'dlle',
				kJPGS2MovThingRes,
				platformIA32NativeEntryPoint,				// INTEL
			#endif
		#elif TARGET_REZ_MAC_PPC
			kJPGS2MovFlags,
			kTheComponentType,									// Code Type
			kJPGS2MovThingRes,						// Code ID
			platformPowerPC,
		#elif TARGET_REZ_MAC_68K
			kJPGS2MovFlags,
			kTheComponentType,
			kJPGS2MovThingRes,
			platform68k,
		#else
			#error "At least one TARGET_REZ_XXX_XXX platform must be defined."
		#endif
	#endif
	#if TARGET_OS_WIN32
		kJPGS2MovFlags,
		'dlle',
		kJPGS2MovThingRes,
		platformWin32,
	#endif
	},
};

resource 'strn' (kJPGS2MovNameRes) {
	"JPGS file"
};

resource 'stri' (kJPGS2MovInfoRes) {
	"Imports a 'JPGS' image sequence file."
};

// Component Name
resource 'STR ' (kJPGS2MovThingRes) {
	"JPGS -> Movie Import Component"
};

resource 'STR#' (kJPGS2MovShortFileTypeNamesRes) {
	{
		"JPGS"
	}
};


resource 'thnr' (kJPGS2MovThingRes)
{
{
	kMimeInfoMimeTypeTag, 1, 0,
	kMimeInfoMimeTypeTag, kJPGS2MovThingRes, 0
}
};

resource 'mime' (kJPGS2MovThingRes) {
{
	kMimeInfoMimeTypeTag,      1, "video/jpgs-image-sequence";
	kMimeInfoFileExtensionTag, 1, "jpgs";
	kMimeInfoDescriptionTag,   1, "JPGS image sequence";
};
};

#if	TARGET_REZ_CARBON_MACHO || TARGET_REZ_WIN32
// Code Entry Point for Mach-O and Windows
resource 'dlle' (kJPGS2MovThingRes) {
	"QTImage2MovImportDirect_ComponentDispatch"
};
#endif


