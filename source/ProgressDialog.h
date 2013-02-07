/*!
	@file		ProgressDialog.h

	Description: Progress dialog code for our thread-safety test application.
	file://localhost/Developer/Documentation/DocSets/com.apple.adc.documentation.AppleSnowLeopard.CoreReference.docset/Contents/Resources/Documents/index.html#samplecode/ThreadsExportMovie/Introduction/Intro.html

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

#ifdef __OBJC__

#import <Cocoa/Cocoa.h>

// timer periods
#define kProgressCheckInterval		0.25


@interface mProgressDialog : NSObject
{
    IBOutlet id	cancelButton;
    IBOutlet id	progressBar;
    IBOutlet id	statusField;
    IBOutlet id	importProgressPanel;

    NSTimer		*_progressTimer;		//!< timer for updating the progress dialog box
	// is this kosher??
    NSAutoreleasePool		*pool;
    BOOL			mouseDownInCButton;
@public
    struct macProgressDialogs	*parent;
}

- (IBAction)cancelClick:(id)sender;
- (void)startProgressTimer;
- (OSErr) updateProgressDialogTimed:(NSTimer *)timer;
- (void)updateProgressDialog;

// getters and setters
- (void) displayIfNeeded;
- (void)setProgressTimer:(NSTimer *)theTimer;
- (void) setMaxRange:(double)maxValue;
- (void) setDoubleValue:(double)value;
- (void) setToolTip:(NSString*)string;
- (void) setMouseDownInCButton:(BOOL)state;
- (id)statusField;
- (NSWindow *)progressPanel;
- (NSRect)cancelButtonRect;
- (BOOL) isMouseDownInCButton;
@end

#endif

/*!
	Mac OS X implementation of the ProgressDialogs class
	encapsulates a Cocoa/Obj-C object, mProgressDialog
 */
typedef struct macProgressDialogs {
#ifdef __OBJC__
	mProgressDialog *dialog;
#else
	struct mProgressDialog *dialog;
#endif
	int			cancelled;
	double		maxValue, currentValue;
} ProgressDialogs;

extern OSErr movieMessagePump( ProgressDialogs *dlg );
extern ProgressDialogs *CreateProgressDialog(const char *title);
ProgressDialogs *CloseProgressDialog(ProgressDialogs *PD);
void SetProgressDialogMaxValDiv100(ProgressDialogs *PD, double maxVal);
void SetProgressDialogValue(ProgressDialogs *PD, double percentage, int immediate);
int SetProgressDialogStopRequested(ProgressDialogs *PD);
int PostYesNoDialog( const char *message, const char *title );
//void *qtLogName();
