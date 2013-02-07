#ifndef _DLLMAIN_H

unsigned int PumpMessages();

struct winProgressDialogs *CreateProgressDialog(const char *title);
struct winProgressDialogs *CloseProgressDialog(struct winProgressDialogs *PD);
void SetProgressDialogMaxValDiv100(struct winProgressDialogs *PD, double maxVal);
void SetProgressDialogValue(struct winProgressDialogs *PD, double percentage, int immediate);
void SetProgressDialogState(struct winProgressDialogs *PD, int state );
int SetProgressDialogStopRequested(struct winProgressDialogs *PD);

int PostYesNoDialog( const char *message, const char *title );
void PostWarningDialog( const char *message, const char *title );


#	define _DLLMAIN_H
#endif