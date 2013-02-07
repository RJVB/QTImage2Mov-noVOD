/*
     File:       QuickTime/QuickTimeComponents.h

     Contains:   QuickTime Interfaces.

     Copyright:  © 1990-2001 by Apple Computer, Inc., all rights reserved

     Warning:    *** APPLE INTERNAL USE ONLY ***
                 This file may contain unreleased API's

     BuildInfo:  Built by:            dkudo
                 On:                  Tue Mar 19 12:53:14 2002
                 With Interfacer:     3.0d35   (Mac OS X for PowerPC)
                 From:                QuickTimeComponents.i
                     Revision:        1.52
                     Dated:           2001/04/26 21:20:37
                     Last change by:  kledzik
                     Last comment:    Switch to cvs style header comment

     Bugs:       Report bugs to Radar component "System Interfaces", "Latest"
                 List the version information (from above) in the Problem Description.

*/
// RJVB 20111130: I suppose we could compile for Mac using a non-Apple compiler...
#ifdef __APPLE_CC__ && !(defined(_MSC_VER) || defined(WIN32) || defined(_WINDOWS))
#	include "QuickTimeComponents_macosx.h"
#else
#	include "QuickTimeComponents_win32.h"
#endif
