#! /usr/bin/perl 
# Authors : Roland SEUHS <roland@wertkarten.net> 
#         Dominique CARON <domi@lpm.univ-montp2.fr> 
# GPL License 
#
# ChangeLog: see http://www.lpm.univ-montp2.fr/~domi/vob2divx/changelog.html

use POSIX;
use Env qw(HOME);
use FileHandle;
STDOUT->autoflush(1);
#
$userconfig="
#		This file is use by vob2divx to read your default parameters
#
#		The nice level of transcode ( nice -$nice transcode ....)
\$nice=10;
#			Choose your preferred encoder
\$DIVX=xvid;	       # divx4,divx5 ....

#		Image Viewer
\$XV=xv;   # you may use \'display\' from ImageMagick

#     		Vobs Player
\$XINE=xine; # You may modify your vob Viewer (mplayer for example)

# 		DivX Player
\$AVIPLAY=mplayer; # You may modify your DivX4 Viewer (mplayer) 
#

#		Cluster config if you intend to use cluster mode
\$CLUSTER_CONFIG=/where/is/your/cluster/config/file;  # YOU MUST MODIFY THIS LINE !!!

# 		remote command if you intend to use cluster mode
\$RMCMD=ssh; 			# change this to rsh if you need

# 	Location of your Image Logo 
# if this file exist the logo will be automatically include when 
# running vob2divx /path/to/vob file_size (alias Quick mode ) 
\$LOGO=/where/is/your/logo.img;

#      Your Default Logo Position (1=TopLeft,2=TopRight,3=BotLeft,4=BotRight,5=Center)
\$POSLOGO=4;

#      Default starting time logo ( in second after the movie beginning )
\$STARTLOGO=2;

#     Default Logo Duration ( in second )
\$TIMELOGO=25;

#		Your preferred Language (fr,en,de...) for audio channel encoding 
#			( USE vob2divx rip to enable this !!!) 
\$LANGUAGE=fr;

# If for some reason vob2divx is unable to determine the audio channel 
# for your LANGUAGE, put here the audio channel number
# ( generally 0 is your language, 1 is english, 2 is another ...)
\$DEF_AUDIOCHANNEL=1;   
# to trace vob2divx put this to STDOUT
\$DEBUG=/dev/null;
# to know what system command are run by vob2divx put this to STDOUT
\$INFO=/dev/null;

# EXT SUBTITLE FILTER 5 LAST OPTIONS (here we just use 3) ...See the docs 8-(
\$EXTSUB=0:0:255;

";

############# SOME VARIABLES ###################
$release="1.1.0 (C) 2002-2003 Dominique Caron";
$last_sec=0;
$v2d="[vob2divx]";
$deb_sec=0;
$keyframes = 250;
$audiosample_length = 1000;
$PGMFINDCLIP=pgmfindclip; # New tool of transcode
$DVDTITLE=dvdtitle;
$RED="\033[1;31m";
$GREEN="\033[0;32m";
$NORM="\033[0;39m";
$MAJOR=0;
$MINOR=6;

#  Functions Declarations
sub mydie;
sub makelogo;
sub audioformat;
sub create_nav;
sub create_extract;
sub calculate_nbrframe;
sub calculate_bitrate;
sub aviencode;
sub a_bitrate;
sub get_params;
sub make_sample;
sub cluster;
sub merge;
sub twoac;
sub finish;
sub ask_filesize;
sub ripdvd;
sub readconf;
sub ask_logo;
sub zooming;
sub findclip;
sub interlaced;
sub get_audio_channel;
sub vobsample;
sub readuserconf;
sub printinfo;
sub chk_wdir;
sub smily;
sub audiorescale;


system("clear");
# Read or create the user configuration file
if ( -e $HOME."/.vob2divxrc" )
{	
        readuserconf;
}else{
        open ( USERCONF,">".$HOME."/.vob2divxrc");
        print USERCONF $userconfig;
        close USERCONF;
        readuserconf;
}


#    Find the transcode release 
$tr_vers=`transcode -v 2>&1 | awk '{print \$2}'| sed s/^v// `;
@Vers = split /\./,$tr_vers;
if (  $Vers[0] < $MAJOR  || ( $Vers[0] == $MAJOR && $Vers[1] < $MINOR ) )
{	 $tr_vers=0 ;
	$clust_percent="";
	print $RED."This vob2divx perl script does not support your transcode release\n Please upgrade to the lastest transcode release (0.6pre4 at least)\n".$NORM;
	exit(1);
}
$clust_percent="--cluster_percentage --a52_dolby_off ";
print $GREEN."$v2d\t Transcode detected release:\t\t   | $Vers[0].$Vers[1].$Vers[2]\n".$NORM;

foreach $pgm ( $XV , $XINE , $AVIPLAY ) 
{
	if ( system("which $pgm > /dev/null 2>&1 ") )
	{ 
		print "$pgm is not installed on this System :-( \n Modify your ~/.vob2divxrc to reach your configuration (DVD player, DivX player, Image viewer....) \n"; exit (0);
	}
}

$PGMF=system("which $PGMFINDCLIP >/dev/null 2>&1 ");
if ( $PGMF == 0  ) { $PGMFINDCLIP=OK; }

my $junk=system("which $DVDTITLE >/dev/null 2>&1 ");
if ( $junk != 0  ) { 	$DVDTITLE=""; }
$urldvdtitle=$GREEN."\t Vob2divx is unable to find dvdtitle in your PATH.\n\t Code Sources of dvdtitle are available at : \n\t http://www.lpm.univ-montp2.fr/~domi/vob2divx/dvdtitle.tgz\n ".$NORM."\n";

$warnclust = 
$RED."***********  WARNING ABOUT CLUSTER MODE *************".$NORM."
If you want to use a cluster :
a) The /path/to/vobs directory must be NFS mounted on each node
and have the same name.
b) You must have rsh or ssh permission on each node,
( modify your ~/.vob2divxrc to select rsh or ssh ).
c) You need to have a $RED${CLUSTER_CONFIG}$NORM file (change this value in your ~/.vob2divxrc) on the node you run vob2divx on.
This file must contain all the nodenames of your cluster:the percentage of frames to encode by each node.
Syntax of this file:
# This is a Comment
# nodename:percentage
asterix:25   #  Duron 333 Mhz
obelix:5  # 486 66Mhz
vercingetorix:70 # Thunderbird 1.2 Ghz
${RED}Of course the total of percentage frames to encode MUST be 100
\n\n".$NORM;

$usage =
$RED."            *****  Warning  *****$NORM
Please note that you are only allowed to use this program according to fair-use laws which vary from country to country. You are not allowed to redistribute 
copyrighted material. Also note that you have to use this software at your own risk.
 
------------------------------------------------------
You may want first rip vob files from a DVD :
then use:

% vob2divx /path/to/vobs rip
(where /path/to/vobs is the directory where vob files will be ripped)
It is recommended to rip your DVD with vob2divx because it save precious informations about the movie to encode. (probe.rip)
---------------------------------------------------

NB: $RED transcode will encode your movie in $DIVX format , to change this,
edit your ~/.vob2divxrc and change the \$DIVX variable. $NORM

There are 2 ways of using this program to encode your vob file(s):

1: Easy\n
-----------
% vob2divx /path/to/vobs 700
(where 700 is the desired filesize in Megabytes, and /path/to/vobs the directory where are the unencrypted vob files)
This mode (alias Quick mode) take all parameters in your ~/.vob2divxrc  which has been created the first time you have run vob2divx. 
Take a look in it please.

2: Better\n
----------
% vob2divx /path/to/vobs sample
This will ask all what it need to make the movie you want ;-)
( /path/to/vobs is the directory where are the unencrypted vob files)

------------\n
You can interrupt the program anytime. To continue encoding, just run the script
without parameters in the same directory.\n$RED\t You MUST NOT run vob2divx from the /path/to/vobs directory.\n$NORM
Vob2divx v$release
\n\n
";
$readme=" 
$RED Vob2divx $NORM is a perl script which make a lot of work for you to
rip your DVD in an AVI file with the fabulous Transcode. 

\t\t What make $RED Vob2divx $NORM ?

 1) rip your DVD to vob files
 2) detect titles ( and then the main title ) of a DVD
 3) detect the DVD volume identification if you install dvdtitle.
 4) detect if the DVD title has a multiangle video stream (and rip only one)
 5) detect if vob frames are interlaced
 6) detect all audio channel (with language) in the vob files
 7) detect all audio channel input format (mp3,dts,ac3,lpcm,mpeg2ext)
 8) is able to encode two audio channel avi files
 9) detect suggested volume rescale
10) is able to add subtitle 
11) detect the aspect ratio and then calculate the best output image size, 
	this is a function of the video bitrate (Bit per Pixel)
	it use the equation: bpp=bitrate*1000/(fps x height x width)
	where bpp depend of the size of the letterboxes
12) detect if deinterlacing is necessary (and detect if transcode was compiled 
	with the Mplayer pp lib)
13) detect if the slow Zooming transcode option (-Z) is necessary or not.
14) is able to encode on a cluster (even multi sequence units video streams)
15) is able to add your Logo to your avi (even in cluster mode).
16) And finally guide you from your DVD to your AVI file.

That's all Folk's ;)... 
All you need is perl, transcode, xv (or Imagemagick), 
a vob file viewer (mplayer or xine etc...),
a divx viewer (mplayer or aviplay etc...) 
and optionnaly dvdtitle (recommended) and pgmfindclip

You will find the latest $RED Vob2divx $NORM Release at:
$GREEN
http://www.lpm.univ-montp2.fr/~domi/vob2divx
$NORM
where you will find also the dvdtitle source code.

Enter 'vob2divx -h ' to have a small help
";


# You may Modify the Next value but take care
# (see the transcode man pages - about Bits per Pixel)
# it's used to estimate the image size of the encoded clip
$bpp=0.18;   # This value =  bitrate x 1000 / ( fps x height x width ) 

############# FUNCTIONS #########################

sub mydie
{
	print $RED.@_[0]."\n".$NORM;
	rename("tmp/dvdtitle","$vobpath/dvdtitle") if ( -e "tmp/dvdtitle" );
	rename("tmp/probe.rip","$vobpath/probe.rip") if ( -e "tmp/probe.rip" );
	exit(0);
}
use sigtrap qw(handler mydie normal-signals error-signals);

sub makelogo
{	print $DEBUG "---> Enter makelogo\n";
	if ( $addlogo > 300 )
	{ 	print $RED."Unable to create your Logo in cluster Mode\n".$NORM ;
		sleep 5 ;
		return(1);
	}
	print $GREEN."$v2d\t Making Logo\n".$NORM;
	$fparams="$params -c $from_frames-$synclogo";
	$add_logo=",logo=file=$LOGO:posdef=$poslogo:rgbswap=1:range=$start_frames_logo-$end_frames_logo";
	$filter="${add_logo}${deint}${sub_title}";
	audiorescale;
	if ( ! -e "tmp/logopass1.finish")
	{
		my $pid = fork();
		mydie "couldn't fork" unless defined $pid;
       		if ($pid)
       		{
			print $GREEN."$v2d\t Pass One ...\n".$NORM;
			$sys = "transcode -q 0 -i $vobpath $fparams -w $bitrate,$keyframes -J $filter -x vob -y $DIVX,null -V  -R 1,$DIVX.logo.log -o /dev/null";
			print $INFO $sys."\n";
			system("nice -$nice $sys") == 0  or ( system("touch tmp/wait.finish")==0 and mydie "Unable to run $sys" ) ;
			system("touch tmp/wait.finish tmp/logopass1.finish");
			wait;
		}else {smily;}
	}else
	{	 print $RED."\tLogo pass 1 already done, remove tmp/logopass1.finish to reencode it\n".$NORM;
	}
	if ( ! -e "tmp/logopass2.finish")
	{
		my $pid = fork();
       		mydie "couldn't fork" unless defined $pid;
       		if ($pid)
       		{
			print $GREEN."$v2d\t Pass Two ...\n".$NORM;
			$sys = "transcode -q 0 -i $vobpath $fparams  -w $bitrate,$keyframes -s $audio_rescale -J $filter -b $audio_bitrate -x vob -y $DIVX -V  -R 2,$DIVX.logo.log -o tmp/Logo.avi";
			print $INFO $sys."\n";
			system("nice -$nice $sys")== 0  or ( system("touch tmp/wait.finish")==0 and mydie "Unable to run $sys");
			system("touch tmp/wait.finish tmp/logopass2.finish");
			wait;
		}else{smily;}
	}else
	{ 	print $RED."\tLogo pass 2 already done, remove tmp/logopass2.finish to reencode it\n".$NORM;
	}
	rename("tmp/2-${dvdtitle}_sync.avi","tmp/withoutlogo.avi");
	$sys = "avimerge -i tmp/Logo.avi tmp/withoutlogo.avi -o tmp/2-${dvdtitle}_sync.avi";
	print $INFO $sys."\n";
	system("nice -$nice $sys");
	print $DEBUG "<--- makelogo\n";
}

sub audiorescale
{     
	print $DEBUG "---> Enter audiorescale\n";
	if ( $CLUSTER ne "NO" )
	{	
		create_extract if ( ! -e "tmp/extract.text" || ! -e "tmp/extract-ok" ) ;
		$info=`cat tmp/extract.text`;
		( $info =~ m,suggested volume rescale=(\d+.*\d+),) or mydie "Unable to find Suggested volume rescal in tmp/extract.text";
		if ( $1  > 1)
		{
			$audio_rescale = $1;
		}else
		{	$audio_rescale = 1;
		}
	}else
	{	if ( ! -e "tmp/astat" )
		{	print $RED."Unable to find the suggested Volume rescale !\n 1 is use for -s parameter\n".$NORM;
			$audio_rescale=1;
			sleep 2;
		}else
		{
			$audio_rescale=`cat tmp/astat`;
			chomp($audio_rescale);
		}
	}
	print $DEBUG "<--- audiorescale\n";
}  # END audiorescale

# This function just display a clock to wait
sub smily
{
	unlink("tmp/wait.finish");
	@t=('|','/','-','\\');
	my $i=0;
	while(! -e "tmp/wait.finish")
	{
		$i=0 if ($i >3);
		print @t[$i]."\r";
		sleep(1); 
		$i++;
	}
	sleep(1);
	unlink("tmp/wait.finish");
	exit(0);
}

sub chk_wdir
{	print $DEBUG "---> Enter chk_wdir\n";
# We check if user is not working in /path/to/vobs
	chomp($vobpath);
	$wdir=`pwd`;
	chdir($vobpath);
	( `pwd` ne $wdir ) or mydie "You MUST NOT run vob2divx from the /path/to/vob directory ...\nPlease cd to another directory";
	chomp($wdir);
	chdir($wdir);
# Verify that there is no alien files in vobpath
    opendir(VOB,$vobpath);
    my(@badfiles)=grep { ! /\.[Vv][Oo][Bb]$/ & -f "$vobpath/$_"} readdir(VOB);
    closedir(VOB);
    foreach $badfile (@badfiles)
	{   
		rename("$vobpath/$badfile","tmp/$badfile") if ( $badfile eq "probe.rip" || $badfile eq "dvdtitle" );
		if ( -f "$vobpath/$badfile" )
		{
			mydie "$vobpath/$badfile is not a vob(VOB) file\n\tYOU MUST remove files other than vob files, probe.rip \n or dvdtitle from $vobpath";
		}
	}

# Vrfy probe.rip
	if ( -e "tmp/probe.rip")
	{
# We need to verify if user has not remove some Vob file(s) since the rip
		opendir(VOB,$vobpath);
		my(@files)=grep { /\.[Vv][oO][bB]$/ } readdir(VOB);
		$i=0;
		foreach $vob ( @files){$i++}
		print $INFO "\t Number of vob files:$i\n";
		closedir(VOB);
		open (PROBE,"<tmp/probe.rip");
		while(<PROBE>)
		{
			if ( $_=~ m,Number of vob files:(\d+),)
			{
				if ( "$i" ne "$1" )
				{
# If user has remove some Vob file we cannot use probe.rip anymore
					print $RED."Number of vob files in probe.rip is not exact, Vob2divx will create extract.txt \n".$NORM;
					system ("touch tmp/probe.rip-BAD");
				}
				$flag=0; last;
			}else{
				$flag=1;
			}
		}
		close(PROBE);
		print $RED."Oups...no number of Vob files in the probe.rip file!\n".$NORM if ( $flag eq 1 );
	}

    if ( ! defined($dvdtitle) && -e "tmp/dvdtitle")
    {   $dvdtitle=`cat tmp/dvdtitle`;
        chomp($dvdtitle);
    }
	print $DEBUG "<--- chk_wdir\n";
} # End Check Working directories


# find a vob for samples (one from the middle of movie and the last vob )
sub vobsample
{	
	opendir(VOB,$vobpath);	
	my(@files)=grep {/\.[Vv][Oo][Bb]$/ & -f "$vobpath/$_"} readdir(VOB);
	closedir(VOB);
	@files=sort @files;
	my $i=0;
	foreach $file (@files)
	{	$i++;
		print $INFO "File $i: $file\n";
	}
	$sample = $files[floor($i / 2)];
	$lastvob = $files[$i-1];
	(-f "$vobpath/$sample" and -f "$vobpath/$lastvob" ) or mydie "Unable to find samples VOB files in $vobpath (files extension MUST be .vob or .VOB)";
}

sub get_audio_channel
{	print $DEBUG "---> Enter get_audio_channel\n";	
	$number_of_ac=0;
	my($i)=0;
	if ( -e "tmp/probe.rip" )
	{
		open (RIP,"<tmp/probe.rip");
		while(<RIP>)
		{       chomp;
			if ( $_ =~ m,(?:ac3|mpeg2ext|lpcm|dts|mp3) ([^\s]+) , )
			{
				print $GREEN."$v2d\t Language of audio stream $i:\t\t   | $1\n".$NORM if ( ! defined(@_[0])); 
				$findaudio_channel=$i if ( $1 eq $LANGUAGE && ! defined($findaudio_channel));
				$i++;
			}
# ALWAYS TCPROBE !!!
# We need the next because YES sometime audio channel are not in order !!!
#			if ( $_ =~ m,track: -a (\d+) \[,)
#			{
#			  	@achannels[$number_of_ac]=$1;
#				$number_of_ac++;
#			}
		}
	}		
	return(0) if ( defined(@_[0])); # get_audio_channel has been call by printinfo ;-)
	$probe=`tcprobe -i $vobpath 2>&1 | grep 'audio track:'` or mydie "Problem when running \'tcprobe -i $vobpath\'";
# If this is so complicated this is because ... YES sometime audio channel are not in order !!
	@line=split /\n/,$probe;
	$tmp=@line[0];
	while ( $tmp ne "" )
	{	
		@info=split /\[/,$tmp;
		@junk=split /-a/,@info[0];
		@achannels[$number_of_ac]=@junk[1];
		print $INFO "\t @info[0]\n" ;
		$number_of_ac++;
		$tmp=@line[$number_of_ac];
	}
	print $GREEN."$v2d\t Number of audio channels detected:\t   | $number_of_ac\n".$NORM;
	$number_of_ac--;
	if ( defined($findaudio_channel))
        {
                $audio_channel=$findaudio_channel;
                print $GREEN."$v2d\t Audio channel for $RED$LANGUAGE$GREEN language:\t\t   | $audio_channel\n\t (You may modify your \$LANGUAGE variable in your ~/. vob2divxrc)\n".$NORM;
        } else
	{
		print $GREEN."$v2d\t Unable to find your Language ($LANGUAGE) in:\t   | tmp/probe.rip \n$v2d\t Default audio channel is set to:\t   | ";
		if ( $def_ac <= @achannels[$number_of_ac])
		{ 
			$audio_channel = $def_ac;
			print "(see your ~/.vob2dixrc) ".$RED;
		}else{
			$audio_channel = 0 ;
			print $RED;
		}	
		print $audio_channel."\n".$NORM;
	}
		print $DEBUG "  <--- get_audio_channel\n";
}

sub readuserconf
{
  open (USERCONF,"<$HOME/.vob2divxrc");
        while (<USERCONF>)
        {
		chomp;
		SWITCH:
		{	
	 	   if ( $_ =~ m,\$nice\s*=\s*([^\s]*)\s*;,){$nice=$1; last SWITCH;}
	 	   if ( $_ =~ m,\$DIVX\s*=\s*([^\s]*)\s*;,){$DIVX=$1; last SWITCH;}
		   if ( $_ =~ m,\$XV\s*=\s*([^\s]*)\s*;,){$XV=$1;last SWITCH;}
		   if ( $_ =~ m,\$XINE\s*=\s*([^\s]*)\s*;,){$XINE=$1;last SWITCH;}
		   if ( $_ =~ m,\$AVIPLAY\s*=\s*([^\s]*)\s*;,){$AVIPLAY=$1;last SWITCH;}
		   if ( $_ =~ m,\$CLUSTER_CONFIG\s*=\s*([^\s]*)\s*;,){$CLUSTER_CONFIG=$1;last SWITCH;}
		   if ( $_ =~ m,\$RMCMD\s*=\s*([^\s]*)\s*;,){$RMCMD=$1;last SWITCH;}
		   if ( $_ =~ m,\$LOGO\s*=\s*([^\s]*)\s*;,){$LOGO=$1;last SWITCH;}
		   if ( $_ =~ m,\$POSLOGO\s*=\s*([^\s]*)\s*;,){$defposlogo=$1;last SWITCH;}
		   if ( $_ =~ m,\$STARTLOGO\s*=\s*([^\s]*)\s*;,){$defbeginlogo=$1;last SWITCH;}
		   if ( $_ =~ m,\$TIMELOGO\s*=\s*([^\s]*)\s*;,){$deftimelogo=$1;last SWITCH;}
		   if ( $_ =~ m,\$LANGUAGE\s*=\s*([^\s]*)\s*;,){$LANGUAGE=$1;last SWITCH;}
		   if ( $_ =~ m,\$DEF_AUDIOCHANNEL\s*=\s*([^\s]*)\s*;,){$def_ac=$1;last SWITCH;}
		   if ( $_ =~ m,\$DEBUG\s*=\s*([^\s]*)\s*;,){$DEBUG=$1;last SWITCH;}
		   if ( $_ =~ m,\$INFO\s*=\s*([^\s]*)\s*;,){$INFO=$1;last SWITCH;}
		   if ( $_ =~ m,\$EXTSUB\s*=\s*([^\s]*)\s*;,){$EXTSUB=$1;last SWITCH;}
		}
        }
        close (USERCONF);
	$DIVX=xvid if ( ! defined ($DIVX));
	$XV=xv if ( ! defined ($XV));
	$XINE=xine if ( ! defined ($XINE));
	$AVIPLAY=aviplay if ( ! defined ($AVIPLAY));
	$RMCMD=rsh if ( ! defined ($RMCMD));
	$POSLOGO=4 if ( ! defined ($POSLOGO));
	$CLUSTER_CONFIG="/see/your/vob2divxrc" if ( ! defined ($CLUSTER_CONFIG));
	$TIMELOGO=25 if ( ! defined ($TIMELOGO));
	$LOGO="/see/your/vob2divxrc" if ( ! defined ($LOGO));
	$DEF_AUDIOCHANNEL=0 if ( ! defined ($DEF_AUDIOCHANNEL));
	$LANGUAGE=fr  if ( ! defined ($LANGUAGE));
	$STARTLOGO=2 if ( ! defined ($STARTLOGO));
	$nice=10 if ( ! defined ($nice));
	$INFO="STDOUT" if ( ! defined ($INFO));
	$DEBUG="/dev/null" if ( ! defined ($DEBUG));
	if ( ! defined ($EXTSUB))
		{ open (USERCONF,">>$HOME/.vob2divxrc");
			print USERCONF "# EXT SUBTITLE FILTER 5 LAST OPTIONS (here we just use 3) ...See the docs 8-(\n\$EXTSUB=0:0:255;";
			close(USERCONF);
			$EXTSUB="0:0:255";
		} 

}  # END readuserconf

sub printinfo
{	
	system("clear") if ( $DEBUG ne STDOUT && $INFO ne STDOUT );	
	print "\t*********************************************************\n";
	print $v2d."   V:\tVideo Output format:\t(1)| $DIVX\n";
	print $v2d."   V:\tVideo Input interlaced:\t   | ";
	if ( defined($deintl) || $params=~ m,-I 3, )
	{  	print $RED."YES\n".$NORM;
		print $v2d."   V:\tDeinterlaced with:\t(2)| ";
		print "MPlayer postproc\n" if ( defined($deintl));
		print "-I 3\n" if ($params=~ m,-I 3, )
	}else{	print "NO\n";}
		
	print $v2d."   V:\tLogo file name:\t\t(1)| $LOGO\n";
	if ( ( $addlogo && $CLUSTER eq "NO" ) || ( $addlogo ne 0 && $addlogo <= 300 && $CLUSTER ne NO))
	{
		$start_frames_logo=floor(($deb_sec+$beginlogo)*$FPS);
		$end_frames_logo=floor($addlogo*$FPS+$start_frames_logo);
		$add_logo=",logo=file=$LOGO:posdef=$poslogo:rgbswap=1:range=$start_frames_logo-$end_frames_logo";
		$endlogo=$beginlogo+$addlogo;
		print  $v2d."   V:\tLogo starting time:\t(2)| $beginlogo s.\n";
		print  $v2d."   V:\tLogo ending time:\t(2)| $endlogo s.\n";
	}else
	{ 	print $v2d."   V:\tLogo inserted:\t\t   | ${RED}NO${NORM}\n";
	}
	get_audio_channel("junk") if (! defined($findaudio_channel));
	print $v2d."   A:\tLanguage Audio channel:\t(1)| $LANGUAGE\n" if (defined($findaudio_channel)) ;
	print $v2d."   C:\tCluster config file:\t(1)| $CLUSTER_CONFIG\n" if ( $CLUSTER ne NO);
	print $v2d."   C:\tCluster remote cmd:\t(1)| $RMCMD\n" if ( $CLUSTER ne NO);
	open (CC,"<$CLUSTER_CONFIG");
	my $i=1;
	while(<CC>)
	{	
		if ( $_=~ m,([^\s#]*)#*,)	
		{	
			print $v2d."   C:\tCluster node $i:\t\t(3)| $1% frames to process\n" if ( $CLUSTER ne NO && $1 ne "" );
			$i++ if ( $CLUSTER ne NO && $1 ne "" );
		}	
	}
	print $v2d."   V:\tFrames to encode:\t   | $nbr_frames, @ $FPS frames per/sec\n";
	printf($v2d." A/V:\tRuntime to encode:\t   | %d hours:%d minutes:%d sec\n",int($runtime/3600),int($runtime-int($runtime/3600)*3600)/60,$runtime-int($runtime/60)*60);
	@audio_channel=split /-a /,$params;
	@audio_channel=split / /,@audio_channel[1];
	print $v2d."   A:\tPrimary Audio channel:\t   | ".@audio_channel[0]."\n";
	if ( defined($ac2))
	{	chomp($ac2);
		print $v2d."   A:\tSecundary Audio Channel:(2)| $ac2\n";
	}
        printf($v2d."   A:\tAudio size:\t\t   | %.2f Mb @ (2) $audio_bitrate Kb/s\n",$audio_size);
	$videosize=$bitrate*1000*$runtime/(1024*1024*8);
	printf($v2d."   V:\tEstimated Video Size:\t   | %.2f Mb @ %d Kb/s\n",$videosize,$bitrate);
	$totalsize=$videosize+$audio_size;
	printf($v2d." A/V:\tEstimated Total Size:\t(2)| %.2f Mb\n",$totalsize);
	print $v2d."   V:\tInput Frame Size:\t   | ${Xaxis}x$Yaxis\n";
	printf($v2d."   V:\tClipped Frame Size:\t   | %dx%d \n",$Xaxis-2*$lr,$Yaxis-2*$tb);
	printf($v2d."   V:\tOriginal aspect ratio:\t   | %.2f:1\n",$aspect_ratio);
	print $v2d."   V:\tOutput Frame Size:\t   | ${NXaxis}x$NYaxis\n";
	printf($v2d."   V:\tFinal aspect ratio:\t   | %.2f:1\n",$NXaxis/$NYaxis);
	printf($v2d."   V:\tAspect ratio error:\t   | %.2f %\n",$AR);
	printf($v2d."   V:\tBits Per Pixel:\t\t   | %.3f\n",$fbpp);
	print $v2d." A/V:\tFinal AVI file name:\t   | $RED$dvdtitle$NORM.avi\n";
	print  $DEBUG $v2d."   T:\tTrcode main parameters:\t(2)| $params\n";
	my $filter=$add_logo.$deintl.$sub_title;
	print $DEBUG $v2d."   T:\tOptional Filters:\t(2)| $filter\n";
	print "\n";
	print "(1)This value can be modify in your ~/.vob2divxrc\n";
	print "(2)This value can be modify in tmp/vob2divx.conf\n";
	print "(3)This value can be modify in your $CLUSTER_CONFIG\n" if ( $CLUSTER ne NO);
	print $RED."\tYou can say \'no\' at this time, modify by hand some parameters \n\t in the tmp/vob2divx.conf(BUT TAKE CARE!) \n\t or in your ~/.vob2divxrc,\n\t and then rerun vob2divx without parameters\n".$NORM;
	print " Ready to encode (y|N)? ";
        $rep=<STDIN>;
        chomp($rep);
        mydie "" if ( $rep ne "y" && $rep ne "Y" );
}

sub interlaced
{
        print $GREEN."$v2d\t Trying to detect if frames are interlaced\n".$NORM;
	my $pid = fork();
	mydie "couldn't fork" unless defined $pid;
	if ($pid)
	{ 
		$interlace=`transcode -i $vobpath/$sample -J 32detect=verbose -c 200-201 2>&1 | grep interlaced`;
		system("touch tmp/wait.finish");
		wait;
	}else{smily;}
	print"\n";
	print $GREEN."$v2d\t This movie need deinterlacing:\t\t   |"; 
	if ( ! ( $interlace =~  m,interlaced = (yes),))
        {
		( $interlace =~  m,interlaced = (no),) or mydie "Unable to Detect Interlacing in $vobpath/$sample";
		$INTERLACE="no";
		print $GREEN." NO\n".$NORM;
	}else{
		$INTERLACE="yes";
		print $RED." YES\n".$NORM;
	}
}

sub findclip
{
	if ( $PGMFINDCLIP eq OK )
	{
		print $GREEN."$v2d\t Trying to detect best Clipping..\n";
		print "\t WARNING : On very small video clips this is seriously buggy...\n".$NORM;
		opendir(VOB,$vobpath);
		my(@files)=grep {/[Vv][Oo][Bb]$/ & -f "$vobpath/$_"}readdir(VOB);
		closedir(VOB);
		@files=sort @files;
		my $i=0;
		my $pid = fork();
		mydie "couldn't fork" unless defined $pid;
		if ($pid)
		{ 	
			foreach $file (@files)
			{	system ("transcode -q 0 -z -K -i $vobpath/$file -x vob,null -y ppm -c 130-135  -o autoclip$i  >/dev/null 2>&1" )==0 or ( system("touch tmp/wait.finish && /bin/rm autoclip*.pgm ")==0 and  mydie "Unable to encode to ppm file ($vobpath/$file)" );
				$i++;
			}
			system("touch tmp/wait.finish");
			wait;
		}else{smily;}
		$clip=`pgmfindclip -b 8,8 autoclip*.pgm` or ( system("/bin/rm autoclip*.pgm") and mydie "Problem to run \'pgmfindclip -b 8,8\'\n Your pgmfindclip release is may be too old ...\n");
		chomp($clip);
		@clip=split /,/,$clip;

# We put the clipping border same size ( the smallest )
		$clip[0]=$clip[2] if ( $clip[2] < $clip[0] ) ;
		$clip[1]=$clip[3] if ( $clip[3] < $clip[1] ) ;
		system("/bin/rm autoclip*.pgm");
		$tb=$clip[0];
		$lr=$clip[1];
		print $GREEN."$v2d\t Pgmfindclip -j options:\t\t   | $tb,$lr\n".$NORM;
	}else{
		print $RED."Vob2divx has not detected pgmfindclip in your PATH... Sorry\n".$NORM;
		print "You may find pgmfindclip at http://www9.informatik.uni-erlangen.de/~Vogelgsang/bp/tctools.html\n";
		sleep(2);
		$tb=0;
		$lr=0;
	}
}

#		ASK if Cluster is used 
sub ask_clust
{
	print $DEBUG "--->  Enter ask_clust\n";
	open(CONF,">>tmp/vob2divx.conf");
	unlink("tmp/cluster.args");
	$CLUSTER="NO";
	print " Do you want to use a cluster (y|N)? ";
	$rep=<STDIN>;
	chomp($rep);
	if ( $rep eq "y" || $rep eq "Y" || $rep eq "o" || $rep eq "0")
	{	
		( -e $CLUSTER_CONFIG ) or mydie $warnclust;
		create_nav if ( ! -e "tmp/filenav-ok" or ! -e "tmp/file.nav" );
		$strF=`tail -1 tmp/file.nav | awk '{print \$1}'`;
		chomp($strF);
		if ( $strF > 0 )  # Take care there are several sequence units !!
		{       
			$display=$strF + 1;
			if ( $strF > 10 )
			{ 	print $RED."\tThere is too much sequence units in this clip to encode it\n\tin cluster mode with a good video quality\n\tReversing to NO CLUSTER...\n".$NORM;
				$strF=NO;
				sleep(2);
			}
		}
# WE need create-extract in CLUSTER Mode to have $audio_rescale :-(
		create_extract if ( $strF ne NO && ( ! -e "tmp/extract-ok" || ! -e "tmp/extract.text"));
		$CLUSTER=$strF;
	}
	print CONF "#clustermode:$CLUSTER # YOU CAN REMOVE BUT NOT MODIFY THIS LINE\n";
        close(CONF);
	print $DEBUG "<--- ask_clust\n";
# End sub ask_clust
}

#********** Cluster MODE ************
sub cluster
{ 	print $DEBUG "--->  Enter cluster\n";
	$wdir=`pwd`;
	chomp($wdir);
	( -e $CLUSTER_CONFIG ) or mydie $warnclust;

	open(GOODNODE,">/tmp/node");
	$NODE=`grep -v "^[[:space:]]*#" $CLUSTER_CONFIG | grep -v '^[[:space:]]*$$' | wc -l`;
	print $GREEN."$v2d\t Number of Nodes:\t\t   | $NODE$NORM\n";	
	if ( $NODE != 0 )
	{
		$poweroff=0;
		$poweron=0;
		$localhost=`hostname`;
		chomp($localhost);
		@allnodes=`grep -v "^[[:space:]]*#" $CLUSTER_CONFIG | grep -v '^[[:space:]]*$$'| awk -F"#" '{print \$1}'`;
		foreach $node  ( @allnodes )	
		{
			@chost=split /:/, $node;
			$rhost=@chost[0];
			if ( $rhost ne "$localhost" )
			{	$rs=system("$RMCMD $rhost whoami >/dev/null 2>&1");
				if ( $rs != 0 )
				{
					$NODE = $NODE - 1;
					print $RED."\tNode $rhost unreachable ...\n";
					$P=@chost[1];
					chomp($P);
					$poweroff = $poweroff + $P;
					print "\tNeed to calculate $poweroff % of frames on other node(s)\n".$NORM;
					last if ( $poweroff >= 100 );
				}else{
					$P=@chost[1];
					chomp($P);
					print GOODNODE "$rhost:$P\n";
					$poweron = $poweron + $P;
				}
			}else{
				$P=@chost[1];
				chomp($P);
				print $DEBUG "Host : ".@chost[0]." , Pow = $P\n";
				print GOODNODE "$rhost:$P\n";
	                        $poweron = $poweron + $P;
			}
		}
		$addpower = $poweroff / $NODE;
		close(GOODNODE);
	}else{
		unlink("/tmp/node");
	}
	if ( -e "/tmp/node" )
	{	
		$tabpower="";
		$sumpow=0;
		$max=0;	
		$i=0;
		@allnodes=`cat /tmp/node`;
		unlink("/tmp/node");
		foreach $node (@allnodes )     
	        { 
	                @chost=split /:/, $node; 
	                $rhost=@chost[0];
			if ( $sumpow < 100 )
			{	$pow=@chost[1];
				$pow=$pow + $addpower;
				$max=$sumpow + $pow;
				$pow = 100 - $sumpow if ( $max > 100 );
				open(CLUSTERARGS,">tmp/cluster.args");
				print CLUSTERARGS "$sumpow,$pow\n";
				close(CLUSTERARGS);
				print $GREEN."$v2d\t Encoding on node $rhost:\t   | -W $sumpow,$pow\n".$NORM;
				if ( $rhost ne "$localhost" ) 
	                	{ 	
					system ("xterm -n rhost -e $RMCMD $rhost vob2divx runclust $wdir &");
				}else{
					system ("xterm -n $rhost -e vob2divx runclust $wdir &");
				}
				@tabpower[$i]=$sumpow;
				$i++;
				$sumpow= $sumpow + $pow;
				sleep (7);
			}
		}
		if  ( $sumpow <  100 )
		{
			$pow = 100 - $sumpow;
			open(CLUSTERARGS,">tmp/cluster.args");
			print  CLUSTERARGS "$sumpow,$pow\n";
			close(CLUSTERARGS);
			print $GREEN."$v2d\t Encoding on localnode with:\t\t   | -W $sumpow,$pow to finish\n".$NORM;
			system ("xterm -e vob2divx runclust $wdir &");
			@tabpower[$i]=$sumpow;
			sleep(3);
		}
		print $GREEN."$v2d\t Wait for nodes finish ....\n".$NORM;
		foreach $endnode ( @tabpower )
		{	
			while ( ! -e "tmp/2-$dvdtitle${endnode}_0.finish" )
			{
				print "\r|"; sleep(1); print "\r/"; sleep(1); print "\r-"; sleep(1); print "\r\\"; sleep(1);
			}
			print $GREEN."$v2d\t Node $endnode has finished to encode\n".$NORM;
		}	
		merge;
		twoac;
		finish;
		unlink("tmp/cluster.args");
	} else {
		print $RED."\tNothing to do :-(\n".$NORM;
	}
	print $DEBUG "<--- cluster\n";
	exit(0);
# End cluster Sub routine
}


#  ******************** Create Nav File (For cluster) ********************

sub create_nav
{
	 print $GREEN."$v2d\t Using Cluster, creating:\t\t   | tmp/file.nav\n".$NORM;
	my $pid = fork();
        mydie "couldn't fork" unless defined $pid;
        if ($pid)
        {
		$sys = "cat $vobpath/*.[Vv][Oo][Bb] | tcdemux -W > tmp/file.nav";
		print $INFO $sys."\n";
		system ("nice -$nice $sys") == 0  or ( system ("touch tmp/wait.finish")== 0 and mydie "Unable to create file nav" );
		system("touch tmp/wait.finish tmp/filenav-ok");	
		wait;
	}else{smily;}
}


# ***********************Create extract info (to calculate bitrate) *******

sub create_extract
{	
	print $DEBUG "--->  Enter create_extract\n";
	get_audio_channel if ( ! defined($audio_channel));
	$audio_format=audioformat("-a $audio_channel");	
	a_bitrate if ( ! defined($audio_bitrate));
	$info=`tcprobe -i $vobpath 2> /dev/null` or mydie "Problem when running \'tcprobe -i $vobpath\'";
	$info =~ m,frame rate: -f (\d+\.\d+) \[,;
	my($FPS)=$1;
	
	print $GREEN."$v2d\t Creating :\t\t\t\t   | tmp/extract.text\n".$NORM;
	my $pid = fork();
        mydie "couldn't fork" unless defined $pid;
        if ($pid)
        {
		if ( $audio_format eq 'pcm' ) # Do not need to decode audio
		{ 	$sys = "cat $vobpath/*.[Vv][Oo][Bb] | nice -$nice tcextract -x $audio_format -t vob | nice -$nice tcscan -b $audio_bitrate -x pcm -f $FPS 2>> tmp/extract.text  >> tmp/extract.text";
		}else{
		$sys = "cat $vobpath/*.[Vv][Oo][Bb] | nice -$nice tcextract -x $audio_format -t vob | nice -$nice tcdecode -x $audio_format | nice -$nice tcscan -b $audio_bitrate -x pcm -f $FPS 2>> tmp/extract.text  >> tmp/extract.text";
		}
		print $INFO "$sys\n";
		system ("nice -$nice $sys") == 0  or ( system("touch tmp/wait.finish")==0 and mydie "Unable to create extract.txt" ) ;
		system("touch tmp/wait.finish tmp/extract-ok");
		wait;
	}else{smily;}
	print "\n";
	print $DEBUG "<--- create_extract\n";
}

# **********************Calculate How many Frames to encode ******************

sub calculate_nbrframe
{
	print $DEBUG "---> Enter calculate_nbrframe\n";
#	We need Info about Clip

	if ( -e "tmp/probe.rip" && ! -e "tmp/probe.rip-BAD")
	{  	$info = `cat tmp/probe.rip`;
		$LOG="tmp/probe.rip";
	}else
	{
		create_extract if (! -e "tmp/extract-ok" || ! -e "tmp/extract.text" );
    		$info = `cat tmp/extract.text`;
		$LOG="tmp/extract.text";
	}
	($info =~ m,V: (\d+) frames,) or mydie "Unable to find number of frames to encode in $LOG" ;
	$tot_frames = $1;

   	( $info =~ m,sec @ (\d+\.\d+) fps,) or mydie "Unable to find number of FPS in $LOG";
        $FPS=$1;

    	$nbr_frames= floor($tot_frames - ($deb_sec+$last_sec)*$FPS);
	print $DEBUG "<--- calculate_nbrframe\n";
}

# ********** Calculate Bitrate ****************

sub calculate_bitrate
{	print $DEBUG "--->  Enter calculate_bitrate\n";

# We need Audio Bitrate
	 a_bitrate if ( ! defined($audio_bitrate) );

# And Also Info about Clip
        if ( -e "tmp/probe.rip" && ! -e "tmp/probe.rip-BAD" )
        {       $info = `cat tmp/probe.rip`;
		$LOG="tmp/probe.rip";
        }else   
        {
                if (! -e "tmp/extract-ok" ||  ! -e "tmp/extract.text" )
                {       create_extract;
                }
                $info = `cat tmp/extract.text`;
		$LOG="tmp/extract.txt";
        }

	( $info =~ m,frames\, (\d+) sec @ ,) or mydie "Unable to find Video Runtime in $LOG";
	$fulltime=$1;
	mydie " ERROR : You said end credits is $last_sec sec. long, but this movie in only $fulltime sec." if ( $fulltime < $last_sec );
        $runtime=$fulltime - ($deb_sec+$last_sec);

	( $info =~ m, A: (\d+\.*\d+) MB @ ,) or mydie "Unable to find Audio Size in $LOG";
	$audio_size = $1*$runtime/$fulltime;

	( $info =~ m, A: .* MB @ (\d+) kbps,) or mydie "Unable to find audio bitrate in $LOG";

	$audio_size = $audio_size*$audio_bitrate/$1;

	$audio_size=2*$audio_size if ( defined($ac2));

	ask_filesize if ( ! $filesize );

	$bitrate = floor(($filesize - $audio_size)/$runtime * 1024 * 1024 * 8 / 1000);
	if ($bitrate < 20)
	{	
		print $RED."\n#### ATTENTION ####\n\tCalculated bitrate is $bitrate kbps, \nwhich does not make much sense, I'll use 700 kbps instead. \nFilesize will not match your preferred filesize. Sorry\n".$NORM." Press Enter -->";
		$junk=<STDIN>;
		$bitrate = 700;
	}
# audio_rescale for CLUSTER mode
	$info=`cat tmp/extract.text` if ( -f "tmp/extract.text");
	if ( $info =~ m,suggested volume rescale=(\d+.*\d+),)
        {		$audio_rescale = $1;
        }

	print $DEBUG "<--- calculate_bitrate\n";

} # END calculate_bitrate

# ********** Main Avi encode ***************

sub aviencode
{       	print $DEBUG "--->  Enter aviencode\n";
# Zooming MUST have been call before aviencode
# zooming will give us $filesize $bitrate and 	Zoom
	$params .=" -$Zoom_mode ${NXaxis}x$NYaxis" if ($Zoom_mode eq Z);
	$params .=" -$Zoom_mode $zH,$zW,$row" if ($Zoom_mode eq B);
	
	printinfo if ( ! -e "tmp/cluster.args");
	cluster if ( $CLUSTER ne "NO" && ! -e "tmp/cluster.args");


	if (  $CLUSTER ne "NO" )	
	{ 	$cluster=`cat tmp/cluster.args`;
		chomp($cluster);
		$cluster="-W $cluster,tmp/file.nav";
		$node=`cat tmp/cluster.args| awk -F, '{print \$1}'`;
		chomp($node);
		$fparams="$params $cluster";
		chomp($CLUSTER);
		$sequnit=$CLUSTER;
	}else{
		$cluster="";
		$sequnit=0;
# Encode all frames (only if $last_sec AND $deb_sec < 600 ) , we'll split after
		if ( $deb_sec < 600 )
		{  $from_frames=0; } else {  $from_frames=$deb_sec*$FPS}
		if ( $last_sec < 600 )
		{ $to_frames=$tot_frames; } else { $to_frames=$tot_frames-$last_sec*$FPS }
        	$fparams="$params -c $from_frames-$to_frames";
	}

	system("rm tmp/*.done  2> /dev/null");

	$start_frames_logo=floor(($deb_sec+$beginlogo)*$FPS);
	$end_frames_logo=floor($addlogo*$FPS+$start_frames_logo);

for ( $i=$sequnit; $i >= 0 ; $i--  )
    	{      
		print("*** SEQ UNIT = $i ********\n***  Cluster NODE number : $node ******* \n")  if (  $CLUSTER ne "NO" );
		if ( $addlogo && $i == 0 && $CLUSTER eq "NO" )
       		{
	       		$add_logo=",logo=file=$LOGO:posdef=$poslogo:rgbswap=1:range=$start_frames_logo-$end_frames_logo";
	        }else
		{	 $add_logo="";
		}

		$filter=$add_logo.$deintl.$sub_title;
# WE NEED the next 4 lines  because in non cluster mode we do not have the $sequnit value, 
# And WE want encode all the sequences unit (so, no -S option) .
	if ( $sequnit != 0 )
	{	$seqopt="-S $i,all";
	}else
	{       $seqopt="";
	}
	if (! -e "tmp/1-${dvdtitle}${node}_${i}.finish")
	{
		unlink("tmp/merge.finish");
		print $GREEN."$v2d\t Encode: $vobpath Pass One ....\n".$NORM;
               	my $pid = fork();
               	mydie "couldn't fork" unless defined $pid;
               	if ($pid)
		{
			wait;
			system("touch tmp/1-${dvdtitle}${node}_${i}.finish");
               	} else
               	{	
			$sys = "transcode -i $vobpath $seqopt $clust_percent $fparams -w $bitrate,$keyframes -J astat=\"tmp/astat\"$filter -x vob -y $DIVX,null -V  -R 1,$DIVX.${dvdtitle}${node}_${i}.log -o /dev/null"; 
#			$sys = "transcode -i $vobpath $seqopt $clust_percent $fparams -w $bitrate,$keyframes -J astat=\"tmp/astat"\"$filter -x vob -y $DIVX,null -V  -R 1,$DIVX.${dvdtitle}${node}_${i}.log -o /dev/null 2>&1  | awk '/filling/{RS=\"\\r\"};/encoding fram/{ORS=\"\\r\"; print}'";
               		print $INFO "$sys\n";
			system("nice -$nice $sys"); 
			print "\n";
			exit(0);
		}
	} else
	{	print $RED."${dvdtitle}${node}_${i} already encoded, remove \"tmp/1-${dvdtitle}${node}_${i}.finish\" to reencode \n".$NORM;
	}
	audiorescale;
	if (! -e "tmp/2-${dvdtitle}${node}_${i}.finish")
	{	
		$filter="-J $filter" if ( $filter ne "" );
		unlink("tmp/merge.finish");	
		print $GREEN."$v2d\t Encode: $vobpath Pass Two ....\n".$NORM;
		my $pid = fork();
		mydie "couldn't fork" unless defined $pid;
		if ($pid)
		{	wait;
			system("touch tmp/2-${dvdtitle}${node}_${i}.finish");
		} else
		{	
			$sys = "transcode -i $vobpath $seqopt $clust_percent $fparams -s $audio_rescale -w $bitrate,$keyframes -b $audio_bitrate -x vob -y $DIVX -V $filter -R 2,$DIVX.${dvdtitle}${node}_${i}.log -o tmp/2-${dvdtitle}${node}_${i}.avi";
#			$sys = "transcode -i $vobpath $seqopt $clust_percent $fparams -s $audio_rescale -w $bitrate,$keyframes -b $audio_bitrate -x vob -y $DIVX -V $filter -R 2,$DIVX.${dvdtitle}${node}_${i}.log -o tmp/2-${dvdtitle}${node}_${i}.avi 2>&1  | awk '/filling/{RS=\"\\r\"};/encoding fram/{ORS=\"\\r\"; print}'";
			print $INFO "$sys\n";
			system("nice -$nice $sys"); 
			print"\n";
			exit(0);
		}
	} else
	{	print $RED."${dvdtitle}${node}_${i} already encoded, remove \"tmp/2-${dvdtitle}${node}_${i}.finish\" to reencode \n".$NORM;
	}
	} # end boucle for
	if ( $CLUSTER ne "NO")
        {       print ("Finish ... Wait \n ");
                sleep (3);
        }
	print $DEBUG "<--- aviencode\n";
} # END Aviencode

#			********* MERGING ( and syncing )Function **************
sub merge
{	print $DEBUG "--->  Enter merge\n";
	if (! -e "tmp/merge.finish" )
	{       unlink("tmp/sync.finish"); 
		unlink("tmp/finish");
		print $GREEN."$v2d\t Merging the sequence units\n".$NORM;	
		my $pid = fork();
		mydie "couldn't fork" unless defined $pid;
		if ($pid)
		{	wait;
			system("touch tmp/merge.finish");
		}else{	
# $CLUSTER  is known because we've pass through aviencode before
			for ( $i=$CLUSTER ; $i >= 0 ; $i-- )
			{ 	print $GREEN."$v2d\t Seq. unit :\t\t\t   | $i\n".$NORM;	
				$sys = "avimerge -i tmp/2-*_$i.avi -o tmp/tmp_movie_$i.avi";
				print $INFO "$sys\n";
		                system("nice -$nice $sys 1> /dev/null");
			}
			if ( $CLUSTER > 0 )
			{
				$sys = "avimerge -i tmp/tmp_movie_*.avi -o tmp/2-$dvdtitle.avi && rm tmp/tmp_movie_*.avi";
				print $INFO "$sys\n";
				system("nice -$nice $sys 1> /dev/null");
			} else {
				rename("tmp/tmp_movie_0.avi","tmp/2-$dvdtitle.avi");
			}
			exit(0);
		}
	}else
	{       
		print $RED."*.avi of $dvdtitle are already merge ... remove \"tmp/merge.finish\" to re-merge it\n".$NORM;
	}
	print $DEBUG "<--- merge\n";


################# add audio in cluster mode ############################

	print $DEBUG "--->  Enter synchro\n";
	audiorescale;
	if (! -e "tmp/sync.finish" )
	{	
		unlink("tmp/finish");
		unlink("tmp/sync.done") if ( -e "tmp/sync.done" );
# We need to catch the keyframe...
		$from_frames=$deb_sec*$FPS;
		$to_frames=$tot_frames-$last_sec*$FPS;
		$synclogo=int(($end_frames_logo+$keyframes)/$keyframes)*$keyframes;
		if ( $addlogo ne 0 && $addlogo <= 300 )
		{	 $start_frames=$synclogo;
		}else{
			$start_frames=$from_frames;
		}
		@tmp = split /-a /,$params;
		@tmp=split / /,@tmp[1];
		$audio_params="-a ".@tmp[0];
		print $GREEN."$v2d\t Merging Video and Audio streams\n".$NORM;
		my $pid = fork();
               	mydie "couldn't fork" unless defined $pid;
               	if ($pid)
               	{       wait;
			system("touch tmp/sync.finish");
		}else{
			$sys = "transcode -p $vobpath $audio_params -b $audio_bitrate -c $start_frames-$to_frames -s $audio_rescale -i tmp/2-$dvdtitle.avi -P 1 -x avi,vob -y raw -o tmp/2-${dvdtitle}_sync.avi -u 50";
#			$sys = "transcode -p $vobpath $audio_params -b $audio_bitrate -c $from_frames-$to_frames -s $audio_rescale -i tmp/2-$dvdtitle.avi -P 1 -x avi,vob -y raw -o tmp/2-${dvdtitle}_0.avi -u 50 2>&1  | awk '/filling/{RS=\"\\r\"};/encoding fram/{ORS=\"\\r\"; print}'";
			print $INFO "$sys\n";
			system("nice -$nice $sys")==0 or mydie "Unable to merge Audio and Video";
                        unlink("tmp/audiochannel2.finish") if ( -e "tmp/audiochannel2.finish");
			exit(0);
		}
	}else
	{	print $RED."$dvdtitle is already sync, remove \"tmp/sync.finish\" to re-sync it\n".$NORM;
	}
	print $DEBUG "<--- synchro\n";
} # END merge


####################### Encode the optionnal second audio channel #############

sub twoac
{
	print $DEBUG "---> Enter twoac\n";
	if ( defined($ac2) && ! -e "tmp/audiochannel2.finish" )
	{ 	 
		unlink("tmp/finish") if ( -e "tmp/finish");
		print $GREEN."$v2d\t Now encode and merge the second audio channel\n".$NORM;	
		audioformat("-a $ac2");
		$sys="transcode -i $vobpath -x null -s $audio_rescale -b $audio_bitrate -g 0x0 -y raw -a $ac2  -o add-on-ac2.avi -u 50";
#		$sys="transcode -i $vobpath -x null -s $audio_rescale -b $audio_bitrate -g 0x0 -y raw -a $ac2  -o add-on-ac2.avi -u 50 2>&1  | awk '/filling/{RS=\"\\r\"};/encoding fram/{ORS=\"\\r\"; print}'";
		print $INFO $sys."\n";
		system("nice -$nice $sys")==0 or mydie "Unable to encode the second audio channel";
		print"\n";
		$sys="avimerge -i tmp/2-${dvdtitle}_sync.avi -o tmp/3-${dvdtitle}_2ac.avi -p add-on-ac2.avi";
		print $INFO "$sys\n";
		system("nice -$nice $sys 1> /dev/null")==0 or mydie "Unable to merge movie and second audio channel";
		rename("tmp/3-${dvdtitle}_2ac.avi","tmp/2-${dvdtitle}_sync.avi") && system("touch tmp/audiochannel2.finish") ;
	}
	 print $DEBUG "<--- twoac\n";
} # END 2ac

#################### Finish the work ########################

sub finish
{
	print $DEBUG "---> Enter finish\n";
	if (! -e "tmp/finish")
	{
		$from_frames=$deb_sec*$FPS;
		$to_frames=$nbr_frames+$from_frames;
		if ( ($CLUSTER eq NO && ($last_sec eq 0 || $last_sec > 600) &&  ($deb_sec eq 0  || $deb_sec > 600)) || $CLUSTER ne NO )
		{
			makelogo if ( $CLUSTER ne NO && $add_logo );
			print $GREEN."$v2d\t Renaming tmp/2-${dvdtitle}_sync.avi $dvdtitle.avi\n".$NORM ;
			rename("tmp/2-${dvdtitle}_sync.avi","$dvdtitle.avi");
		}else{
			$sys="avisplit -t $from_frames-$to_frames -i  tmp/2-${dvdtitle}_sync.avi -o $dvdtitle.avi && mv $dvdtitle.avi-0000 $dvdtitle.avi";
			print $GREEN."$v2d\t Splitting the result to $nbr_frames frames.\n".$NORM;
			print $INFO "$sys\n";
			system("nice -$nice $sys");
		}
		system("touch tmp/finish");
	}
	print " Now take a look at the end of $dvdtitle.avi\n\t If for some reason the divx file does'nt reach the end credits, just edit tmp/vob2divx.conf, decrease the endtime value, remove the tmp/finish file and then run vob2divx without parameters.";
	print " Is you divx file OK? (Y/N): ";
	$rep=<STDIN>;
	chomp($rep);
	if ($rep eq "O" or $rep eq "o" or $rep eq "y" or $rep eq "Y")
	{
		print "May I clean the tmp directory and other temporaries and log files ? (y/N): ";
		$rep=<STDIN>;
		chomp($rep);
		if ($rep eq "O" or $rep eq "o" or $rep eq "y" or $rep eq "Y")
		{
			rename("tmp/dvdtitle",$vobpath."/dvdtitle") if ( -e "tmp/dvdtitle" );
			rename("tmp/probe.rip",$vobpath."/probe.rip") if ( -e "tmp/probe.rip" );
			system("/bin/rm -rf tmp/*  *.log video* audio_sample* add-on-ac2.avi");
		}
	}
	print $DEBUG "<--- finish\n";
	mydie $NORM."Bye !!!";
}  # ENd finish


#  *************   Get Audio Bitrage ************

sub a_bitrate
{       print $DEBUG "--->  Enter Audio_bitrate\n";
	while ( $audio_bitrate ne 32 && $audio_bitrate ne 48 && $audio_bitrate ne 64 && $audio_bitrate ne 96 && $audio_bitrate ne 128 && $audio_bitrate ne 256 )
	{
		print " Enter the desired MP3 Audio output bitrate (Kb/s) [default:96]: ";
		$audio_bitrate=<STDIN>;
		chomp($audio_bitrate);
		if ( $audio_bitrate eq "" )
		{
			$audio_bitrate=96;
			last;
		}
	}
	open (CONF,">>tmp/vob2divx.conf");
	print CONF "#a_bitrate:$audio_bitrate\n";
	close CONF;	
	print $DEBUG "<--- Audio_bitrate\n";
}

#*********************** Read actual conf
sub readconf
{
	print $DEBUG "---> Enter readconf\n";
if ( -e "tmp/vob2divx.conf")
{
	open (CONF,"<tmp/vob2divx.conf");
                while (<CONF>)
                {       chomp;
                        SWITCH:
                        {
                        if ( $_ =~ m,#vobpath:([^#^ ]*),) {$vobpath=$1 ;last SWITCH ;}
                        if ( $_ =~ m,#params:([^#]*),) {$params=$1 ;last SWITCH ;}
                        if ( $_ =~ m,#filesize:([^#^ ]*),) {$filesize=$1 ;last SWITCH;}
                        if ( $_ =~ m,#clustermode:([^#^ ]*),) {$CLUSTER=$1;last SWITCH;}
                        if ( $_ =~ m,#a_bitrate:([^#^ ]*),) {$audio_bitrate=$1;last SWITCH;}
                        if ( $_ =~ m,#endtime:([^#^ ]*),) {$last_sec=$1;last SWITCH;}
                        if ( $_ =~ m,#begtime:([^#^ ]*),) {$deb_sec=$1;last SWITCH;}
                        if ( $_ =~ m,#addlogo:([^#^ ]*),) {$addlogo=$1;last SWITCH;}
                        if ( $_ =~ m,#beginlogo:([^#^ ]*),) {$beginlogo=$1;last SWITCH;}
                        if ( $_ =~ m,#poslogo:([^#^ ]*),) {$poslogo=$1;last SWITCH;}
                        if ( $_ =~ m,#movietitle:([^#^ ]*),) {$dvdtitle=$1;last SWITCH;}
                        if ( $_ =~ m,#subtitle:([^#^ ]*),) {$sub_title=$1;last SWITCH;}
                        if ( $_ =~ m,#deintl:([^#^ ]*),) {$deintl=$1;last SWITCH;}
                        if ( $_ =~ m,#audiochannel2:([^#]*),) {$ac2=$1;last SWITCH;}
                        }
                }
	close(CONF);
}
	print $DEBUG " VOBPATH :$vobpath\n";
	print $DEBUG " PARAMS :$params\n";
	print $DEBUG " FILESIZE:$filesize\n";
	print $DEBUG " CLUSTER :$CLUSTER\n";
	print $DEBUG " a_bitrate :$audio_bitrate\n";
	print $DEBUG " movietitle :$dvdtitle\n";
	print $DEBUG "<--- readconf\n";
}

# ********************* Get Needed parameters ************
sub get_params
{  	print $DEBUG "--->  Enter get_params\n";
	if ( ! -e "tmp/vob2divx.conf")
# We are in Quick Mode
	{
		$i = 0;
		if ( $ARGV[$i])
		{	$vobpath = $ARGV[$i];
			$i ++;
		} else
		{	system ("echo \"$readme\" | less -R ");
			if ( $DVDTITLE eq "" ) { print $urldvdtitle;}
			exit(1);
		}
		mydie "Path: $vobpath does not exist." if (! -e $vobpath);
		mkdir ("tmp",0777);
		if ($ARGV[$i] > 1)
		{	$filesize = $ARGV[$i];
			$i++;
		}else
		{
			mydie "Please supply filesize \n\t or \"sample\" if you want to create samples for cropping.";
		}
	} else 
# We are in 'continue' mode
	{   
		readconf;
	}

	chk_wdir;
	$audio_bitrate=96 if ( ! defined($audio_bitrate));	
	ask_clust if (! defined($CLUSTER) );

#   For Quick mode only .....	

	if ( ! defined($params) ) 
	{
		vobsample;
		$dvdtitle=movie if ( ! defined ($dvdtitle));
		findclip;
		get_audio_channel if ( ! defined($audio_channel));
		interlaced;
		if ( $INTERLACE eq yes )
		{
			$PP=`transcode -J pp=lb -c 9-11  2>&1 | grep failed`;
                	if ( $PP eq "" )
			{	
				$deintl=",pp=lb";
			}else
			{
				$params=" -I 3 ";
			} 
		}
		$params .= "-a $audio_channel -j $tb,$lr";
	}
	zooming;
	if ( ! defined($addlogo) && -e $LOGO )
	{
		$LG=`transcode -J logo=help -c 9-11  2>&1 | grep rgbswap`;
        	if ( $LG ne "" )
		{
			$beginlogo=$defbeginlogo;  
			if ( $deftimelogo+$beginlogo > $runtime)
			{
				$addlogo=$runtime-$defbeginlogo;
			}else{
				$addlogo=$deftimelogo; 
			}
			$poslogo=$defposlogo;
		}else{
			print $RED."Transcode is not compile with ImageMagick.\nUnable to encode your Logo $LOGO\n".$NORM; sleep 1;
		}
	}

# End Quick mode Configuration
	

	$dvdtitle=movie if ( ! defined($dvdtitle));

	open(CONF,">tmp/vob2divx.conf");
	if ( defined($vobpath) ) {print CONF "#vobpath:$vobpath # DO NOT MODIFY THIS LINE\n";}
	if ( defined($last_sec) ) { print CONF "#endtime:$last_sec\n";}
	if ( defined($deb_sec) ) { print CONF "#begtime:$deb_sec\n";}
	if ( defined($audio_bitrate) ) {print CONF "#a_bitrate:$audio_bitrate\n";}
	if ( defined($filesize) ) { print CONF "#filesize:$filesize\n";}
	if ( defined($addlogo) ){ print CONF "#addlogo:$addlogo # THIS VALUE IS THE DURING TIME OF LOGO (in sec.)\n";}
	if ( defined($beginlogo) ){ print CONF "#beginlogo:$beginlogo\n";}
	if ( defined($poslogo) ){ print CONF "#poslogo:$poslogo\n";}
	if ( defined($dvdtitle) ){ print CONF "#movietitle:$dvdtitle # DO NOT MODIFY THIS LINE\n";}
	if ( defined($params) ) { print CONF "#params:$params# YOU MUST KNOW WHAT YOU DO IN THIS LINE\n";}
	if ( defined($CLUSTER) ) { print CONF "#clustermode:$CLUSTER # YOU CAN REMOVE BUT NOT MODIFY THIS LINE\n";}
	if ( defined($sub_title) ) {print CONF "#subtitle:$sub_title\n";}
	if ( defined($deintl) ) {print CONF "#deintl:$deintl\n";}
	if ( defined($ac2) ) {print CONF "#audiochannel2:$ac2\n";}
	close(CONF);

	print $DEBUG "<--- get_params\n";
}
###################### Audio Input format ###################
sub audioformat
{
	print $DEBUG "---> Enter audioformat\n";
	@_[0] ="-a 0" if (! defined(@_[0]));
	my $audio_format=`tcprobe -i $vobpath/$sample 2> /dev/null ` or mydie "Problem when running \'tcprobe -i ".$vobpath."/".$sample."\'";
	( $audio_format =~ m,audio track: @_[0] [^n]*n 0x(\d+) .*,) or mydie "Unable to find audio channel ".@_[0]." format";
	my $tmp=$1;
	SWITCH: 
	{
	if ( $tmp == 2000 ) {  $audio_format=ac3 ; last SWITCH;}
	if ( $tmp == 50 ) {  $audio_format=mpeg2ext ; last SWITCH;}
	if ( $tmp == 10001 ) {  $audio_format=pcm ; last SWITCH;}
	if ( $tmp eq "1000F" ) {  $audio_format=dts ; last SWITCH;}
	if ( $tmp == 55 ) 
	{
		$audio_format=mp3;
		$MP3=`transcode -x null,mp3 -c 9-11 2>&1 | grep failed`;
		if ( $MP3 ne "" )
		{
			print $RED;
			print("\n *******   WARNING !! *************\n It seems that your transcode is'nt compiled with lame , it's not able to encode this audio channel \n\n");
			print $NORM;
			exit(1);
		}
		last SWITCH;
	}
	mydie "Unable to find a known audio format ($tmp is unknown)";
	}
	return($audio_format);
	print $DEBUG "<--- Audio_format\n";
}

#******************* Make Audio sample **************
sub make_sample
{
	@actmp=split / /,@_[0];
	print $GREEN."$v2d\t Making a sound sample audio channel:\t   | @actmp[6]\n".$NORM;
	@_[2] = 100 if (! defined(@_[2]));
	audioformat ("-a ".@actmp[6]);
	my $pid = fork();
        mydie "couldn't fork" unless defined $pid;
        if ($pid)
        {
		$sys = "transcode -q 0 -i $vobpath/$sample @_[0] -w 100,@_[2] -c 0-@_[2] -o @_[1].avi 2> /dev/null";
		print $INFO $sys."\n";
		system ("nice -$nice $sys") == 0 or ( system("touch tmp/wait.finish") && mydie "Unable to run\'$sys\'");
		system("touch tmp/wait.finish");
		wait;
	}else{smily;}
}

sub ask_filesize
{
                print " Enter the maximal avifile size (in MB): ";
                $filesize=<STDIN>;
                chomp($filesize);
                open(CONF,">>tmp/vob2divx.conf");
                print CONF "#filesize:$filesize\n";
                close(CONF);
}


sub ask_logo
{
	$LG=`transcode -J logo=help -c 9-11  2>&1 | grep rgbswap`;
	if ( $LG ne "" )
	{
		if ( -r $LOGO )
		{       
			print " Do you want to add the Logo $LOGO at the beginning of this movie (Y/n)? ";
			$rep= <STDIN>;
			chomp($rep);
			if ( $rep ne "N" && $rep ne "n" )
			{
				print " How many seconds after the movie beginning  must be your Logo displayed (MAX=$runtime - see your ~/.vob2divxrc for [default:$defbeginlogo]): ";
				$beginlogo=<STDIN>;
				chomp($beginlogo);
				$beginlogo=$defbeginlogo if ( $beginlogo eq ""  ) ;
				while ( ! defined($addlogo) || $addlogo + $beginlogo > $runtime )
				{
					$MAX=$runtime-$beginlogo;
					$deftimelogo=$MAX if ( $MAX < $deftimelogo);
					print "How long (in sec.) should your Logo be displayed (MAX=$MAX - see your ~/.vob2divxrc for [default:$deftimelogo])? ";
					$addlogo=<STDIN>;
					chomp($addlogo);
					$addlogo=$deftimelogo if ( $addlogo eq "" || $addlogo == 0 ) ;
				}
				print $RED."\t**** WARNING *****\As your logo timing is > 300 sec., it will NOT be encoded in CLUSTER mode !!!\n" if ( $addlogo > 300 );
				print " Where must appear your Logo (1=TopLeft,2=TopRight,3=BotLeft,4=BotRight,5=Center, see your ~/.vob2divxrc for [default:$defposlogo]): ";
				$poslogo=<STDIN>;
                                chomp($poslogo);
				$poslogo=$defposlogo if ( ! ($poslogo =~ m,[12345],));
                       	}else
                       	{
                               	$addlogo=0;
                       	}
               	}else{
                       	print $RED."\tIf you want to add a Logo at the beginning of this movie \n\t You must modify the \$LOGO variable, which point to your image file (actually $LOGO), in ~/.vob2divxrc\n".$NORM;
			$junk=<STDIN>;
			$addlogo=0;
               	}
       	} else
       	{
			print $RED."Transcode is not compile with ImageMagick... Unable to encode your Logo $LOGO".$NORM."\n" if  ( -r $LOGO );
             		$addlogo=0;
       	}      
	open(CONF,">>tmp/vob2divx.conf");
	print CONF "#addlogo:$addlogo # THIS VALUE IS THE DURING TIME OF LOGO (in sec.)\n";
	print CONF "#poslogo:$poslogo\n" if ( defined($poslogo));
	print CONF "#beginlogo:$beginlogo\n" if ( defined($beginlogo));
	close(CONF);
}
# END ask_logo


#******************Evaluate the Zoom*********************
sub zooming
{
	print $DEBUG "---> Enter Zooming\n";
# We need the Bitrate to calculate new image Size for the bpp
	calculate_bitrate;
#       We need also the Frame rate $FPS
	calculate_nbrframe if ( ! defined ($nbr_frames));

	$probe = `tcprobe -i  $vobpath/$sample 2> /dev/null ` or mydie "Problem when running \'tcprobe -i $vobpath/$sample\'";

	($probe =~ m,import frame size: -g (\d+)x,) or mydie "Unable to find Width image size";
	$Xaxis=$1;

	( $probe =~ m,import frame size: -g \d+x(\d+).*,) or mydie "Unable to find Hight image size";
	$Yaxis=$1;

	( $probe =~ m,aspect ratio: (\d+):(\d+).*,) or mydie "Unable to find Image Aspect ratio";
	$aspect_ratio=$1/$2;
	@tmp=split /-j /,$params;
	@tmp=split / /,@tmp[1];
	@clip=split /,/,@tmp[0];
	$tb=@clip[0];
	$lr=@clip[1] ;
# New in 1.0.2
	$visual_Yaxis=$Xaxis/$aspect_ratio;
	$aspect_ratio=($Xaxis-2*$lr)/($visual_Yaxis*(1-2*$tb/$Yaxis));

	if ( $Yaxis-2*$tb > 0 && $Xaxis-2*$lr > 0 )
	{
		$bpp=$bpp*$Yaxis*$Xaxis/(($Yaxis-2*$tb)*($Xaxis-2*$lr));
	}else
	{	
		mydie  "Something crazy !! Your image has a null or negative Size?\nAre you trying holographics movie ;-)?\n transcode is bad to do that ....";
	}

# New Width Image = SQRT (Bitrate * aspect / QualityRatio x FPS )
	$NXaxis=sqrt(1000*$bitrate*$aspect_ratio/($bpp*$FPS));
# Finale Image MUST have a multiple of 16 size
	@NXaxis[1]=16*floor($NXaxis/16);
	@NXaxis[2]=16*ceil($NXaxis/16);
# Limits 	
	for ( $i=1 ; $i<3 ; $i++ )
	{
	@NXaxis[$i]= $Xaxis-2*$lr if ( @NXaxis[$i] > $Xaxis-2*$lr );
	@NXaxis[$i]= 720 if ( $NXaxis > 720 );
	@NXaxis[$i]= 320 if ( $NXaxis < 320);
	}

#                       New Height
# Finale Image MUST have a multiple of 16 size
	@NYaxis[1]=16*floor((@NXaxis[1]/$aspect_ratio)/16);
	@NYaxis[2]=16*ceil((@NXaxis[1]/$aspect_ratio)/16);
	@NYaxis[3]=16*floor((@NXaxis[2]/$aspect_ratio)/16);
	@NYaxis[4]=16*ceil((@NXaxis[2]/$aspect_ratio)/16);

# If we can find similar AR with better BPP ... get it !
# Avec 1 poids de 110, si AR varie de 1% , BPP ne doit pas varier de plus de 0.009 bpp (1/110)
	$weight=110;
# Quality = BPP*weight-%aspect_ratio_error 
	$Quality=$weight*1000*$bitrate/(@NXaxis[1]*@NYaxis[1]*$FPS)-abs(@NXaxis[1]/@NYaxis[1]-$aspect_ratio)*100/$aspect_ratio;
	for ( $i=1; $i < 5 ; $i++ )
	{	
		for ( $j=1; $j<3; $j++ )
		{	print $DEBUG "---------------------\n";	
			printf($DEBUG " bpp=%.3f",1000*$bitrate/(@NXaxis[$j]*@NYaxis[$i]*$FPS));
			printf($DEBUG " and AR_err=%.3f %\n",abs(@NXaxis[$j]/@NYaxis[$i]-$aspect_ratio)*100/$aspect_ratio);
			$tmp=$weight*1000*$bitrate/(@NXaxis[$j]*@NYaxis[$i]*$FPS)-abs(@NXaxis[$j]/@NYaxis[$i]-$aspect_ratio)*100/$aspect_ratio;
			printf($DEBUG "Quality = %.6f\n",$tmp);
			if ( $tmp >= $Quality)
			{ 	
				$Quality=$tmp;
				$NXaxis=@NXaxis[$j];
				$NYaxis=@NYaxis[$i];
				$fbpp=1000*$bitrate/($NXaxis*$NYaxis*$FPS);
				print $DEBUG "CATCH !! \n";
			}
		}
	}
# Limits but normally impossible to fall into
	$NYaxis=$Yaxis if ( $NYaxis > $Yaxis );
#  zH zW and row are the -B parameters
	$row=16;
 	$zH=floor(($Yaxis-2*$tb-$NYaxis)/$row);
	$zW=floor(($Xaxis-2*$lr-$NXaxis)/$row);

	if ( ($Xaxis - 2*$lr)/16 == floor(($Xaxis - 2*$lr)/16) && ($Yaxis - 2*$tb)/16 == floor (($Yaxis - 2*$tb)/16) )
	{
		print $GREEN."$v2d\t Cripped image size is a multiple of 16, Slow Zooming is not necessary\n".$NORM if ( ! -e "tmp/cluster.args");
		$Zoom_mode="B";
	}else{
		print $RED."$v2d\tWARNING : Clipped Image size is not a multiple of 16 ..\n\t You MUST use the Slow Zooming\n".$NORM if ( ! -e "tmp/cluster.args");
                $Zoom_mode="Z";
	}
	sleep(1);
	$AR=abs(100-($NXaxis*100/($NYaxis*$aspect_ratio)));
	print $DEBUG "<--- Zooming\n";
}	


# ********************** Config ****************************

sub config
{       print $DEBUG "--->  Enter config\n";
	mydie "There is still a tmp/vob2divx.conf , please remove all tmp files\n (or at least tmp/vob2divx.conf) before running vob2dix /path/to/vob sample" if ( -e "tmp/vob2divx.conf") ;
	$vobpath = $ARGV[0];
	mydie "Directory \"$vobpath\" does not exist \n Sorry" if ( ! -e $vobpath );
	chk_wdir;
	mkdir ("tmp",0777);

	open(CONF,">>tmp/vob2divx.conf");
	print CONF "#vobpath:$vobpath # DO NOT MODIFY THIS LINE\n";
	close(CONF);

	vobsample;

	print "\n You will now have a look with \'$XINE\' on the Vob File $lastvob.\n Look how long (in seconds) is the end credits (so we can remove it), you also may find which audio stream and subtitle number you will choose.\n";
	print " Press Enter -> ";
	$junk=<STDIN>;
	system ("$XINE $vobpath/$lastvob >/dev/null 2>&1");
# How many second remove from end of movie...
	print " How long (in seconds) are the end credits (we will not process it and so increase video bitrate) [default:0]? "; 
        $last_sec=<STDIN>;
        chomp($last_sec);

        $last_sec=0 if ( $last_sec eq "" || $last_sec < 10 );
	print " How many seconds will you remove from the beginning [default:0]? ";
        $deb_sec=<STDIN>;
        chomp($deb_sec);
	$deb_sec=0 if ( $deb_sec eq "" );

	print $RED."\t**** WARNING ****\nIn cluster mode we split the movie after it's completly encoded\n".$NORM and sleep(5) if ( $last_sec > 600 or $deb_sec > 600 ); 

	open(CONF,">>tmp/vob2divx.conf");
	print CONF "#endtime:$last_sec\n";
	print CONF "#begtime:$deb_sec\n";
	close CONF;

#*************SOUND SAMPLE**********************
	$as=20;
	get_audio_channel;
       	print " Do you want to make Sound samples to find which audio channel is the one you want (y|N)? ";
       	$rep=<STDIN>;
       	chomp($rep);
	$pcm_swb="";
	my($chkpcm)=0;
	if ( $rep eq "o" ||   $rep eq "O" ||  $rep eq "y" ||  $rep eq "Y" )
	{
		for ($i = 0; $i <= $number_of_ac; $i ++)
       		{
               		make_sample("-x vob -y $DIVX -V -a $i $pcm_swb ", "audio_sample._-a_${i}_", $audiosample_length);
               		print " To ear this audio sample, please press Enter ->";
               		$junk=<STDIN>;
               		system("$AVIPLAY audio_sample._-a_${i}_.avi > /dev/null 2>&1 ") or mydie "Problem to run \'$AVIPLAY audio_sample._-a_${i}_.avi\'";
			my($audio_format)=audioformat("-a $i");
			if ($audio_format eq "pcm" && $chkpcm eq 0 )
			{       print $GREEN."$v2d\t Audio channel $i format:\t\t\t   | $audio_format\n".$NORM;
				print " Was the sound completly noisy (y|N)? ";
				$rep= <STDIN>;
                        	chomp($rep);
                        	if ( $rep eq "O" or $rep eq "o" or $rep eq "y" or $rep eq "Y")
				{	print $GREEN."$v2d\t Remake this sample with option:\t   | -d\n".$NORM;
					$pcm_swb='-d' ;
				 	unlink("audio_sample._-a_${i}_.avi");
					$i--;
					$chkpcm=1;
				 	next;	
				}
				$chkpcm=1;
			}
               		print " Was it the Audio channel you want (y|N)? ";
			$rep= <STDIN>;
			chomp($rep);
	               	if ($rep eq "O" or $rep eq "o" or $rep eq "y" or $rep eq "Y")
	               	{     
	               		unlink("audio_sample._-a_${i}_.avi");
#		      		$good_audio="-a $i $pcm_swb";
	               		$as=$i;
	               		last;
			}elsif ($i == $number_of_ac )
			{       print $RED."\tNo more Audio channels !\n".$NORM;
			}
        	       	unlink("audio_sample._-a_${i}_.avi");
			$chkpcm=0;
             	}
       	 }
	while ( ! grep (/$as/,@achannels) )
        {
               	print " Enter audio stream number to process [default:$audio_channel]? ";
               	$as=<STDIN>;
               	chomp($as);
               	$as = $audio_channel if ( $as eq "" );
		print $RED."$as : is not an available audio channel.\n".$NORM if  ( ! grep (/$as/,@achannels) );
         }
	$audio_channel=$as;
	my($auf)=audioformat("-a $as");
	print $GREEN."$v2d\t Audio channel $as format:\t\t   | $auf\n".$NORM;
	if ($auf eq 'pcm' && $chkpcm eq 0 )
	{       print $RED."$v2d\t As this audio channel is PCM format, it may be completly noisy\n".$NORM;	
		make_sample("-x vob -y $DIVX -V -a $as", "audio_sample._-a_${as}_", $audiosample_length);
                 print " Ear this audio sample, please press Enter ->";
		$junk=<STDIN>;
		system("$AVIPLAY audio_sample._-a_${as}_.avi > /dev/null 2>&1 ") or mydie "Problem to run \'$AVIPLAY audio_sample._-a_${as}_.avi\'";
		unlink("audio_sample._-a_${as}_.avi");
		print " Was this sample completly noisy (y|N)? ";
		$rep=<STDIN>;
		chomp($rep);
		if ( $rep eq "O" or $rep eq "o" or $rep eq "y" or $rep eq "Y")
		{ $pcm_swb="-d";
		}
	}
		
	$good_audio="-a $audio_channel $pcm_swb";
	if ( $number_of_ac > 0 )
	{
		print " Do you want to have another audio channel in your AVI movie (take care of the Video quality which decrease with 2 audio channels for the same movie size), this audio channel will be encoded at the same bitrate than the first audio channel (y|N)? ";
		$rep=<STDIN>;
		chomp($rep);
		if ( $rep eq "y" || rep eq "O" ||  $rep eq "o" ||  $rep eq "Y" )
		{	$ac2=100;
			while($ac2>$number_of_ac || $ac2 == $audio_channel )
			{	
				print " Enter the other audio channel number you want(MAX=$number_of_ac): ";
				$ac2=<STDIN>;
			}
			chomp($ac2);
			$auf2=audioformat("-a $ac2");
			print $GREEN."$v2d\t Audio channel $i format:\t\t | $auf2\n";
			open (CONF,">>tmp/vob2divx.conf");
			print CONF "#audiochannel2:$ac2 -$pcm_swb\n";
			close(AC2);
		}
	}
# audio bitrate
	a_bitrate;


#****************CROPPING TOP/BOTTOM ***********************
	findclip;
	print " Clipping Top/Bottom \n You must have the smallest black LetterBox at top/bottom \n (It's better to leave black LetterBox at top/bottom if you intend to have SubTitle)\n";
	print " To see the first sample, please press Enter -->";
	$rep=<STDIN>;
	system("/bin/rm video_s._-j_*.ppm 2> /dev/null ");
	$inc=8;
	while ( $rep ne "O" &&  $rep ne "o" && $rep ne "y" && $rep ne "Y" )
	{
		$sys="transcode -q 0 -z -k -i $vobpath/$sample -j $tb,$lr  -x vob,null -y ppm -c 10-11 -o video_s._-j_$tb,$lr_";
		print $INFO "$sys\n";
        	system ($sys."  > /dev/null");
		$tmp = `/bin/ls -1 video_s._-j_$tb,$lr_*.ppm`;
		@aclip = split /\n/, $tmp;
		foreach $file ( @aclip  ) { system ("$XV $file") }
		print " Are Top/Bottom LetterBoxes (-j $RED$tb$NORM,$lr) OK ?(y), to big (b) or to small (s): ";
		$rep= <STDIN>;
		chomp($rep);
		if ($rep eq "O" or $rep eq "o" or $rep eq "y" or $rep eq "Y")
		{	
			system("rm video_s._-j_$tb,$lr_*.ppm" );
			$top_bot=1;
			last;
		}elsif ( $rep eq "S" || $rep eq "s"  )
		{
			system("rm video_s._-j_$tb,$lr_*.ppm");
			$top_bot=0;
			$tb=$tb-$inc;
			if ( $tb < 0 ) { $tb = 0;}
		}elsif  ( $rep eq "B" || $rep eq "b"  )
		{
			system("rm video_s._-j_$tb,$lr_*.ppm" );
			$top_bot=0;
			$tb=$tb+$inc;
		}
	}


#***************** CROPPING LEFT RIGHT **************************
	print " Now Clipping Left/Right \n";
	print " To see the first sample, please press Enter -->";
        $rep=<STDIN>;
	$inc=8;
	while ( $rep ne "O" &&  $rep ne "o" && $rep ne "y" && $rep ne "Y" )
	{
		$sys="transcode -q 0 -z -k -i $vobpath/$sample -j $tb,$lr -x vob,null -y ppm -c 10-11 -o video_s._-j_$tb,$lr_";
		print $INFO "$sys\n";
       		system ("$sys > /dev/null");
		$tmp = `/bin/ls -1 video_s._-j_$tb,$lr_*.ppm`;
		@aclip = split /\n/, $tmp;
		foreach $file ( @aclip  ){system ("$XV $file")}
		print " Are Left/Right LetterBoxes (-j $tb,$RED$lr$NORM) OK ?(y), to big (b) or to small (s): ";
		$rep= <STDIN>;
		chomp($rep);
		if ( $rep eq "O" || $rep eq "o" or $rep eq "y" or $rep eq "Y" )
		{	
			system("rm video_s._-j_$tb,$lr_*.ppm");
			$left_right=1;
			last;
		}elsif ( $rep eq "B" || $rep eq "b"  )
		{
			system("rm video_s._-j_$tb,$lr_*.ppm");
			$left_right=0;
			$lr=$lr+$inc;
		}elsif  ( $rep eq "S" || $rep eq "s"  )
		{
			system("rm video_s._-j_$tb,$lr_*.ppm");
			$left_right=0;
			$lr=$lr-$inc;
			if ( $lr < 0 ) { $lr = 0 ;}
		}
        }

#************************* SUBTITLE ***********************
	$st=10;
	$SUBT=`tcprobe -i $vobpath -H 15 2> /dev/null` or mydie "Problem when running \'tcprobe -i $vobpath -H 15 \'";
	if ( ($SUBT =~ m,detected \((\d+)\) subtitle,))
	{      
		print $GREEN."$v2d\t Number of subtitles detected:\t   | $1 \n".$NORM;
		$number_of_st=$1-1;
		if ( -f "tmp/probe.rip")
		{
			open(PROBE,"<tmp/probe.rip");
			while(<PROBE>)
			{ 
				print $GREEN."$v2d\t Subtitle $1 language:\t\t   | $2\n".$NORM if ( $_=~ m, subtitle (\d+)=(.*),)	;
			}
			close(PROBE);
		}
		print " Do you want subtitle (y|N)? ";
		$rep= <STDIN>;
		chomp($rep);
		if ($rep eq "O" or $rep eq "o" or $rep eq "y" or $rep eq "Y")
		{
			while ( $st >= $number_of_st )
			{
				print " SubTitle number (MAX=$number_of_st)[default:0]? ";
				$st=<STDIN>;
				chomp($st);
				$st = 0 if ( $st eq "" );
			}
			$sub_title=",extsub=$st:$tb:0:1:$EXTSUB";
			open (CONF,">>tmp/vob2divx.conf");
			print CONF "#subtitle:$sub_title\n";
			close (CONF);
		}

	}
#************** ANTIALIASING & DEINTERLACING ******************** 
	interlaced;
 	print " Do you want to deinterlace this movie(";
	print "Y|n)? " if ( $INTERLACE eq "yes" );
	print "y|N)? " if ( $INTERLACE eq "no") ;
       	$rep= <STDIN>;
       	chomp($rep);

        if ( $rep eq "O" || $rep eq "o" || $rep eq "y" || $rep eq "Y" || ($INTERLACE eq "yes" && ($rep ne 'N' || $rep ne 'n')) )
        {	
		$PP=`transcode -J pp=lb -c 9-11  2>&1 | grep failed`;
#		$SD=`transcode -J smartdeinter -c 9-11 2>&1 | grep failed`;
		print $GREEN."$v2d\t Deinterlace with Mplayer postproc.:\t   | ";
		if ( $PP eq "" ) 
		{
			print "YES\n".$NORM." Do you want to use the Mplayer pp filter (y|N)? ";
        		$rep= <STDIN>;
        		chomp($rep);
       			if ( $rep eq "O" || $rep eq "o" || $rep eq "y" || $rep eq "Y" )
			{	 $deintl=",pp=lb";
				 open (CONF,">>tmp/vob2divx.conf");
				 print CONF "#deintl:$deintl\n";
				 close (CONF);
			}else {
				$dintl=" -I 3";
			}
#Sorry, only RGB input allowed for now: $deintl="_-J_smartdeinter=diffmode=2:highq=1:cubic=1";
        	}else
		{ 	print "NO\n".$NORM;
			$dintl=" -I 3";
		}
	}
        print " Does your clip need Antialiasing (slower) (y|N)? ";
        $rep= <STDIN>;
        chomp($rep);

	$aalias=" -C 3" if ( $rep eq "O" || $rep eq "o" || $rep eq "y" || $rep eq "Y" );
#       Write parameters
	( $left_right eq 1 && $top_bot eq 1 ) or mydie "Oups Sorry.. I miss some parameters :-(";
	$params = "$good_audio -j $tb,$lr$dintl$aalias";
	open (CONF,">>tmp/vob2divx.conf");
	print CONF "#params:$params # YOU MUST KNOW WHAT YOU DO IN THIS LINE\n";
	close(CONF);

# ask logo need runtime
	zooming;
#    Ask for a Logo
	ask_logo;
#     Search DVD Title
	if ( ! defined($dvdtitle))
	{	print " Enter the title of this movie (blank space available): ";
		$dvdtitle=<STDIN>;
		$dvdtitle =~ s/ /_/g;
		chomp($dvdtitle);
		if ( $dvdtitle eq "" ) { $dvdtitle="movie";}
	}
	open (CONF,">>tmp/vob2divx.conf");
        print CONF "#movietitle:$dvdtitle # DO NOT MODIFY THIS LINE\n";
	close(CONF);

}   # END Config 

#*************** RIP A DVD ***********************
sub ripdvd
{ 	
	$vobpath = $ARGV[0];
	( -e $vobpath ) or mydie "Directory \"$vobpath\" does not exist \n Sorry";
	print " On which device is your DVD [default: /dev/dvd]? " ;
	$dvd=<STDIN>;
	chomp($dvd);
	$dvd="/dev/dvd" if ( $dvd eq "" ); 
	if ( $DVDTITLE ne "" )
	{ 
		$dvdtitle=`$DVDTITLE $dvd 2> /dev/null` or die "Problem when running \'dvdtitle $dvd\'";
		chomp($dvdtitle);
	}else
	{  	
		print " Vob2divx does'nt find dvdtitle, please enter this DVD Movie Title: ";
		$dvdtitle=<STDIN>;
		$dvdtitle =~ s/ /_/g;
		chomp($dvdtitle);
		$dvdtitle = VT if ( $dvdtitle eq "" );
	}
	print $RED;
	print "******* WARNING *********\n";
	print "All files in $vobpath will be deleted !!!\n";
	print "Press Enter to continue or <Ctrl-C> to Abort\n";
	print $NORM;
	$rep=<STDIN>;
# From the excellent cpdvd of Vogelgsang (http://www9.informatik.uni-erlangen.de/~Vogelgsang/)
	$probe = `tcprobe -i \"$dvd\" 2>&1` or die "Problem when running \'tcprobe -i $dvd\'";
	($probe =~ m,DVD title \d+/(\d+),) or die "Probing DVD failed! - No DVD?";
	$totalTitles = $1;
	print " titles: total=$totalTitles\n";

	@checkTitles = 1 .. $totalTitles;
# now probe each title and find longest
	$longestLen   = 0;
	$longestTitle = 0;
	for(@checkTitles) {
# call tcprobe for info
		  $probe = `tcprobe -i \"$dvd\" 2>&1 -T $_` or die "Problem when running \'tcprobe -i $dvd\'";
# extract title playback time -> titlelen
  		($probe =~ m,title playback time: .* (\d+) sec,) or die "No time found in tcprobe for title $_ !";
 		 $titleLen[$_] = $1;
# extract title set (VTS file) -> titleset
  		($probe =~ m,title set (\d+),) or die "No title set found in tcprobe for title $_! ";
  		$titleSet[$_] = $1;
# extract angles
  		($probe =~ m,(\d+) angle\(s\),) or die "No angle found in tcprobe for title $_ !";
  		$angles = $1;
# extract chapters
  		($probe =~ m,(\d+) chapter\(s\),) or die "No chapter found in tcprobe for title $_ !";
  		$chapters = $1;

# calc hour, minute of title len
  		$sec  = $titleLen[$_];
  		$hour = int($sec / 3600);
  		$sec -= $hour * 3600;
  		$min  = int($sec / 60);
  		$sec -= $min * 60;
 
# verbose
  		printf("%02d: len=%02d:%02d:%02d titleset=%02d angles=%02d chapters=%02d\n", $_,$hour,$min,$sec,$titleSet[$_],$angles,$chapters);
 
# find largest title
  		if($titleLen[$_] > $longestLen) {
   			 $longestLen   = $titleLen[$_];
    			$longestTitle = $_;
 		 }
	}
	print " The Main Title seems to be the Title No : $longestTitle, OK ? (y/n) :";
	$rep=<STDIN>;
        chomp($rep);
	if ( $rep ne "o" &&  $rep ne "O" && $rep ne "y" && $rep ne "Y" ) 
	{ 	
	    print "Ups... Enter the Title number please : ";
	    $longestTitle=<STDIN>;
	    chomp($longestTitle);
	}
#  Check if this title is multiangle ....
	$probe = `tcprobe -i \"$dvd\" 2>&1 -T $longestTitle` or die "Problem when running \'tcprobe -i $dvd\'";
	($probe =~ m,(\d+) angle\(s\),) or die "No angle found in tcprobe for title $longestTitle !";
        $angles = $1;
	if ( $angles > 1 )
        { 	
                  print $RED."***************** WARNING!!!! *********************\n\t This is a multi angles video stream. \n";
		print $NORM." Do you know which angle number you want to rip (y|N)? ";
		$rep=<STDIN>;
        	chomp($rep);
        	die "OK ... Have a look on your DVD to find which angle you like\n Bye" if ( $rep ne "o" &&  $rep ne "O" && $rep ne "y" && $rep ne "Y" );
		print " OK ... we continue ...\n";
		print " There is $angles which one do you want? ";
		print $NORM;
                $angle=<STDIN>;
                chomp($angle);
	}else{
		$angle=1;
	}
#  Check if this title is multichapter
	($probe =~ m,(\d+) chapter\(s\),) or die "No chapter found in tcprobe for title $longestTitle !";
	$chapter=$1;
	if ( $chapter > 1 )
	{ 	print " Do you want to rip this title chapter by chapter (y|N)? ";
		$rep=<STDIN>;
                chomp($rep);
	}
#	
	system("/bin/rm -f $vobpath/*  2> /dev/null");
        open (TITLE,">$vobpath/dvdtitle");
	print TITLE $dvdtitle;
	close(TITLE);
	opendir(VOB,$vobpath);
	chdir($vobpath) or die "Unable to chdir to $vobpath.. please DO NOT USE the ~ character in the /path/to/vob";
	$sys="tcprobe -i $dvd -T $longestTitle >> probe.rip 2>&1 ";
    	system ("nice -$nice $sys");
	if ( $rep eq "y" || $rep eq "Y" || $rep eq "o" || $rep eq "0")
	{
		for ( $i=1;$i<=$chapter;$i++)
		{
			$sys="tccat -i /dev/dvd -T $longestTitle,$i,$angle | split -b 1024m - ${dvdtitle}_T${longestTitle}_C${i}_" ;
			print $INFO $sys."\n";
			system("nice -$nice $sys");
		}
	}else{
		$sys="tccat -i /dev/dvd -T $longestTitle,-1,$angle | split -b 1024m - ${dvdtitle}_T${longestTitle}_" ;
		print $INFO $sys."\n";
		system("nice -$nice $sys");
	}
#	opendir(VOB,".");
# Check if $dvdtitle is well in the vob file name AND the vob file is well in the current directory 
	my(@files)=grep { /$dvdtitle/ && -f "$_" } readdir(VOB);
	closedir(VOB);
	my($i)=0;
 	foreach $vob (@files){rename($vob,$vob.".vob");$i++;}
	open (PROBE,">>probe.rip");
	print PROBE "Number of vob files:$i" ;
	close(PROBE);
	print $GREEN."$v2d\t Vob files are in:\t\t   | $vobpath\n".$NORM;
	print " You may now run vob2divx with yours arguments to encode the vob file(s)\n\n";
	exit(0);
} # END ripdvd


#********************* MAIN () **************************

if ($ARGV[1] eq "sample")
{
        config;
	ask_clust;
}

if ($ARGV[0] eq runclust )
{
        if (defined($ARGV[1]))
        {
                $wdir=$ARGV[1];
                chdir($wdir) or mydie $wdir." does not exist or is'nt a directory";		
# We are on a cluster node all needed parameters ARE known via -->
				readconf;
# And now we can aviencode  
		zooming;
                aviencode;
                exit(0);
        }else{
                  mydie "Error: Why run vob2divx with runclust option ?\n";
        }
# We do never come here !
exit(1);
}
if ( $ARGV[0] eq "-v" )
{
        print "Vob2divx v$release\n";
        exit(0);
}

if ( $ARGV[0] eq "-h" || $ARGV[0] eq "--help" )
{
        system (" echo \"$usage\" | less -R ");
        if ( $DVDTITLE eq "" ) { print $urldvdtitle ;}
        exit(0);
}

if ($ARGV[1] eq "rip" )
{ ripdvd;
}	

if ($ARGV[1] eq "continue" || $ARGV[0] eq "continue" || ! defined($ARGV[0]))
{	unlink("tmp/cluster.args");
# We CONTINUE ....
	get_params;
}  

if (defined ($ARGV[0]) && $ARGV[0] ne  "continue" && ! defined($ARGV[1]))
{ 
	system (" echo \"$usage\" | less -R ");
  exit(1);
}

if ( $ARGV[1] > 1 && $ARGV[1] ne "sample" )
{
# Quick Mode
mydie "There is still a tmp/vob2divx.conf , please remove all tmp files\n before running vob2dix /path/to/vob SIZE" if ( -e "tmp/vob2divx.conf") ;
get_params;
}

if (1)
{
	aviencode;	
	print $GREEN."$v2d\t Renaming tmp/2-${dvdtitle}_0.avi tmp/2-${dvdtitle}_sync.avi\n".$NORM;
	rename("tmp/2-${dvdtitle}_0.avi","tmp/2-${dvdtitle}_sync.avi");
	twoac;
	finish;
# We do never come here !
	exit(1);
}
