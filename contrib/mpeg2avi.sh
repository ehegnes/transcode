#!/bin/bash
#-------------------------------------------------------------------------------------------------------------
# command:	/usr/local/bin/mpeg2avi.sh mpg|vob 16:9|4:3
# author:	Andreas Reichel
# email: 	andreas.reichel@i-kit.de
# url:		http://www.i-kit.de
#-------------------------------------------------------------------------------------------------------------
#
# simple bash-script to transcode multiple mpeg2 files to cd-sized avi-files in mpeg4-format
# use it, to convert videos recorded from dvb-cards
#
#-------------------------------------------------------------------------------------------------------------
# simple howto:
#
# 1) record the movie using vdr [1] and the linux-dvb-drivers [2]
# 2) cut the movie using vdr [1]
# 3) convert the pva-pes formatted movie-files to ps-formated mpegII-files using pvastrumento [3] and wine [4]
# 4) run this skript in directory of movie-files to transcode [5]
# 5) optional: burn the avi-files to cdrom
#-------------------------------------------------------------------------------------------------------------
# sources and references:
# [1] http://www.cadsoft.de/people/kls/vdr/download.htm
# [2] http://www.linuxdvb.tv/download/
# [3] http://www.offeryn.de/dv.htm
# [4] http://www.winehq.org/download.shtml
# [5] http://www.theorie.physik.uni-goettingen.de/~ostreich/transcode/#download
#-------------------------------------------------------------------------------------------------------------

EXT=${1}
NICELEVEL=10				# run it in background
AF=""

OUT_MCODEC="xvid"			# xvid|divx|opendivx
MBITRATE="1400,1000,100" 		# bitrate, key-frames, crispness
RES169="640x360"
RES43="640x480"

ABITRATE="128"				# vbr using lame seems to bee broken
CDSIZE="700"				# use 700mb splitting

case "${EXT}" in
	mpg) 	CODEC="mpeg2"
		;;
	vob)	CODEC="vob"
		;;
	*)	echo "Usage: /usr/local/bin/mpeg2avi.sh mpg|vob 16:9|4:3"
		exit 1
esac
case "${2}" in
	"16:9")	Y=80
		RES=${RES169}
		;;
	"4:3")	Y=0
		RES=${RES43}
		;;
	*)	echo "Usage: /usr/local/bin/mpeg2avi.sh mpg|vob 16:9|4:3"
		exit 1
		;;
esac


for F in `ls *.${EXT}`
do
	FN=`basename ${F} .${EXT}`

	#first loop, exit on error
	NORMALIZE=`nice -n ${NICELEVEL} transcode -x ${CODEC} -y ${OUT_MCODEC} -i ${F} -R 1 -O -V -M 2 -w ${MBITRATE} -j ${Y},0,${Y},0 -Z ${RES} -J astat -b ${ABITRATE} | awk '/-s[ 0123456789.]/ {match($0,/-s [0123456789.]+/); print substr($0,RSTART,RLENGTH)}'`
	test $? -ne 0 && (echo "ERROR: ${F}, first loop"; exit 1)

	#second loop, exit on error
	nice -n ${NICELEVEL} transcode -x ${CODEC} -y ${OUT_MCODEC} -i ${F} -o ${FN}.avi -R 2 -O -V -M 2 -w ${MBITRATE} -j ${Y},0,${Y},0 -Z ${RES} -b ${ABITRATE} ${NORMALIZE}
	test $? -ne 0 && (echo "ERROR: ${F}, second loop"; exit 1)

	#concat filenames
	AF="${AF} ${FN}.avi"
done

#get name of parent-dir as moviename
DIRNAME=`pwd`
TOP_DIRNAME=`(cd .. ; pwd)`
MOVIENAME=`basename ${DIRNAME} ${TOP_DIRNAME}`

# merge avifiles and remove them
avimerge -i ${AF} -o ${MOVIENAME}.avi
test $? -eq 0 && rm ${AF} *.log

#split in cd-sized chunks and clean up
avisplit -i ${MOVIENAME}.avi -o ${MOVIENAME} -s ${CDSIZE}
test $? -eq 0 && rm ${MOVIENAME}.avi
test $? -eq 0 && rm -f *.vdr *.mpg *.log
exit 0
