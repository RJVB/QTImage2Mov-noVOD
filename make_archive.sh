#!/bin/sh

PATH=/usr/local/bin:${PATH} ; export PATH

THISHOST="`uname`"
case $THISHOST in
	Linux|Darwin)
		;;
	*)
		THISHOST="`uname -m`"
		;;
esac

case $THISHOST in 
	Darwin|"Power Macintosh"|Linux)
		  # this is similar to tcsh's 'symlink chase' option: always use the physical path, not the path as given
		  # by any symlinks in it.
		set -P
		;;
esac

cd "`dirname $0`"/..

mkdir -p $HOME/work/Archive

ECHO="/usr/local/bin/echo"

CleanUp(){
	${ECHO} "Removing temp copies"
	rm -rf $HOME/work/Archive/QTImage2Mov $HOME/work/Archive/brigade $HOME/work/Archive/QTImage2Mov.tar $HOME/work/Archive/brigade.tar $HOME/work/Archive/make_archive.tar &
	exit 2
}

trap CleanUp 1
trap CleanUp 2
trap CleanUp 15

CP="cp"

OS="`uname`"
gcp --help 2>&1 | fgrep -- --no-dereference > /dev/null
if [ $? = 0 ] ;then
	 # hack... gcp must be gnu cp ...
	OS="LINUX"
	CP="gcp"
fi

${CP} --help 2>&1 | fgrep -- --no-dereference > /dev/null
if [ $? = 0 -o "$OS" = "Linux" -o "$OS" = "linux" -o "$OS" = "LINUX" ] ;then
	${ECHO} -n "Making temp copies..."
	${CP} -prd QTImage2Mov brigade GNUStep-PC-LogController $HOME/work/Archive/
	${ECHO} " done."
	cd $HOME/work/Archive/
else
	${ECHO} -n "Making temp copies (tar to preserve symb. links).."
	gnutar -cf $HOME/work/Archive/make_archive.tar QTImage2Mov brigade GNUStep-PC-LogController
	sleep 1
	${ECHO} "(untar).."
	cd $HOME/work/Archive/
	gnutar -xf make_archive.tar
	rm make_archive.tar
fi

sleep 1
${ECHO} "Cleaning out the backup copy"
( gunzip -vf QTImage2Mov/*.gz ;\
  bunzip2 -v QTImage2Mov/*.bz2 ;\
  gunzip -vf brigade/*.gz ;\
  bunzip2 -v brigade/*.bz2 ;\
) 2>&1
rm -rf QTImage2Mov/QTImage2MovPB/build QTImage2Mov/{Release,Develop,Debug} QTImage2Mov/*.all_data QTImage2Mov/.hg QTImage2Mov/*.{log,tc,mov,VOD,avi,mp4,aup,caf,png,rgb,tif,ncb,user,docset}
rm -rf brigade/build brigade/{Debug,Develop,Release} brigade/QT brigade/.hg brigade/*.{tab,log,tc,mov,VOD,avi,mp4,aup,caf,png,rgb,tif} brigade/FFmpeg/*/lib brigade/FFmpeg/win32/*.exe
rm -rf GNUStep-PC-LogController/build GNUStep-PC-LogController/QT GNUStep-PC-LogController/.hg GNUStep-PC-LogController/*.{tab,log,tc,mov,VOD,avi,mp4,aup,caf,png,rgb,tif}

trap "" 1
trap "" 2
trap "" 15

sleep 1
${ECHO} "Archiving the cleaned backup directory"
gnutar -cvvf QTImage2Mov.tar QTImage2Mov
rm -rf QTImage2Mov
gnutar -cf brigade.tar brigade
rm -rf brigade
gnutar -cf GNUStep-PC-LogController.tar GNUStep-PC-LogController
rm -rf GNUStep-PC-LogController
ls -lU QTImage2Mov.tar.bz2 brigade.tar.bz2 GNUStep-PC-LogController.tar.bz2
bzip2 -vf QTImage2Mov.tar brigade.tar GNUStep-PC-LogController.tar

trap CleanUp 1
trap CleanUp 2
trap CleanUp 15
${ECHO} "Cleaning up"

open .

trap 1
trap 2
trap 15

${ECHO} "Making remote backups - put your commands here"
