/*
	File:		dllmain.c
	
	Description: Standard Windows DLL main routine

	Author:		QuickTime team

	Copyright: 	© Copyright 1999 Apple Computer, Inc. All rights reserved.
	
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

*/

#include <QTML.h>
#include <Windows.h>
#include <Commctrl.h>

#include "Dllmain.h"

static HINSTANCE ghInst = NULL;
//HMODULE *phModule = NULL;
extern BOOL InitQTMovieWindows(HINSTANCE hInst);

//void UnloadOurSelves()
//{
//	// this works like a charm ... too well, as it quits the programme that called us too...
//	if( phModule ){
//		FreeLibraryAndExitThread( phModule, 0 );
//	}
//}

/* ----------------------------------------------------------- */

BOOL WINAPI DllMain(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{ INITCOMMONCONTROLSEX lpInitCtrls;
  BOOL cc;
//  BOOL mm;

	if( ghInst != hInst ){
		lpInitCtrls.dwSize = sizeof(lpInitCtrls);
		lpInitCtrls.dwICC = ICC_PROGRESS_CLASS;
		cc = InitCommonControlsEx(&lpInitCtrls);
	}

	ghInst = (HINSTANCE)hInst;
//	mm = GetModuleHandleEx( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
//			&qtLog_Initialised, &phModule
//	);

// 20110322 : there should be no output to the SS_Log window from here... weird side-effects can result from that
// (serious process deadlocks)

	switch(ul_reason_for_call) {
		case DLL_PROCESS_ATTACH:
			break;

		case DLL_THREAD_ATTACH:
			break;

		case DLL_THREAD_DETACH:
			break;

		case DLL_PROCESS_DETACH:
			break;
    }

	return TRUE;
}

/* ----------------------------------------------------------- */

#include "winixdefs.h"
#include "Logging.h"

static char errbuf[512];

typedef struct winProgressDialogs {
	HWND bar, bartip;
	HWND stop, stoptip;
	TOOLINFO barInfo;
	short pushed;
	double maxValue;
	char *title;
} ProgressDialogs;

// a minimal message pump which can be called regularly to make sure MSWindows event messages
// get handled.
unsigned int PumpMessages()
{ MSG msg;
  unsigned int n = 0;
	if( *errbuf && qtLogPtr ){
		Log( qtLogPtr, errbuf );
		errbuf[0] = '\0';
	}
	while( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) ){
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		n += 1;
	}
	if( *errbuf && qtLogPtr ){
		Log( qtLogPtr, errbuf );
		errbuf[0] = '\0';
	}
	return n;
}

ProgressDialogs *CreateProgressDialog(const char *title)
{ ProgressDialogs *PD;
	if( (PD = (ProgressDialogs*) calloc(1, sizeof(ProgressDialogs))) ){
		PD->bar = CreateWindowEx(WS_EX_COMPOSITED, PROGRESS_CLASS, (LPCSTR) title, WS_CAPTION|WS_VISIBLE,
				1, 1, 320, 50, 0, 0, ghInst, NULL
		);
		PD->stop = CreateWindowEx(0, WC_BUTTON, (LPCTSTR) "X", WS_CHILD|WS_VISIBLE,
				295, 6, 22, 15, PD->bar, 0, ghInst, NULL
		);
		if( PD->stop ){
		  TOOLINFO toolInfo = { 0 };
			PD->stoptip = CreateWindowEx(NULL, TOOLTIPS_CLASS, NULL,
					WS_POPUP |TTS_ALWAYSTIP | TTS_BALLOON,
					CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
					PD->stop, 0, ghInst, NULL
			);
			toolInfo.cbSize = sizeof(toolInfo);
			toolInfo.hwnd = PD->stop;
			toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
			toolInfo.uId = (UINT_PTR) PD->stop;
#ifdef LOG_FRANCAIS
			toolInfo.lpszText = "Interrompt l'importation";
#else
			toolInfo.lpszText = "Interrupts the import";
#endif
			SendMessage( PD->stoptip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo );
		}
		if( PD->bar ){
			BringWindowToTop( PD->bar );
			SendMessage( PD->bar, PBM_SETRANGE, 0, MAKELPARAM(0,10000) );
			SetProgressDialogValue(PD, 0.0, 1 );
			PD->title = strdup(title);
			PD->bartip = CreateWindowEx(NULL, TOOLTIPS_CLASS, NULL,
					WS_POPUP |TTS_ALWAYSTIP | TTS_BALLOON,
					CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
					PD->bar, 0, ghInst, NULL
			);
			memset( &PD->barInfo, 0, sizeof(PD->barInfo) );
			PD->barInfo.cbSize = sizeof(PD->barInfo);
			PD->barInfo.hwnd = PD->bar;
			PD->barInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
			PD->barInfo.uId = (UINT_PTR) PD->bar;
			PumpMessages();
		}
		else{
			FormatMessage( FORMAT_MESSAGE_IGNORE_INSERTS|FORMAT_MESSAGE_FROM_SYSTEM,
				NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), errbuf, sizeof(errbuf), NULL
			);
			Log( qtLogPtr, errbuf );
			errbuf[0] = '\0';
			PD = CloseProgressDialog(PD);
		}
	}
	return PD;
}

ProgressDialogs *CloseProgressDialog(ProgressDialogs *PD)
{
	if( PD ){
		PumpMessages();
		if( PD->bar ){
			DestroyWindow(PD->bar);
			PD->bar = NULL;
		}
		if( PD->stop ){
			DestroyWindow(PD->stop);
			PD->stop = NULL;
		}
		if( PD->bartip ){
			DestroyWindow(PD->bartip);
			PD->bartip = NULL;
		}
		if( PD->stoptip ){
			DestroyWindow(PD->stoptip);
			PD->stoptip = NULL;
		}
		if( PD->title ){
			free(PD->title);
			PD->title = NULL;
		}
		free(PD);
		PD = NULL;
		PumpMessages();
	}
	return PD;
}

void SetProgressDialogMaxValDiv100(ProgressDialogs *PD, double maxVal)
{
	if( PD && PD->bar ){
		PD->maxValue = maxVal * 100.0;
		SendMessage( PD->bar, PBM_SETRANGE, 0, MAKELPARAM(0, PD->maxValue ) );
	}
}

void SetProgressDialogValue(ProgressDialogs *PD, double percentage, int immediate)
{
	if( PD && PD->bar ){
		percentage *= 100;
		SendMessage( PD->bar, PBM_SETPOS, (WPARAM) percentage, 0 );
//		if( percentage ){
//			Log( qtLogPtr, "Progress: %g%%", 100.0 * percentage / PD->maxValue );
//		}
		if( PD->bartip ){
#	ifdef LOG_FRANCAIS
			snprintf( errbuf, sizeof(errbuf), "ProgrËs: %g%%", 100.0 * percentage / PD->maxValue );
#	else
			snprintf( errbuf, sizeof(errbuf), "Progress: %g%%", 100.0 * percentage / PD->maxValue );
#	endif
			PD->barInfo.lpszText = errbuf;
			SendMessage( PD->bartip, TTM_ADDTOOL, 0, (LPARAM)&PD->barInfo );
			if( immediate ){
				RedrawWindow( PD->bartip, NULL, NULL, RDW_UPDATENOW|RDW_ALLCHILDREN );
			}
#ifndef DEBUG
			errbuf[0] = '\0';
#endif
		}
		if( immediate ){
			RedrawWindow( PD->bar, NULL, NULL, RDW_UPDATENOW|RDW_ALLCHILDREN );
			PumpMessages();
		}
	}
}

void SetProgressDialogState(ProgressDialogs *PD, int state )
{
	if( PD && PD->bar ){
		SendMessage( PD->bar, PBM_SETPOS, (WPARAM) (state), 0 );
		RedrawWindow( PD->bar, NULL, NULL, RDW_UPDATENOW|RDW_ALLCHILDREN );
		PumpMessages();
	}
}

int SetProgressDialogStopRequested(ProgressDialogs *PD)
{ int pushed = noErr, confirmation;
  LRESULT state;
	if( PD && PD->stop ){
		PumpMessages();
		state = SendMessage( PD->stop, BM_GETSTATE, 0, 0 );
		if( !(state & BST_PUSHED) ){
//			Log( qtLogPtr, "Button state=0x%lx, pushed=%d", state, PD->pushed );
			if( PD->pushed && ((state & BST_FOCUS) || (state & BST_HOT)) ){
#ifdef LOG_FRANCAIS
				confirmation = MessageBox( PD->stop, "Interrompre l'importation?",
									 PD->title, MB_APPLMODAL|MB_ICONQUESTION|MB_YESNO
				);
#else
				confirmation = MessageBox( PD->stop, "Interrupt import operation?",
									 PD->title, MB_APPLMODAL|MB_ICONQUESTION|MB_YESNO
				);
#endif
				if( confirmation == IDOK || confirmation == IDYES ){
					Log( qtLogPtr, "Import cancelled by user" );
					pushed = userCanceledErr;
				}
			}
			PD->pushed = 0;
		}
		else{
			PD->pushed = 1;
//			Log( qtLogPtr, "Button state=0x%lx, pushed=%d", state, PD->pushed );
		}
		RedrawWindow( PD->bar, NULL, NULL, RDW_UPDATENOW|RDW_ALLCHILDREN );
	}
	PumpMessages();
	return pushed;
}

int PostYesNoDialog( const char *message, const char *title )
{ int  confirmation;
	PumpMessages();
	confirmation = MessageBox( NULL, message, title, MB_APPLMODAL|MB_ICONQUESTION|MB_YESNO );
	PumpMessages();
	if( confirmation == IDOK || confirmation == IDYES ){
		return 1;
	}
	else{
		return 0;
	}
}

void PostWarningDialog( const char *message, const char *title )
{ int  confirmation;
	PumpMessages();
	confirmation = MessageBox( NULL, message, title, MB_APPLMODAL|MB_ICONEXCLAMATION|MB_OK );
	PumpMessages();
}

char *tmpImportFileName( const char *theURL, const char *ext )
{ extern char *concat( const char*, ... );
  extern const char *basename( const char *url );
  char *FName = NULL;
  char tmpdir[MAX_PATH], tmppath[MAX_PATH];
  DWORD r;
	r = GetTempPath( sizeof(tmpdir), tmpdir );
	if( r > 0 && r <= sizeof(tmpdir) ){
		if( GetTempFileName( tmpdir, TEXT("Q2M"), 0, tmppath ) ){
			// we're not satisfied with the name created, so remove the file
			unlink(tmppath);
			FName = concat( tmppath, ".", basename(theURL), ".", ext, NULL );
		}
	}
	return FName;
}
