/*!
	@file		ProgressDialog.m

	Description: Progress dialog code for our thread-safety test application.

	Author:		QuickTime Engineering, RJVB

	Copyright: 	© Copyright 2003-2004 Apple Computer, Inc. All rights reserved.
				2011 IFSTTAR
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

	Change History (most recent first):  <1> dts 01/28/04 initial release
*/

#import "copyright.h"
IDENTIFY("a Cocoa progress dialog in Objective-C" );

#import "ProgressDialog.h"
#import "AppKit/NSAlert.h"
#import "PCLogController.h"

@implementation mProgressDialog

- (id) init:(ProgressDialogs*)PD pool:(NSAutoreleasePool*)pll title:(NSString*)str
{
	[super init];
	if( self ){
		// load up the nib
		if( !importProgressPanel ){
			[NSBundle loadNibNamed:@"ProgressDialog" owner:self];
		}
		[progressBar setUsesThreadedAnimation:YES];
		[progressBar setMinValue:0.0];
		[progressBar setDoubleValue:0.0];
		[statusField setStringValue:str];
//		[importProgressPanel setTitle:str];
#ifdef LOG_FRANCAIS
		[progressBar setToolTip:@"Pas encore démarré"];
		[cancelButton setToolTip:@"Interrompt l'importation"];
		[statusField setToolTip:@"Nom du fichier en cours d'importation"];
#else
		[progressBar setToolTip:@"Not started yet"];
		[cancelButton setToolTip:@"Interrupts the import"];
		[statusField setToolTip:@"Filename being imported"];
#endif
//		[importProgressPanel setDelegate:self];
		parent = PD;
		pool = pll;
	}

	return self;
}

- (void)windowDidLoad:(NSNotification *)dummy
{
	NSLog( @"[%@ windowDidLoad]", self );
	[importProgressPanel setExcludedFromWindowsMenu:YES];
}

- (IBAction) cancelClick:(id)sender
{
	// user cancelled
	if( parent ){
		parent->cancelled = YES;
	}
	NSLog( @"[%@ %@%@]\n", NSStringFromClass([self class]), NSStringFromSelector(_cmd), sender );

	// stop the timer
	[self setProgressTimer:nil];

//	[importProgressPanel close];
}

- (void) startProgressTimer
{
    [progressBar setDoubleValue:0];
    [self setProgressTimer:[NSTimer
            scheduledTimerWithTimeInterval:kProgressCheckInterval
            target:self
            selector:@selector(updateProgressDialogTimed:)
            userInfo:nil
            repeats:YES]];
}

- (OSErr) updateProgressDialogTimed:(NSTimer *)timer
{ extern OSErr movieProgressProc( ProgressDialogs *dlg, BOOL flush );
	return movieProgressProc(parent, NO);
}

- (void) updateProgressDialog
{
	/* @synchronized(self) */{
		if( [importProgressPanel isExcludedFromWindowsMenu] ){
			[importProgressPanel setExcludedFromWindowsMenu:YES];
		}
		[progressBar displayIfNeeded];
		[importProgressPanel update];
	}
}

- (void) displayIfNeeded
{
	[progressBar displayIfNeeded];
	[statusField displayIfNeeded];
 }

- (void)setProgressTimer:(NSTimer *)theTimer
{
    [_progressTimer invalidate];
    [_progressTimer release];
    [theTimer retain];
    _progressTimer = theTimer;
}

- (void) setMaxRange:(double)maxValue
{
	[progressBar setMaxValue:maxValue];
}

- (void) setDoubleValue:(double)value
{
	[progressBar setDoubleValue:value];
}

- (void) setToolTip:(NSString*)string
{
	[progressBar setToolTip:string];
}

- (void) setMouseDownInCButton:(BOOL)state
{
	if( mouseDownInCButton != state ){
		if( state ){
			[cancelButton highlight:YES];
			[cancelButton setState:NSOnState];
		}
		else{
			[cancelButton highlight:NO];
			[cancelButton setState:NSOffState];
		}
		mouseDownInCButton = state;
		[cancelButton displayIfNeeded];
	}
}

- (id)statusField
{
    return statusField;
}

- (NSWindow *)progressPanel
{
    return importProgressPanel;
}

- (NSRect)cancelButtonRect
{
    return [cancelButton frame];
}

- (BOOL) isMouseDownInCButton
{
	return mouseDownInCButton;
}

- (NSAutoreleasePool*) autoReleasePool
{
	return pool;
}

@end

OSErr movieProgressProc( ProgressDialogs *dlg, BOOL flush )
{ NSWindow *progressWindow;				// the progress dialog sheet
  unsigned int type;
  NSAutoreleasePool *pool;
  NSEvent *event = nil;
  NSWindow *window;

	if( dlg == nil ){
		return paramErr;
	}

	pool = [[NSAutoreleasePool alloc] init];
	progressWindow = [dlg->dialog progressPanel];

	// if we're on the main thread, look for clicks on the Cancel button in the progress dialog box
	do{

		event = [NSApp /*progressWindow*/ nextEventMatchingMask:NSAnyEventMask/*NSLeftMouseUpMask*/
					untilDate:[NSDate distantPast]
					inMode:NSDefaultRunLoopMode
					dequeue:YES
		];

		if( event ){
			window = [event window];
			if( window == progressWindow ){
			  NSRect cancelButtonRect = [dlg->dialog cancelButtonRect];
				type = [event type];
	//			NSLog( @"Event: %@ type=%u", event, type );
				switch( type ){
					case NSLeftMouseDown:
						/* @synchronized(dlg->dialog) */{
							[dlg->dialog
								setMouseDownInCButton:NSPointInRect([event locationInWindow], cancelButtonRect)
							];
						}
						break;
					case NSLeftMouseUp:
						// NOTE: we really ought to track the mouse from mouse-down to mouse-up; here we just look for
						// mouse-up events in side the Cancel button....you could do better!
						/* @synchronized(dlg->dialog) */{
							if( [dlg->dialog isMouseDownInCButton]
							   && NSPointInRect([event locationInWindow], cancelButtonRect)
							){
								dlg->cancelled = YES;
							}
							[dlg->dialog setMouseDownInCButton:NO];
						}
						break;
					default:
	//					[dlg->dialog
	//						setMouseDownInCButton:[dlg->dialog isMouseDownInCButton]
	//					];
						[NSApp sendEvent:event];
						break;
				}
			}
			else{
				[NSApp sendEvent:event];
			}
		}
	} while( flush && event != nil && !dlg->cancelled );

	[dlg->dialog updateProgressDialog];
	[pool drain];

	if( dlg->cancelled ){
		return userCanceledErr;
	}
	else{
		return noErr;
	}
}

ProgressDialogs *CreateProgressDialog(const char *title)
{ ProgressDialogs *PD;
  NSAutoreleasePool *pool = nil;
	if( (PD = (ProgressDialogs*) calloc(1, sizeof(ProgressDialogs))) ){
//		pool = [[NSAutoreleasePool alloc] init];
		PD->dialog = [[mProgressDialog alloc] init:PD
							    // is this kosher??
								pool:pool
								title:[NSString stringWithCString:title
									  encoding:NSUTF8StringEncoding]
		];
		if( PD->dialog ){
//			[NSApp beginSheet:[PD->dialog progressPanel]
//				  modalForWindow:nil
//				  modalDelegate:nil
//				  didEndSelector:nil
//				  contextInfo:nil
//			];
			[PD->dialog startProgressTimer];
//			[[PD->dialog progressPanel] display];
			[[PD->dialog progressPanel] orderFrontRegardless];
		}
	}
	return PD;
}

ProgressDialogs *CloseProgressDialog(ProgressDialogs *PD)
{
	if( PD ){
		/* @synchronized(PD->dialog) */{
			[PD->dialog setProgressTimer:nil];
			PD->dialog->parent = NULL;
//			[[PD->dialog progressPanel] close];
			// clanganalyser says "Incorrect decrement of the reference count of an object that is not owned at this point by the caller"
			// but if we don't do the release, the progress dialog remains open
			[[PD->dialog progressPanel] release];
//			[NSApp endSheet:[PD->dialog progressPanel]];
			// is this kosher?? (Answer: no...)
			if( [PD->dialog progressPanel] ){
				[[PD->dialog autoReleasePool] drain];
			}
			[PD->dialog autorelease];
		}
		free(PD);
		PD = NULL;
	}
	return PD;
}

void SetProgressDialogMaxValDiv100(ProgressDialogs *PD, double maxVal)
{
	if( PD && PD->dialog ){
		PD->maxValue = maxVal * 100.0;
		[PD->dialog setMaxRange:PD->maxValue];
		movieProgressProc(PD,YES);
	}
}

void SetProgressDialogValue(ProgressDialogs *PD, double percentage, int immediate)
{
	if( PD && PD->dialog ){
		PD->currentValue = percentage * 100;
		[PD->dialog setDoubleValue:PD->currentValue];
		if( immediate ){
		  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
		  NSString *here;
#ifdef LOG_FRANCAIS
			here = [NSString stringWithFormat:@"Progrès: %g%%",
						   100.0 * PD->currentValue / PD->maxValue];
#else
			here = [NSString stringWithFormat:@"Progress: %g%%",
						   100.0 * PD->currentValue / PD->maxValue];
#endif
			[PD->dialog setToolTip:here];
			[PD->dialog displayIfNeeded];
//			PCLogStatus( [NSApplication sharedApplication],
//					  @"%@ %s", here, "finished" );
			movieProgressProc(PD,YES);
			[pool drain];
		}
	}
}

int SetProgressDialogStopRequested(ProgressDialogs *PD)
{ int confirmation;
	if( PD && PD->dialog ){
		movieProgressProc(PD,YES);
		if( PD->cancelled ){
#ifdef LOG_FRANCAIS
			confirmation = PostYesNoDialog( "Interrompre l'importation?",
					[[[PD->dialog statusField] stringValue] cStringUsingEncoding:NSUTF8StringEncoding]
			);
#else
			confirmation = PostYesNoDialog( "Interrupt import operation?",
					[[[PD->dialog statusField] stringValue] cStringUsingEncoding:NSUTF8StringEncoding]
			);
#endif
			if( confirmation  ){
				return userCanceledErr;
			}
			else{
				PD->cancelled = NO;
				return noErr;
			}
		}
	}
	return noErr;
}

int PostYesNoDialog( const char *message, const char *title )
{ NSAlert* alert = [NSAlert
			alertWithMessageText:[NSString stringWithCString:title encoding:NSUTF8StringEncoding]
#ifdef LOG_FRANCAIS
			defaultButton:@"Oui" alternateButton:@"Non" otherButton:NULL
#else
			defaultButton:@"Yes" alternateButton:@"No" otherButton:NULL
#endif
			informativeTextWithFormat:[NSString stringWithCString:message encoding:NSUTF8StringEncoding]
		];
	return NSAlertDefaultReturn == [alert runModal];
}

//void *qtLogName()
//{
//	return [NSApplication sharedApplication];
//}
//
//void *nsString(char *s)
//{
//	return [NSString stringWithCString:s encoding:NSUTF8StringEncoding];
//}
