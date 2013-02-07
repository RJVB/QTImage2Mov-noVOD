/*!
 *  @file IMGFrameFromJPGS.c
 *  QTImage2Mov
 *
 *  Created by René J.V. Bertin on 20100713.
 *  Copyright 2010 INRETS / RJVB. All rights reserved.
 *
 */

#include "winixdefs.h"
#include "copyright.h"
IDENTIFY("JPGS importer code for Add_imgFrames_From_File");

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "Logging.h"

#include "AddImageFrames.h"
#include "QTils.h"
#include "IMGFrameFromJPGS.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#ifndef NO_FFMPEGLIBS_DEPEND
#	include "ffmpegDemux.h"
#endif

#ifndef NO_FFMPEGLIBS_DEPEND
static int Get_MPG4_Fragment_Frame( MPG4_Fragments *mp4Frag )
{ FFMemoryDemuxer *demux = (FFMemoryDemuxer*) mp4Frag->ffDemuxer;
  int ret, ret2, retSet = 0;
	// check if we have a valid 'next' frame
	if( demux && demux->next.code >= 0 ){
		// valid frame, now update everything as if we just read it from the fragment.
		mp4Frag->current.frameNr += 1;
		demux->current = demux->next;
		ret = demux->current.code;
		retSet = 1;
		if( demux->current.st->codec ){
			if( demux->current.st->codec->codec ){
				mp4Frag->codecName = demux->current.st->codec->codec->long_name;
			}
			mp4Frag->codecTag = demux->current.st->codec->codec_tag;
			mp4Frag->width = demux->current.st->codec->width;
			mp4Frag->height = demux->current.st->codec->height;
		}
		mp4Frag->timeBase = ((double)demux->current.st->time_base.num)/
			((double)demux->current.st->time_base.den);
		mp4Frag->current.offset = demux->current.pkt.pos;
		mp4Frag->current.byteLength = demux->current.pkt.size;
		mp4Frag->current.duration = demux->current.pkt.duration * mp4Frag->timeBase;
		mp4Frag->totalDuration += mp4Frag->current.duration;
#ifdef DEBUG
		fprintf( stderr, "Read frame %hd at position %llu, size %d, %hdx%hd, duration %gs\n",
			   mp4Frag->current.frameNr, mp4Frag->current.offset, mp4Frag->current.byteLength,
			   mp4Frag->width, mp4Frag->height, mp4Frag->current.duration );
#endif
	}
	// preroll, caching the next frame
	if( (ret2 = ffMemoryDemuxerReadNextPacket(demux)) < 0 ){
		// no next frame; signal that we're done with this fragment.
		mp4Frag->done = 1;
	}
	return (retSet)? ret : ret2;
}
#endif

static int Initialise_MPG4_Fragment( FILE *fp, MPG4_Fragments *mp4Frag, long byteLength, FOFFSET joff )
{ size_t r;
  int ret = -1;
	mp4Frag->byteLength = byteLength;
	mp4Frag->fileOffset = joff;
#ifndef NO_FFMPEGLIBS_DEPEND
	mp4Frag->fragmentBuf = av_realloc( mp4Frag->fragmentBuf,
							 mp4Frag->byteLength * sizeof(unsigned char) );
	if( mp4Frag->ffDemuxer ){
		ffFreeMemoryDemuxer( (FFMemoryDemuxer**) &mp4Frag->ffDemuxer);
	}
#else
	mp4Frag->fragmentBuf = (mp4Frag->fragmentBuf) ? realloc( mp4Frag->fragmentBuf,
								    mp4Frag->byteLength * sizeof(unsigned char) )
							    : malloc(mp4Frag->byteLength * sizeof(unsigned char));
#endif
	if( mp4Frag->fragmentBuf ){
		FFSEEK( fp, joff, SEEK_SET );
		r = fread( mp4Frag->fragmentBuf, sizeof(unsigned char), mp4Frag->byteLength, fp );
		FFSEEK( fp, joff, SEEK_SET );
	}
	if( mp4Frag->fragmentBuf
#ifndef NO_FFMPEGLIBS_DEPEND
	   && (mp4Frag->ffDemuxer = (void*) ffCreateMemoryDemuxer( mp4Frag->fragmentBuf, mp4Frag->byteLength ))
#endif
	){
		mp4Frag->done = 0;
		mp4Frag->current.frameNr = 0;
		mp4Frag->totalDuration = 0;
#ifndef NO_FFMPEGLIBS_DEPEND
		// read the 1st frame
		if( (ret = Get_MPG4_Fragment_Frame(mp4Frag) < 0) ){
			// failure, release the fragment resources:
			Finalise_MPG4_Fragment(mp4Frag);
		}
		else{
			// valid fragment
			mp4Frag->valid = 1;
			// the 1st call to Get_MPG4_Fragment_Frame() stored the packet in the 'next' field.
			// the 2nd call will transfer the packet to 'current', update the size, offset, etc. members
			// and cache the next packet. This allows us to present a 'current' frame to our caller
			// with knowledge of whether or not it is the fragment's last frame.
			ret = Get_MPG4_Fragment_Frame(mp4Frag);
		}
#else
		mp4Frag->valid = -1;
		mp4Frag->done = 1;
		// hardcoded values that ought to be correct:
		mp4Frag->totalDuration = 1;
		mp4Frag->width = 720;
		mp4Frag->height = 576;
		ret = 0;
#endif // NO_FFMPEGLIBS_DEPEND
	}
	return ret;
}

#ifndef _BRIGADE_H
static void Finalise_MPG4_Fragment( MPG4_Fragments *mp4Frag )
{
	if( mp4Frag ){
#ifndef NO_FFMPEGLIBS_DEPEND
		if( mp4Frag->ffDemuxer ){
			ffFreeMemoryDemuxer( (FFMemoryDemuxer**) &mp4Frag->ffDemuxer);
		}
		av_freep(&mp4Frag->fragmentBuf);
#else
		free(mp4Frag->fragmentBuf);
		mp4Frag->fragmentBuf = NULL;
#endif // NO_FFMPEGLIBS_DEPEND
	}
}
#endif

#define DEPTH	current.time.minutes
#define YRES	current.time.seconds
#define XRES	current.time.hours

int ReadNextIMGFrame_JPGS( IMG_FILE_IO *jfio, char *theURL, IMG_FILE FP, imgFrames *jF )
{ int ok = 1;
  char parseBuf[512], *pB;
  int parseBufLen = sizeof(parseBuf)/sizeof(char);
  OFFSET imgOffset;
  long imgLen;
  static unsigned long sequenceType;
  unsigned long type;
  JPGSSources *vs = (JPGSSources*) FP;
  FILE *fp = vs->fp;
  MPG4_Fragments *mp4Frag = &vs->mp4Fragment;

#ifndef NO_FFMPEGLIBS_DEPEND
	if( mp4Frag->ffDemuxer ){
		if( Get_MPG4_Fragment_Frame(mp4Frag) >= 0 ){
			jF->imgOffset = mp4Frag->fileOffset + mp4Frag->current.offset;
			jF->imgLen = mp4Frag->current.byteLength;
			jF->imgWidth = mp4Frag->width;
			jF->imgHeight = mp4Frag->height;
			jF->imgDepth = mp4Frag->DEPTH;
			jF->imgXRes = mp4Frag->XRES;
			jF->imgYRes = mp4Frag->YRES;
			jF->imgDuration = mp4Frag->current.duration;
			jF->imgFrames = 1;
			jF->imgType = imgSeqMP4Type;
			ok = 1;
			goto bail;
		}
		else{
			// done with the current MP4 fragment
			mp4Frag->done = 1;
			Finalise_MPG4_Fragment(mp4Frag);
			fprintf( stderr, "Finished MP4 fragment; %hd frames for %gs (-> %gHz)\n",
				   mp4Frag->current.frameNr, mp4Frag->totalDuration,
				   mp4Frag->current.frameNr / mp4Frag->totalDuration );
		}
	}
#endif
	pB = fgets( parseBuf, parseBufLen, fp );
	if( !pB || strncmp( parseBuf, "frame=", 6) ){
		ok = 0;
		goto bail;
	}
	else{
	  long *typePtr = (long*) &parseBuf[6];
		switch( *typePtr ){
			case 'JPEG':
			case 'GEPJ':
				type = imgJPEGType;
				break;
			case 'JPG2':
			case '2GPJ':
				type = imgJPEG2000Type;
				break;
			case '.PNG':
			case 'GNP.':
				type = imgPNGType;
				break;
			case '.GIF':
			case 'FIG.':
				type = imgGIFType;
				break;
			case '.BMP':
			case 'PMB.':
				type = imgBMPType;
				break;
			case 'TIFF':
			case 'FFIT':
				type = imgTIFFType;
				break;
			case '.PDF':
			case 'FDP.':
				type = imgPDFType;
				break;
			case '.SGI':
			case 'IGS.':
			case '.RGB':
			case 'BGR.':
				type = imgSGIType;
				break;
			case '.RAW':
			case 'WAR.':
				type = imgRAWType;
				break;
			case '.MP4':
			case '4PM.':
				type = imgSeqMP4Type;
				break;
			default:
				ok = 0;
				goto bail;
				break;
		}
	}

	if( jF->imgID == -1 ){
	  char buf[512];
	  struct stat sb;
	  extern char *OSTStr(unsigned long);
		// the image type for this sequence is determined by the 1st frame:
		sequenceType = type;
		snprintf( buf, sizeof(buf), "QTImage2Movie '%.4s' jpgs image sequence\n", OSTStr(sequenceType) );
		AddMetaDataStringToCurrentTrack( jF->destFile, akFormat, buf, NULL );

#ifdef _MSC_VER
		fstat( _fileno(fp), &sb );
#else
		fstat( fileno(fp), &sb );
#endif
		snprintf( buf, sizeof(buf), "modif. %s", ctime( &sb.st_mtime ) );
		AddMetaDataStringToCurrentTrack( jF->destFile, akYear, buf, NULL );
		AddMetaDataStringToCurrentTrack( jF->destFile, akCreationDate, buf, NULL );
		// determine file size via FFSEEK, as fstat() probably will return the size in a 32bit field!
		{ OFFSET here = FFTELL(fp);
			FSEEK(fp, 0, SEEK_END);
			jfio->amountToRead = FTELL(fp);
			FSEEK(fp, here, SEEK_SET);
		}
	}
	else if( type != sequenceType ){
		ok = 0;
		goto bail;
	}

	jF->imgType = sequenceType;
	jF->imgFrames = 1;

	if( !(pB = fgets( parseBuf, parseBufLen, fp ))
	    || sscanf( parseBuf, "rect=%hux%hux%hu@%hux%hu",
				&jF->imgWidth, &jF->imgHeight, &jF->imgDepth,
				&jF->imgXRes, &jF->imgYRes
			) != 5
	){
		ok = 0;
		goto bail;
	}
	if( !(pB = fgets( parseBuf, parseBufLen, fp ))
	    || sscanf( parseBuf, "size=%ld", &jF->imgLen ) != 1
	){
		ok = 0;
		goto bail;
	}
	imgLen = jF->imgLen;
	if( !(pB = fgets( parseBuf, parseBufLen, fp ))
	    || sscanf( parseBuf, "time=%lf", &jF->imgDuration ) != 1
	){
		ok = 0;
		goto bail;
	}
	if( (imgOffset = FTELL(fp)) ){
		jF->imgOffset = imgOffset;
	}
	else{
		ok = 0;
	}
	if( type == imgSeqMP4Type ){
#pragma mark ---do Initialise_MPG4_Fragment
		if( Initialise_MPG4_Fragment( fp, mp4Frag, jF->imgLen, imgOffset ) >= 0 ){
#ifndef NO_FFMPEGLIBS_DEPEND
			jF->imgOffset = mp4Frag->fileOffset + mp4Frag->current.offset;
			jF->imgLen = mp4Frag->current.byteLength;
			jF->imgDuration = mp4Frag->current.duration;
			jF->imgWidth = mp4Frag->width;
			jF->imgHeight = mp4Frag->height;
#else
			mp4Frag->width = jF->imgWidth;
			mp4Frag->height = jF->imgHeight;
			mp4Frag->totalDuration = jF->imgDuration;
			jF->fragmentBuf = mp4Frag->fragmentBuf;
			jF->fragmentLength = mp4Frag->byteLength;
#endif
			mp4Frag->DEPTH = jF->imgDepth;
			mp4Frag->XRES = jF->imgXRes;
			mp4Frag->YRES = jF->imgYRes;
			FSEEK( fp, imgOffset, SEEK_SET );
		}
	}
	else{
		mp4Frag->valid = 0;
	}
	// spool to the next image. the frame=JPEG field starts at a new line, so we have
	// to skip the additional byte corresponding to the newline character.
	if( FSEEK( fp, (imgLen + 1), SEEK_CUR ) ){
		Log( qtLogPtr, "ReadNextIMGFrame() failed to seek on \"%s\" (%s)\n",
			theURL, strerror(errno)
		);
		ok = 0;
	}
bail:
	jfio->fractionRead = ((double)FTELL(fp)) / jfio->amountToRead;
		if( ok ){
			jF->imgID += 1;
		}
	return ok;
}

IMG_FILE _open_JPGSFile( const char *theURL, const char *mode, IMG_FILE_IO *jfio )
{ JPGSSources *vs = (jfio)? (JPGSSources*) jfio->sourcePtr : NULL;
  FILE *fp = (vs)? fopen( theURL, "rb" ) : NULL;
	if( fp && vs ){
		vs->fp = fp;
		jfio->sourceType = 'JPGS';
		CLEAR_MPG4_FRAGMENT(&vs->mp4Fragment);
	}
	else{
		vs = NULL;
	}
	return vs;
}

int _close_JPGSFile( IMG_FILE VS, IMG_FILE_IO *jfio )
{ int r = -1;
  JPGSSources *vs = (JPGSSources*) VS;
	if( vs ){
		Finalise_MPG4_Fragment(&vs->mp4Fragment);
		r = fclose(vs->fp);
		vs->fp = NULL;
	}
	return r;
}

int _rewind_JPGSFile( IMG_FILE VS )
{ int r = -1;
  JPGSSources *vs = (JPGSSources*) VS;
	if( vs ){
		r = FFSEEK( vs->fp, 0, SEEK_SET );
	}
	return r;
}

int _eof_JPGSFile( IMG_FILE vs )
{
	return (vs && ((JPGSSources*) vs)->fp)? feof( ((JPGSSources*) vs)->fp ) : 0;
}

int _error_JPGSFile( IMG_FILE vs )
{
	if( vs && ((JPGSSources*) vs)->fp ){
		return ferror( ((JPGSSources*) vs)->fp );
	}
	else{
		return 1;
	}
}
