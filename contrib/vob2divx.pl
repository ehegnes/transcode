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

# If for some reason vob2divx is unable to determine the audio channel for your LANGUAGE
# put here the audio channel number ( generaly 0 is your language, 1 is english, 2 is another ...)
\$DEF_AUDIOCHANNEL=1;   
# to trace vob2divx
\$DEBUG=/dev/null;
# to know what system command are launch
\$INFO=/dev/null;

";



$release="1.0rc2";
$PGMFINDCLIP=pgmfindclip; # New tool of transcode
$DVDTITLE=dvdtitle;
system("clear");
$RED="\033[1;31m";
$GREEN="\033[0;32m";
$NORM="\033[0;39m";
$MAJOR=0;
$MINOR=6;


#  Functions Declarations
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
sub ask_filesize;
sub ripdvd;
sub readconf;
sub ask_logo;
sub zooming;
sub findclip;
sub interlaced;
sub get_audio_channel;
sub vobsample;
sub movrip;
sub readuserconf;
sub printinfo;
sub chk_wdir;
sub smily;
sub audiorescale;


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


#    Found the transcode release 
$tr_vers=`transcode -v 2>&1 | awk '{print \$2}'| sed s/^v// `;
@Vers = split /\./,$tr_vers;
if (  $Vers[0] < $MAJOR  || ( $Vers[0] == $MAJOR && $Vers[1] < $MINOR ) )
{	 $tr_vers=0 ;
	$clust_percent="";
	print $RED."This vob2divx perl script does not support your transcode release\n Please upgrade to the lastest transcode release (0.6pre4 at least)\n".$NORM;
	exit(1);
}
else 
{ 	$tr_vers=N; 
	$clust_percent="--cluster_percentage --a52_dolby_off ";
}
print "\tTranscode detected release = ".$Vers[0].".".$Vers[1].".".$Vers[2]."\n";

foreach $pgm ( $XV , $XINE , $AVIPLAY ) 
{
	if ( system("which ".$pgm." > /dev/null 2>&1 ") )
	{ 
		print $pgm." is not installed on this System :-( \n Modify your ~/.vobdivxrc to reach your configuration (DVD player, DivX player, Image viewer....) \n"; exit (0);
	}
}

$PGMF=system("which ".$PGMFINDCLIP." >/dev/null 2>&1 ");
if ( $PGMF == 0  ) { $PGMFINDCLIP=OK; }

my $junk=system("which ".$DVDTITLE." >/dev/null 2>&1 ");
if ( $junk != 0  ) { 	$DVDTITLE=""; }
$urldvdtitle=$GREEN."\t Vob2divx is unable to find dvdtitle in your PATH.\n\t Code Sources of dvdtitle are available at : \n\t http://www.lpm.univ-montp2.fr:7082/~domi/vob2divx/dvdtitle.tgz\n ".$NORM."\n";


$warnclust = 
" ".$RED."***********  WARNING ABOUT CLUSTER MODE *************".$NORM."
If you want to use a cluster :
a) The /path/to/vobs directory must be NFS mounted on each node
and have the same name.
b) You must have rsh or ssh permission on each node,
( modify your ~/.vob2divxrc to select rsh or ssh ).
c) You need to have a ".$RED.$CLUSTER_CONFIG.$NORM." file (change this value in your ~/.vob2divxrc) on the node you run vob2divx on.
This file must contain all the nodenames of your cluster:the percentage of frames to encode by each node.
Syntax of this file:
# This is a Comment
asterix:25   #  Duron 333 Mhz
obelix:5  # 486 66Mhz
vercingetorix:70 # Thunderbird 1.2 Ghz
".$RED."Of course the total of percentage frames to encode MUST be 100".$NORM."
\n\n";

$usage =
$RED."            *****  Warning  *****".$NORM."
Please note that you are only allowed to use this program according to fair-use laws which vary from country to country. You are not allowed to redistribute 
copyrighted material. Also note that you have to use this software at your own risk.
 
------------------------------------------------------
You may want first rip vob files from a DVD :
then use:

% vob2divx /path/to/vobs rip
(where /path/to/vobs is the directory where vob files will be ripped)
It is recommended to rip your DVD with vob2divx because it save precious informations about the movie to encode. (probe.rip)
---------------------------------------------------

NB: ".$RED." transcode will encode your movie in ".$DIVX." format , to change this,
edit your ~/.vob2divxrc and change the \$DIVX variable. ".$NORM."

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
without parameters in the same directory.\n".$RED."\t You MUST not run vob2divx from the /path/to/vobs directory.\n".$NORM."
Vob2divx Release: $release
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
	this is a function of the video bitrate
	it use the equation: X=bitrate*1000/(fps x height x width)
	 where X depend of the size of the letterboxes
12) detect if deinterlacing is necessary (and detect if transcode is compile 
	with the Mplayer pp lib)
13) detect if the slow Zooming transcode option (-Z) is necessary or not.
14) is able to encode on a cluster ( even multi sequence units video streams)
15) is able to add your Logo to your avi.
16) And finally guide you from your DVD to your AVI file.

That's all Folk's ;)... 
All you need is perl, transcode, xv (or Imagemagick), 
a vob file viewer (mplayer or xine etc...),
a divx viewer (mplayer or aviplay etc...) 
and optionnaly dvdtitle (recommended) and pgmfindclip

You will find the latest $RED Vob2divx $NORM Release at:
$GREEN
http://www.lpm.univ-montp2.fr:7082/~domi/vob2divx
$NORM
where you will find also the dvdtitle source code.

Enter 'vob2divx -h ' to have a small help
";


$last_sec=0;
$keyframes = 1000;
$sample = "";
$audiosample_length = 1000;
$long_timeout = 20;

# You may Modify the Next value but take care
# it's used to estimate the image size of the encoded clip
$quality_ratio=0.15;   # This value =  bitrate x 1000 / ( fps x height x width ) 
############# FUNCTIONS #########################

	
sub audiorescale
{       if ( $CLUSTER ne "NO" && ! -e "tmp/extract.text" )
	{	create_extract;
	}
	if ( $CLUSTER ne "NO" )
	{	$info=`cat tmp/extract.text`;
		( $info =~ m,suggested volume rescale=(\d+.*\d+),) or die $RED."Unable to find Suggested volume rescal\n".$NORM ;
		if ( $1  > 1)
		{
			$audio_rescale = $1;
		}else
		{	$audio_rescale = 1;
		}
	}else
	{	if ( ! -e "tmp/astat".$node )
		{	print $RED. "Unable to find the suggested Volume rescale !\n 1 is use for -s parameter\n".$NORM;
			$audio_rescale=1;
			sleep 2;
		}else
		{
			$audio_rescale=`cat tmp/astat$node`;
			chomp($audio_rescale);
		}
	}
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

# We check if user is not working in /path/to/vobs
sub chk_wdir
{	print $DEBUG "---> Enter chk_wdir\n";
	chomp($vobpath);
	$wdir=`pwd`;
	chdir($vobpath);
	( `pwd` ne $wdir ) or die $RED." You MUST NOT run vob2divx from the /path/to/vob directory ...\n".$NORM."Please cd to another directory\n".$NORM;
	chomp($wdir);
	chdir($wdir);
}

# mv info files from /path/to/vobs to tmp if those info files exist 
sub movrip
{
	system ("mv ".$vobpath."/dvdtitle tmp") if ( -e $vobpath."/dvdtitle");
	system ("mv ".$vobpath."/probe.rip tmp") if ( -e $vobpath."/probe.rip");
	if ( ! defined($dvdtitle) && -e "tmp/dvdtitle")
	{	$dvdtitle=`cat tmp/dvdtitle`;
		chomp($dvdtitle);
	}
}

# find a vob for samples (one from the middle of movie and the last vob )
sub vobsample
{
	opendir(VOB,$vobpath);
	my(@files)=grep {/\.[Vv][Oo][Bb]$/ & -f "$vobpath/$_"} readdir(VOB);
	@files=sort @files;
	my $i=0;
	foreach $file (@files)
	{	$i++;
		print ("File ".$i.": ".$file."\n");
	}
	$sample = $files[floor($i / 2)];
	$lastvob = $files[$i-1];
	(-e $vobpath."/".$sample and -e $vobpath."/".$lastvob ) or die $RED."Unable to find samples VOB files in ".$vobpath." (files extension MUST be .vob or .VOB)\n".$NORM ;
}
	


sub get_audio_channel
{	print $DEBUG "---> Enter get_audio_channel\n";	
	$number_of_ac=0;
	if ( -e "tmp/probe.rip" )
	{
		open (RIP,"<tmp/probe.rip");
		while(<RIP>)
		{       chomp;
			if ( $_ =~ m,(?:ac3|mpeg2ext|lpcm|dts|mp3) ([^\s]+) , )
			{
				$findaudio_channel=$number_of_ac if ( $1 eq $LANGUAGE && ! defined($findaudio_channel));
				$number_of_ac++; 
			}
		}
		if ( defined($findaudio_channel))
		{
			$audio_channel=$findaudio_channel;
			print "\t Audio channel for ".$RED.$LANGUAGE.$NORM." language is: ".$audio_channel."\n\t(You may modify your \$LANGUAGE variable in your ~/.vob2divxrc)\n";
		}
	}else
	{
		$probe=`tcprobe -i $vobpath 2>&1 | grep 'audio track:'`;
		@line=split /\n/,$probe;
		$tmp=@line[0];
		while ( $tmp ne "" )
		{	$tmp=@line[$number_of_ac];
			@info=split /\[/,$tmp;
			if ( $tmp ne "" )
			{ 	@junk=split /-a/,@info[0];
				@achannels[$number_of_ac]=@junk[1];
				print @info[0]."\n" ;
				$number_of_ac++;
				@achannels[number_of_ac];
			}
		}
		print $RED." Transcode has detected $number_of_ac audio channel(s)\n";
	}
	$number_of_ac--;
	if ( !defined($audio_channel))
	{
		print $RED."Unable to find your Language (".$LANGUAGE.")... \naudio channel is set to ";
		if ( $def_ac <= @achannels[$number_of_ac])
		{ 
			$audio_channel = $def_ac;
			print "your ~/.vob2dixrc default: ";
		}else{
			$audio_channel = 0 ;
			print "default: ";
		}	
		print $audio_channel."\n".$NORM;
	}
		
		print $DEBUG "  <--- get_audio_channel\n";
}


sub readuserconf
{
  open (USERCONF,"<".$HOME."/.vob2divxrc");
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

}  # END readuserconf

sub printinfo
{	system("clear");	
	print "\tEncoded output format: ".$DIVX."\n";
	if ( $addlogo )
	{
		$startlogo=floor($beginlogo*$FPS);
		$timelogo=floor($addlogo*$FPS+$startlogo);
		$add_logo=",logo=file=".$LOGO.":posdef=".$poslogo.":rgbswap=1:range=".$startlogo."-".$timelogo;
		print "\tYour Logo ".$LOGO." will be encoded\n";
	}else
	{ 	print "\tYour Logo ".$LOGO." will NOT be encoded\n";
	}
	print "\tEncode on a Cluster using ".$CLUSTER_CONFIG."\n# hostname:% of frames to encode\n".`cat $CLUSTER_CONFIG`."\n" if ( $CLUSTER ne NO);
	 print "\t(The previous parameter(s) can be modify in your ~/.vob2divxrc)\n";
	print "\t\t* *\n";
	print ("\t Number of frames to encode: ".$nbr_frames.", @ ".$FPS." frames per/sec\n") ;
	print("\t Runtime to encode: ".$runtime." sec.\n");
	@audio_channel=split /-a /,$params;
	@audio_channel=split / /,@audio_channel[1];
	print ("\t Primary Audio channel: ".@audio_channel[0]."\n");
	if ( defined($ac2))
	{	chomp($ac2);
		print "\t Secundary Audio Channel: ".$ac2."\n";
	}
        printf("\t Audio size: %.2f Mb @ ".$audio_bitrate." Kb/s\n",$audio_size);
	$videosize=$bitrate*1000*$runtime/(1024*1024*8);
	printf("\t Video Size: %.2f Mb @ %d Kb/s\n",$videosize,$bitrate);
	print "\t Output Image Size= ".$NXaxis."x".$NYaxis."\n";
	$totalsize=$videosize+$audio_size;
	printf("\t Total Size estimated: %.2f Mb\n",$totalsize);
	print "\t Your final AVI file name will be: ".$RED.$dvdtitle.$NORM.".avi \n";
	print("\t Transcode main parameters: ".$params."\n");
        $filter=$add_logo.$ppdintl.$sub_title;
	print("\t Optional Filters: ".$filter."\n");

	print $RED."\tYou can say no at this time, modify by hand some parameters \n\t in the tmp/vob2divx.conf(BUT TAKE CARE!) \n\t or in your ~/.vob2divxrc,\n\t and then rerun vob2divx without parameters\n";
	print $GREEN."Ready to encode ?(Y/N) ".$NORM;
        $rep=<STDIN>;
        chomp($rep);
        exit(1) if ( $rep ne "y" && $rep ne "Y" );
}


sub interlaced
{
        print $GREEN."\t Trying to detect if frames are interlaced...\n".$NORM;
	my $pid = fork();
	die "couldn't fork\n" unless defined $pid;
	if ($pid)
	{ 
		$interlace=`transcode -i $vobpath/$sample -J 32detect=verbose -c 100-101 2>&1 | grep interlaced`;
		system("touch tmp/wait.finish");
	}else
	{
		smily;
	}
	print"\n";
        if ( ! ( $interlace =~  m,interlaced = (yes),))
        {
                ( $interlace =~  m,interlaced = (no),) or die $RED."Unable to Detect Interlacing in ".$vobpath."/".$sample."\n".$NORM;
                 $INTERLACE="no";
		print $RED."It seems that this movie DOES'NT NEED deinterlacing\n".$NORM;
        }else{
                $INTERLACE="yes";
		print $RED."It seems that this movie NEED deinterlacing\n".$NORM;
        }
}

sub findclip
{
	if ( $PGMFINDCLIP eq OK )
	{
		print $GREEN."\t Trying to detect best Clipping..\n".$NORM;
		print "WARNING : On very small video clips this is seriously buggy...\n";
		opendir(VOB,$vobpath);
		my(@files)=grep {/[Vv][Oo][Bb]$/ & -f "$vobpath/$_"}readdir(VOB);
		@files=sort @files;
		my $i=0;
		my $pid = fork();
		die "couldn't fork\n" unless defined $pid;
		if ($pid)
		{ 	
			foreach $file (@files)
			{	system ("transcode -q 0 -z -K -i ".$vobpath."/".$file." -x vob,null -y ppm -c 30-35  -o autoclip".$i."  >/dev/null 2>&1" )==0 or ( system("touch tmp/wait.finish")==0 and  die $RED."Unable to encode ppm files".$NORM );
				$i++;
			}
			system("touch tmp/wait.finish");
		}else
		{
			smily;
		}
		$clip=`pgmfindclip autoclip*.pgm`;
		@clip=split /,/,$clip;
		for ( $i=0; $i<4; $i++)
		{
			if ( $clip[$i]/8 - floor ($clip[$i]/8) >= 0.5 )
			{	$clip[$i]=ceil($clip[$i]/8)*8;
			}else
			{	 $clip[$i]=floor($clip[$i]/8)*8;
			}
		}

# We put the clipping border same size ( the smallest )
		if ( $clip[2] > $clip[0] ) { $clip[2] = $clip[0] } else { $clip[0] = $clip[2]}
		if ( $clip[3] > $clip[1] ) { $clip[3] = $clip[1] } else { $clip[1] = $clip[3]}
		system("/bin/rm autoclip*.pgm");
		$tb=$clip[0];
		$lr=$clip[1];
		print "\n\t pgmfindclip : -j ".$tb.",".$lr."\n";
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
	print $GREEN."\n Do you want to use a cluster ? [y/n] : ".$NORM;
	$CLUSTER=<STDIN>;
	chomp($CLUSTER);
	if ( $CLUSTER eq "y" || $CLUSTER eq "Y" || $CLUSTER eq "o" || $CLUSTER eq "0")
	{	
		( -e $CLUSTER_CONFIG ) or die $warnclust;
		create_nav if ( ! -e "tmp/filenav-ok" and ! -e "tmp/file.nav" );
		$strF=`tail -1 tmp/file.nav | awk '{print \$1}'`;
		chomp($strF);
		if ( $strF > 0 )  # Take care there are several sequence units !!
		{       
			$display=$strF + 1;
			print $RED."\n*********** WARNING !!!!************\n there is ".$display." Video stream sequence units in the Vob file(s)\n".$NORM;
			sleep(2);
			if ( $strF > 10 )
			{ 	print $RED." There is too much sequence units in this clip to encode it in cluster mode with a good video quality\n Reversing to NO CLUSTER...\n".$NORM;
				$strF=NO;
				sleep(2);
			}
		}
# WE need create-extract in CLUSTER Mode to have $audio_rescale :-(
		create_extract if ( $strF ne NO && ! -e "tmp/extract-ok" && ! -e "tmp/extract.text");
		print CONF "#clustermode:".$strF." # YOU CAN REMOVE BUT NOT MODIFY THIS LINE\n";
		$CLUSTER=$strF;
	}else
	{	print CONF "#clustermode:NO  # YOU CAN REMOVE BUT NOT MODIFY THIS LINE\n";
		$CLUSTER="NO";
	}
        close(CONF);
# End sub ask_clust
}

#********** Cluster MODE ************
sub cluster
{ 	print $DEBUG "--->  Enter cluster\n";
	$wdir=`pwd`;
	chomp($wdir);
	( -e $CLUSTER_CONFIG ) or die $warnclust;

	open(GOODNODE,">/tmp/node");
	$NODE=`grep -v "^[[:space:]]*#" $CLUSTER_CONFIG | grep -v '^[[:space:]]*$$' | wc -l`;
	print "\t Number of Nodes : ".$NODE;	
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
			{	$rs=system($RMCMD." ".$rhost." whoami >/dev/null 2>&1");
				if ( $rs != 0 )
				{
					$NODE = $NODE - 1;
					print $RED."Node ".$rhost." unreachable ...\n";
					$P=@chost[1];
					chomp($P);
					$poweroff = $poweroff + $P;
					print " Need to calculate ".$poweroff." % of frames on other node(s)".$NORM;
					last if ( $poweroff >= 100 );
				}else{
					$P=@chost[1];
					chomp($P);
					print GOODNODE $rhost.":".$P."\n";
					$poweron = $poweron + $P;
				}
			}else{
				$P=@chost[1];
				chomp($P);
				print $DEBUG "Host : ".@chost[0]." , Pow = ".$P."\n";
				print GOODNODE $rhost.":".$P."\n";
	                        $poweron = $poweron + $P;
			}
		}
		$ addpower = $poweroff / $NODE;
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
				print CLUSTERARGS $sumpow.",".$pow."\n";
				close(CLUSTERARGS);
				print $GREEN."\tEncoding on node ".$rhost."  with -W ".$sumpow.",".$pow."\n".$NORM;
				if ( $rhost ne "$localhost" ) 
	                	{ 	
					system ("xterm -n ".rhost." -e ".$RMCMD." ".$rhost." vob2divx runclust ".$wdir." &");
				}else{
					system ("xterm -n ".$rhost." -e vob2divx runclust ".$wdir." &");
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
			print  CLUSTERARGS $sumpow.",".$pow."\n";
			close(CLUSTERARGS);
			print $GREEN."Encoding on localnode with -W ".$sumpow.",".$pow." to finish...".$NORM;
			system ("xterm -e vob2divx runclust ".$wdir." &");
			@tabpower[$i]=$sumpow;
			sleep(3);
		}
		print "Wait for nodes finish ....\n";
		foreach $endnode ( @tabpower )
		{	
			while ( ! -e "tmp/2-".$dvdtitle.$endnode."_0.finish" )
			{
				print "\r|"; sleep(1); print "\r/"; sleep(1); print "\r-"; sleep(1); print "\r\\"; sleep(1);
			}
			print "Node ".$endnode." has finished to encode\n";
		}	
		merge;
		unlink("tmp/cluster.args");
	} else {
		print " Nothing to do :-( ";
	}
	print $DEBUG "<--- cluster\n";
	exit(0);
# End cluster Sub routine
}


#  ******************** Create Nav File (For cluster) ********************

sub create_nav
{
	 print("Using Cluster : creating tmp/file.nav\n");
	my $pid = fork();
        die "couldn't fork\n" unless defined $pid;
        if ($pid)
        {
		$sys = "cat ".$vobpath."/*.[Vv][Oo][Bb] | tcdemux -W > tmp/file.nav";
		print $INFO $sys."\n";
		system ("nice -".$nice." ".$sys) == 0  or ( system ("touch tmp/wait.finish")== 0 and die $RED."Unable to create file nav ".$NORM);
		system("touch tmp/wait.finish tmp/filenav-ok");	
	}else
	{
		smily;
	}
}


# ***********************Create extract info (to calculate bitrate) *******

sub create_extract
{	
	print $DEBUG "--->  Enter create_extract\n";
	get_audio_channel if ( ! defined($audio_channel));
	audioformat("-a ".$audio_channel) if ( ! defined($audio_format));	
	a_bitrate if ( ! defined($audio_bitrate));
	$info=`tcprobe -i $vobpath 2> /dev/null`;
	$info =~ m,frame rate: -f (\d+\.\d+) \[,;
	my($FPS)=$1;
	
	print("\t creating tmp/extract.text\n");
	my $pid = fork();
        die "couldn't fork\n" unless defined $pid;
        if ($pid)
        {
		$sys = "cat ".$vobpath."/*.[Vv][Oo][Bb] | nice -".$nice." tcextract -x ".$audio_format." -t vob | nice -".$nice." tcdecode -x ".$audio_format." | nice -".$nice." tcscan -b ".$audio_bitrate." -x pcm -f ".$FPS." 2>> tmp/extract.text  >> tmp/extract.text";
		print $INFO $sys."\n";
		system ("nice -".$nice." ".$sys) == 0  or ( system("touch tmp/wait.finish")==0 and die $RED."Unable to create extract.txt".$NORM ) ;
		system("touch tmp/wait.finish tmp/extract-ok");
	}else
	{
		smily;
	}
	print "\n";
	print $DEBUG "<--- create_extract\n";
}

# **********************Calculate How many Frames to encode ******************

sub calculate_nbrframe
{
	print $DEBUG "---> Enter calculate_nbrframe\n";
#	We need Info about Clip

	if ( -e "tmp/probe.rip" )
	{  	$info = `cat tmp/probe.rip`;
	}else
	{
		create_extract if (! -e "tmp/extract-ok" &&  ! -e "tmp/extract.text" );
    		$info = `cat tmp/extract.text`;
	}
	($info =~ m,V: (\d+) frames,) or die $RED."Unable to find number of frames to encode\n".$NORM ;
	$tot_frames = $1;

   	( $info =~ m,sec @ (\d+\.\d+) fps,) or die $RED."Unable to find number of FPS\n".$NORM;
        $FPS=$1;

    	$nbr_frames= floor($tot_frames - $last_sec*$FPS);
	print $DEBUG "<--- calculate_nbrframe\n";
}

# ********** Calculate Bitrate ****************

sub calculate_bitrate
{	print $DEBUG "--->  Enter calculate_bitrate\n";

# We need Audio Bitrate
	 a_bitrate if ( ! defined($audio_bitrate) );

# And Also Info about Clip
        if ( -e "tmp/probe.rip" )
        {       $info = `cat tmp/probe.rip`;
        }else   
        {
                if (! -e "tmp/extract-ok" ||  ! -e "tmp/extract.text" )
                {       create_extract;
                }
                $info = `cat tmp/extract.text`;
        }

	( $info =~ m,frames\, (\d+) sec @ ,) or die $RED."Unable to find Video Runtime\n".$NORM;
	$fulltime=$1;
        $runtime=$fulltime - $last_sec;

	( $info =~ m, A: (\d+\.*\d+) MB @ ,) or die $RED."Unable to find Audio Size\n".$NORM;
	$audio_size = $1*$runtime/$fulltime;

	( $info =~ m, A: .* MB @ (\d+) kbps,) or die $RED."Unable to find audio bitrate\n".$NORM;

	$audio_size = $audio_size*$audio_bitrate/$1;

	$audio_size=2*$audio_size if ( defined($ac2));

	ask_filesize if ( ! $filesize );

	$bitrate = floor(($filesize - $audio_size)/$runtime * 1024 * 1024 * 8 / 1000);
	if ($bitrate < 20)
	{	
		print $RED."\n#### ATTENTION ####\n\tCalculated bitrate is ".$bitrate." kbps, \nwhich does not make much sense, I'll use 700 kbps instead. \nFilesize will not match your preferred filesize. Sorry\n $NORM Press Enter -->";
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
# zooming will give us $filesize $bitrate and 	Zoom
	zooming;
	$params .=" -".$Zoom_mode." ".$NXaxis."x".$NYaxis if ($Zoom_mode eq Z);
	$params .=" -".$Zoom_mode." ".$zH.",".$zW.",".$row if ($Zoom_mode eq B);
	
	printinfo if ( ! -e "tmp/cluster.args");
	cluster if ( $CLUSTER ne "NO" && ! -e "tmp/cluster.args");

	if (  $CLUSTER ne "NO" )	
	{ 	$cluster=`cat tmp/cluster.args`;
		chomp($cluster);
		$cluster="-W ".$cluster.",tmp/file.nav";
		$node=`cat tmp/cluster.args| awk -F, '{print \$1}'`;
		chomp($node);
		$params=$params." ".$cluster;
		chomp($CLUSTER);
		$sequnit=$CLUSTER;
	}else{
		$cluster="";
# We now split after all encoded
		$params=$params." -c 0-".$tot_frames;
		$sequnit=0;
	}

	if (  $CLUSTER ne "NO" )
 	{	print("***  Cluster NODE number : ".$node." ******* \n");
		print("*** SEQ UNIT = ".$sequnit." ********\n");
	}

	system("rm tmp/*.done  2> /dev/null");
	
	for ( $i=$sequnit; $i >= 0 ; $i--  )
    	{      
	if ( $addlogo && $i == 0 && ( ! $node  ) )
       	{
		$startlogo=floor($beginlogo*$FPS);
		$timelogo=floor($addlogo*$FPS+$startlogo);
       		$add_logo=",logo=file=".$LOGO.":posdef=".$poslogo.":rgbswap=1:range=".$startlogo."-".$timelogo;
        }
	$filter=$add_logo.$ppdintl.$sub_title;
# WE NEED the next 4 lines  because in non cluster mode we do not have the $sequnit value, 
# And WE want encode all the sequences unit (so, no -S option) .
	if ( $sequnit != 0 )
	{	$seqopt="-S ".$i.",all";
	}else
	{       $seqopt="";
	}
	if (! -e "tmp/1-".$dvdtitle.$node."_".$i.".finish")
	{
		system("rm tmp/merge.finish 2> /dev/null");
		print("Encode: ".$vobpath." Pass One ....\n");
               	my $pid = fork();
               	die "couldn't fork\n" unless defined $pid;
               	if ($pid)
               	{       while(! -e  "tmp/1-".$dvdtitle.$node."_".$i.".done")
                       	{       sleep $long_timeout;
                       	}
                       	system("touch tmp/1-".$dvdtitle.$node."_".$i.".finish");
               	} else
               	{	
			$sys = "transcode -i ".$vobpath."/ ".$seqopt." ".$clust_percent." ".$params." -w ".$bitrate.",".$keyframes." -J astat=\"tmp/astat".$node."\"".$filter." -x vob -y ".$DIVX.",null -V  -R 1,".$DIVX.".".$dvdtitle.$node."_".$i.".log -o /dev/null"; 
#			$sys = "transcode -i ".$vobpath."/ ".$seqopt." ".$clust_percent." ".$params." -w ".$bitrate.",".$keyframes." -J astat=\"tmp/astat".$node."\"".$filter." -x vob -y ".$DIVX.",null -V  -R 1,".$DIVX.".".$dvdtitle.$node."_".$i.".log -o /dev/null 2>&1  | awk '/filling/{RS=\"\\r\"};/encoding fram/{ORS=\"\\r\"; print}'";
               		print $INFO $sys."\n";
			system("nice -".$nice." ".$sys) == 0 && system("touch tmp/1-".$dvdtitle.$node."_".$i.".done");
			print "\n";
			exit(0);
		}
	} else
	{	print($RED.$dvdtitle.$node."_".$i." already encoded, remove \"tmp/1-".$dvdtitle.$node."_".$i.".finish\" to reencode \n".$NORM);
	}
	audiorescale;
	if (! -e "tmp/2-".$dvdtitle.$node."_".$i.".finish")
	{	
		$filter="-J ".$filter if ( $filter ne "" );
		system("rm tmp/merge.finish 2> /dev/null");	
		print("Encode: ".$vobpath." Pass two ....\n");
		my $pid = fork();
		die "couldn't fork\n" unless defined $pid;
		if ($pid)
		{	while(! -e  "tmp/2-".$dvdtitle.$node."_".$i.".done")
			{	sleep $long_timeout;
			}
			system("touch tmp/2-".$dvdtitle.$node."_".$i.".finish");
		} else
		{	
			$sys = "transcode -i ".$vobpath."/ ".$seqopt." ".$clust_percent." ".$params." -s ".$audio_rescale." -w ".$bitrate.",".$keyframes." -b ".$audio_bitrate." -x vob -y ".$DIVX." -V ".$filter." -R 2,".$DIVX.".".$dvdtitle.$node."_".$i.".log -o tmp/2-".$dvdtitle.$node."_".$i.".avi";
#			$sys = "transcode -i ".$vobpath."/ ".$seqopt." ".$clust_percent." ".$params." -f ".$FPS." -s ".$audio_rescale." -w ".$bitrate.",".$keyframes." -b ".$audio_bitrate." -x vob -y ".$DIVX." -V ".$filter." -R 2,".$DIVX.".".$dvdtitle.$node."_".$i.".log -o tmp/2-".$dvdtitle.$node."_".$i.".avi";
#			$sys = "transcode -i ".$vobpath."/ ".$seqopt." ".$clust_percent." ".$params." -s ".$audio_rescale." -w ".$bitrate.",".$keyframes." -b ".$audio_bitrate." -x vob -y ".$DIVX." -V ".$filter." -R 2,".$DIVX.".".$dvdtitle.$node."_".$i.".log -o tmp/2-".$dvdtitle.$node."_".$i.".avi 2>&1  | awk '/filling/{RS=\"\\r\"};/encoding fram/{ORS=\"\\r\"; print}'";
                       print $INFO $sys."\n";
                       system("nice -".$nice." ".$sys) == 0 && system("touch tmp/2-".$dvdtitle.$node."_".$i.".done");
			print"\n";
                       exit(0);
		}
	} else
	{	print($RED.$dvdtitle.$node."_".$i." already encoded, remove \"tmp/2-".$dvdtitle.$node."_".$i.".finish\" to reencode \n".$NORM);
	}
	} # end boucle for
	if ( $CLUSTER ne "NO")
        {       print ("Finish ... Wait \n ");
                sleep (3);
        }
} # END Aviencode

#			********* MERGING (and Syncing) Function **************
sub merge
{	print $DEBUG "--->  Enter merge\n";
	if (! -e "tmp/merge.finish" )
	{       unlink("tmp/sync.finish"); 
		my $pid = fork();
		die "couldn't fork\n" unless defined $pid;
		if ($pid)
		{	while(! -e  "tmp/merge.done")
			{	sleep $long_timeout;
			}
			system("touch tmp/merge.finish");
			unlink("touch tmp/merge.done");
		} else
# $CLUSTER  is known because we've pass through aviencode before
		{	if ( $CLUSTER ne "NO" )
			{	print $GREEN."\t Merging the sequence units\n".$NORM;	
				for ( $i=$CLUSTER ; $i >= 0 ; $i-- )
				{ 	print "\tSeq. unit : ".$i."\n";	
					$sys = "avimerge -i tmp/2-*_".$i.".avi -o tmp/tmp_movie_".$i.".avi";
					print $INFO $sys."\n";
		                        system("nice -".$nice." ".$sys." 1> /dev/null");
				}
				$sys = "avimerge -i tmp/tmp_movie_*.avi -o tmp/2-".$dvdtitle.".avi";
				print $INFO $sys."\n";
				system("nice -".$nice." ".$sys." 1> /dev/null");
				system("rm tmp/tmp_movie_*.avi");

			}else{
				print $GREEN."\tRenaming ".$dvdtitle."_0.avi ".$dvdtitle.".avi\n".$NORM if ( $last_sec eq 0 );
			      	rename("tmp/2-".$dvdtitle."_0.avi",$dvdtitle.".avi") if ( $last_sec eq 0 );
			      	$sys="avisplit -t 0-".$nbr_frames." -i  tmp/2-".$dvdtitle."_0.avi -o ".$dvdtitle.".avi && mv ".$dvdtitle.".avi-0000 ".$dvdtitle.".avi";
				if ( $last_sec ne 0)
				{
					print $GREEN."\t Splitting the result to ".$nbr_frames." frames.\n".$NORM;
					print $INFO $sys."\n"; 
                        		system("nice -".$nice." ".$sys);
				}

			}
			system("touch tmp/merge.done");
			exit(0);
		}
	}else
	{       
		print ($RED."*.avi of ".$dvdtitle." are already merge ... remove \"tmp/merge.finish\" to re-merge it\n".$NORM);
	}

	audiorescale;
	if (! -e "tmp/sync.finish" &&  $CLUSTER ne "NO" )
	{	
		my $pid = fork();
                die "couldn't fork\n" unless defined $pid;
                if ($pid)
                {       while(! -e  "tmp/sync.done")
                        {       sleep $long_timeout;
                        }
                        system("touch tmp/sync.finish");
			unlink("tmp/sync.done");
                } else
                {
			@tmp = split /-a /,$params;
			@tmp=split / /,@tmp[1];
			$audio_params="-a ".@tmp[0];
		    print $GREEN."\t Merging Video and Audio streams\n".$NORM;
			$sys = "transcode -p ".$vobpath." ".$audio_params." -b ".$audio_bitrate." -s ".$audio_rescale." -i tmp/2-".$dvdtitle.".avi -P 1 -x avi,vob -y raw -o ".$dvdtitle.".avi -u 50";
#                       $sys = "transcode -p ".$vobpath." ".$audio_params." -b ".$audio_bitrate." -s ".$audio_rescale." -c 0-".$nbr_frames." -i tmp/2-".$dvdtitle.".avi -P 1 -x avi,vob -y raw -o ".$dvdtitle.".avi -u 50 2>&1  | awk '/filling/{RS=\"\\r\"};/encoding fram/{ORS=\"\\r\"; print}'";

			 print $INFO $sys."\n";
			system("touch tmp/sync.done");
			system("nice -".$nice." ".$sys)==0 or die " Unable to merge Audio and Video \n";
			exit(0);
		}
	    }elsif ( $CLUSTER ne NO ) 
	    {	print $RED.$dvdtitle." is already sync, remove \"tmp/sync.finish\" to re-sync it\n".$NORM;
	    }
	if ( defined($ac2) && ! -e "tmp/audiochannel2.finish" )
	{ 	 print $GREEN."\t Now encode and merge the second audio channel\n".$NORM;	
		audioformat("-a ".$ac2);
		$sys="transcode -i ".$vobpath." -x null -s ".$audio_rescale." -b ".$audio_bitrate." -g 0x0 -y raw -a ".$ac2."  -o add-on-ac2.avi -u 50";
#		$sys="transcode -i ".$vobpath." -x null -s ".$audio_rescale." -b ".$audio_bitrate." -g 0x0 -y raw -a ".$ac2."  -o add-on-ac2.avi -u 50 2>&1  | awk '/filling/{RS=\"\\r\"};/encoding fram/{ORS=\"\\r\"; print}'";
		print $INFO $sys."\n";
		system("nice -".$nice." ".$sys)==0 or die $RED."Unable to encode the second audio channel\n".$NORM;
		print"\n";
		$sys="avimerge -i ".$dvdtitle.".avi -o ac2movie.avi  -p add-on-ac2.avi";
		print $INFO $sys."\n";
		system("nice -".$nice." ".$sys." 1> /dev/null")==0 or die $RED."Unable to merge movie and second audio channel".$NORM;
		rename("ac2movie.avi",$dvdtitle.".avi") && system("touch tmp/audiochannel2.finish") ;
	}
	print $GREEN."May I clean the tmp directory and other temporaries and log files ? (Y/N): ".$NORM;
	$rep=<STDIN>;
	chomp($rep);
	if ($rep eq "O" or $rep eq "o" or $rep eq "y" or $rep eq "Y")
	{
		system ("mv tmp/dvdtitle ".$vobpath) if ( -e "tmp/dvdtitle" ); 
		system ("mv tmp/probe.rip  ".$vobpath) if ( -e "tmp/probe.rip" ); 
		system("/bin/rm -rf tmp/*  *.log video* audio_sample* add-on-ac2.avi");
	}

	print "Bye !!!\n";
	print $DEBUG "<--- merge\n";
}  # ENd merge


#  *************   Get Audio Bitrage ************

sub a_bitrate
{       print $DEBUG "--->  Enter Audio_bitrate\n";
	while ( $audio_bitrate ne 32 && $audio_bitrate ne 48 && $audio_bitrate ne 64 && $audio_bitrate ne 96 && $audio_bitrate ne 128 && $audio_bitrate ne 256 )
	{
		print $GREEN."\n By default the MP3 Audio output bitrate is set to 96kbs, enter another value to change this : ".$NORM;
		$audio_bitrate=<STDIN>;
		chomp($audio_bitrate);
		if ( $audio_bitrate eq "" )
		{
			$audio_bitrate=96;
			last;
		}
	}
	open (CONF,">>tmp/vob2divx.conf");
	print CONF "#a_bitrate:".$audio_bitrate."\n";
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
                        if ( $_ =~ m,#addlogo:([^#^ ]*),) {$addlogo=$1;last SWITCH;}
                        if ( $_ =~ m,#beginlogo:([^#^ ]*),) {$beginlogo=$1;last SWITCH;}
                        if ( $_ =~ m,#poslogo:([^#^ ]*),) {$poslogo=$1;last SWITCH;}
                        if ( $_ =~ m,#movietitle:([^#^ ]*),) {$dvdtitle=$1;last SWITCH;}
                        if ( $_ =~ m,#subtitle:([^#^ ]*),) {$sub_title=$1;last SWITCH;}
                        if ( $_ =~ m,#ppdintl:([^#^ ]*),) {$ppdintl=$1;last SWITCH;}
                        if ( $_ =~ m,#audiochannel2:([^#^ ]*),) {$ac2=$1;last SWITCH;}
                        }
                }
	close(CONF);
}
	print $DEBUG " VOBPATH :".$vobpath."\n";
	print $DEBUG " PARAMS :".$params."\n";
	print $DEBUG " FILESIZE:".$filesize."\n";
	print $DEBUG " CLUSTER :".$CLUSTER."\n";
	print $DEBUG " a_bitrate :".$audio_bitrate."\n";
	print $DEBUG " movietitle :".$dvdtitle."\n";
	print $DEBUG "<--- readconf\n";
}

# ********************* Get Needed parameters ************
sub get_params
{  	print $DEBUG "--->  Enter get_params\n";
	if ( ! -e "tmp/vob2divx.conf")
        {
		$i = 0;
        	if ( $ARGV[$i])
        	{       $vobpath = $ARGV[$i];
                	$i ++;
        	} else
                {       system ("echo \"$readme\" | more ");
			if ( $DVDTITLE eq "" ) { print $urldvdtitle;}
                        exit(1);
		}
        	if (! -e $vobpath)
        	{       print $RED;
			print("Path: ".$vobpath." does not exist.\n");
			print $NORM;
                	exit(1);
        	}
		chk_wdir;
		mkdir ("tmp",0777);
		if ($ARGV[$i] > 1)
        	{       $filesize = $ARGV[$i];
			$i++;
        	}else
		{
			print("Please supply filesize \n\t or \"sample\" if you want t
o create samples for cropping.\n\n");
                        exit(1);
                }
	}
	else
	{   
		readconf;
	}

	$audio_bitrate=96 if ( ! defined($audio_bitrate));	
	ask_clust if (! defined($CLUSTER) );
				
#   For Quick mode only .....	

	if ( ! defined($params) ) 
	{
		vobsample;
		movrip;
		$dvdtitle=movie if ( ! defined ($dvdtitle));
		findclip;
		get_audio_channel if ( ! defined($audio_channel));
		interlaced;
		if ( $INTERLACED eq yes )
		{
			$PP=`transcode -J pp=lb -c 9-11  2>&1 | grep failed`;
                	if ( $PP eq "" )
			{	
				$ppdintl=",pp=lb";
			}else
			{
				$params=" -I 3 ";
			} 
		}
		$params .= "-a ".$audio_channel." -j ".$tb.",".$lr;
	}

	if ( ! defined($addlogo) && -e $LOGO )
	{
		$LG=`transcode -J logo=help -c 9-11  2>&1 | grep rgbswap`;
        	if ( $LG ne "" )
		{
			$addlogo=$deftimelogo;
			$poslogo=$defposlogo;
			$beginlogo=$defbeginlogo;  
		}else{
			print $RED."Transcode is not compile with ImageMagick.\nUnable to encode your Logo ".$LOGO."\n".$NORM;
		}
	}

# End Quick mode Configuration
	

	$dvdtitle=movie if ( ! defined($dvdtitle));

	open(CONF,">tmp/vob2divx.conf");
	if ( defined($vobpath) ) {print CONF "#vobpath:".$vobpath." # DO NOT MODIFY THIS LINE\n";}
	if ( defined($last_sec) ) { print CONF "#endtime:".$last_sec."\n";}
	if ( defined($audio_bitrate) ) {print CONF "#a_bitrate:".$audio_bitrate."\n";}
	if ( defined($filesize) ) { print CONF "#filesize:".$filesize."\n";}
	if ( defined($addlogo) ){ print CONF "#addlogo:".$addlogo." # THIS VALUE IS THE DURING TIME OF LOGO (in sec.)\n";}
	if ( defined($beginlogo) ){ print CONF "#beginlogo:".$beginlogo."\n";}
	if ( defined($poslogo) ){ print CONF "#poslogo:".$poslogo."\n";}
	if ( defined($dvdtitle) ){ print CONF "#movietitle:".$dvdtitle." # DO NOT MODIFY THIS LINE\n";}
	if ( defined($params) ) { print CONF "#params:".$params."# YOU MUST KNOW WHAT YOU DO IN THIS LINE\n";}
	if ( defined($CLUSTER) ) { print CONF "#clustermode:".$CLUSTER." # YOU CAN REMOVE BUT NOT MODIFY THIS LINE\n";}
	if ( defined($sub_title) ) {print CONF "#subtitle:".$sub_title."\n";}
	if ( defined($ppdintl) ) {print CONF "#ppdintl:".$ppdintl."\n";}
	if ( defined($ac2) ) {print CONF "#audiochannel2:".$ac2."\n";}
	close(CONF);

	print $DEBUG "<--- get_params\n";
}
###################### Audio Input format ###################
sub audioformat
{
	print $DEBUG "---> Enter audioformat\n";
	@_[0] ="-a 0" if (! defined(@_[0]));
	$audio_format=`tcprobe -i $vobpath/$sample 2> /dev/null `;
	( $audio_format =~ m,audio track: @_[0] [^n]*n 0x(\d+) .*,) or die $RED."Unable to find audio channel ".@_[0]." format\n".$NORM;
	my $tmp=$1;
	SWITCH: 
	{
	if ( $tmp == 2000 ) {  $audio_format=ac3 ; last SWITCH;}
	if ( $tmp == 50 ) {  $audio_format=mpeg2ext ; last SWITCH;}
	if ( $tmp == 10001 ) {  $audio_format=lpcm ; last SWITCH;}
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
			last SWITCH;
		}
	}
	die $RED."Unable to find a known audio format".$NORM;
	}
	print $INFO "\t Audio channel format of ".@_[0].": ".$audio_format."\n";
	print $DEBUG "<--- Audio_format\n";
}

#******************* Make Audio sample **************
sub make_sample
{
	#print @_[0];
	@_[2] = 100 if (! defined(@_[2]));
	$prefix = @_[1];
	$sys = "transcode -q 0 -i ".$vobpath."/".$sample." ".@_[0]." -w 100,".@_[2]." -c 0-".@_[2]." -o ".$prefix.".avi 2> /dev/null";
	print $INFO $sys."\n";
	system ("nice -".$nice." ".$sys);
	@actmp=split / /,@_[0];
	audioformat ("-a ".@actmp[6]);
}

sub ask_filesize
{
                print $GREEN."\n Enter the maximal avifile size (in MB) : ".$NORM;
                $filesize=<STDIN>;
                chomp($filesize);
                open(CONF,">>tmp/vob2divx.conf");
                print CONF "#filesize:".$filesize."\n";
                close(CONF);
}


sub ask_logo
{
	$LG=`transcode -J logo=help -c 9-11  2>&1 | grep rgbswap`;
	if ( $LG ne "" )
	{
		if ( -r $LOGO )
		{       
			print $GREEN."Do you want to add the Logo ".$LOGO." at the beginning of this movie ? (y/n)".$NORM;
			$rep= <STDIN>;
			chomp($rep);
			if ( $rep ne "N" && $rep ne "n" )
			{
				print $GREEN."How long (in sec.) after the movie beginning must your Logo be displayed (see your ~/.vobdivxrc for default)[".$defbeginlogo."]: ".$NORM;
				$beginlogo=<STDIN>;
				chomp($beginlogo);
				$beginlogo=$defbeginlogo if ( $beginlogo eq ""  ) ;
				print $GREEN."How long (in sec.) should your Logo be displayed (see your ~/.vobdivxrc for default)[".$deftimelogo."]?".$NORM;
				$addlogo=<STDIN>;
				chomp($addlogo);
				$addlogo=$deftimelogo if ( $addlogo eq "" || $addlogo == 0 ) ;
				print $GREEN."Where do you want to put this Logo( 1=TopLeft,2=TopRight,3=BotLeft,4=BotRight,5=Center , see your ~/.vobdivxrc for default)[".$defposlogo."]: ".$NORM;
				$poslogo=<STDIN>;
                                chomp($poslogo);
				$poslogo=$defposlogo if ( ! ($poslogo =~ m,[12345],));
                       	}else
                       	{
                               	$addlogo=0;
                       	}
               	}else{
                       	print $RED;
                       	print " If you want to add a Logo at the beginning of this movie \n You must modify the \$LOGO variable, which point to your iamge file (actually $LOGO), in ~/.vob2divx \n";
			print $NORM;
			$junk=<STDIN>;
			$addlogo=0;
               	}
       	} else
       	{
			print $RED."Transcode is not compile with ImageMagick... Unable to encode your Logo ".$LOGO.$NORM."\n" if  ( -r $LOGO );
             		$addlogo=0;
       	}      
	open(CONF,">>tmp/vob2divx.conf");
	print CONF "#addlogo:".$addlogo." # THIS VALUE IS THE DURING TIME OF LOGO (in sec.)\n";
	print CONF "#poslogo:".$poslogo."\n" if ( defined($poslogo));
	print CONF "#beginlogo:".$beginlogo."\n" if ( defined($beginlogo));
	close(CONF);
}
# END ask_logo


################### Evaluate the Zoom..........
sub zooming
{
	print $DEBUG "---> Enter Zooming\n";
# We need the Bitrate to calculate new image Size for the quality_ratio
	calculate_bitrate;
#       We need also the Frame rate $FPS
	calculate_nbrframe if ( ! defined ($nbr_frames));

	$probe = `tcprobe -i  $vobpath/$sample 2> /dev/null `;

	($probe =~ m,import frame size: -g (\d+)x,) or die $RED."Unable to find Width image size\n".$NORM;
	$Xaxis=$1;

	( $probe =~ m,import frame size: -g \d+x(\d+).*,) or die $RED."Unable to find Hight image size\n".$NORM;
	$Yaxis=$1;

	( $probe =~ m,aspect ratio: (\d+):(\d+).*,) or die $RED."Unable to find Image Aspect ratio\n".$NORM;
	$aspect_ratio=$1/$2;
	@tmp=split /-j /,$params;
    @tmp=split / /,@tmp[1];
    @clip=split /,/,@tmp[0];
	$tb=@clip[0];
	$lr=@clip[1];


# The Quality ratio is bigger whitout letter box->image size is smaller ..
# normal .. no ?
	if ( $Yaxis-2*$tb > 0 && $Xaxis-2*$lr > 0 )
	{
		$quality_ratio=$quality_ratio*$Yaxis*$Xaxis/(($Yaxis-2*$tb)*($Xaxis-2*$lr));
	}else
	{	print $RED;
		print "Something crazy !! Your image has a null or negative Size?";
		print " Are you trying holographique movie ;-)?\n transcode is bad to do that ....";
		print $NORM;
		exit(1);
	}
	printf $INFO ("\t Quality Ratio is: %.2f \n",$quality_ratio) if ( ! -e "tmp/cluster.args" );

# New Width Image = SQRT (Bitrate * aspect / QualityRatio x FPS )
	$NXaxis=sqrt(1000*$bitrate*$aspect_ratio/($quality_ratio*$FPS));
# Finale Image MUST have a multiple of 16 size
	$NXaxis=16*floor($NXaxis/16);
# Limits 	
	$NXaxis = $Xaxis-2*$lr if ( $NXaxis > $Xaxis-2*$lr );
	$NXaxis = 720 if ( $NXaxis > 720 );
	$NXaxis = 320 if ( $NXaxis < 320);

#                       New Height
# Finale Image MUST have a multiple of 16 size
	$NYaxis=16*floor((($NXaxis*(1-2*$tb/$Xaxis))/$aspect_ratio)/16);
# Limits but normally impossible to fall into
	$NYaxis=$Yaxis if ( $NYaxis > $Yaxis );
	$row=16;
#  zH zW and row are the -B parameters
 	$zH=floor(($Yaxis-2*$tb-$NYaxis)/$row);
	$zW=floor(($Xaxis-2*$lr-$NXaxis)/$row);

	if ( ($Xaxis - 2*$lr)/16 == floor(($Xaxis - 2*$lr)/16) && ($Yaxis - 2*$tb)/16 == floor (($Yaxis - 2*$tb)/16) )
	{
		print $RED."NB: Because the cripped image size is a multiple of 16, using the Slow Zooming is not necessary\n".$NORM if ( ! -e "tmp/cluster.args");
		print $INFO "\t Original image Size= ".$Xaxis."x".$Yaxis."\n";
		print $INFO "\t New image Size= ".$NXaxis."x".$NYaxis."\n";
		$Zoom_mode="B";
	}else{
		print $RED."WARNING : Clipped Image size is not a multiple of 16 .. You MUST use the Slow Zooming\n".$NORM if ( ! -e "tmp/cluster.args");
                $Zoom_mode="Z";
	}

	print $DEBUG "<--- Zooming\n";
}	


# ********************** Config ****************************

sub config
{       print $DEBUG "--->  Enter config\n";
	if ( -e "tmp/vob2divx.conf") 
	{ 	print $GREEN."May I remove the old tmp/vob2divx.conf ? (Y/N): ".$NORM;
		$rep=<STDIN>;
		chomp($rep);
		unlink("tmp/vob2divx.conf") if ( $rep eq "O" || $rep eq "o" || $rep eq "y"  || $rep eq "Y" );
		system("clear");
	}
	$vobpath = $ARGV[0];

	if ( ! -e $vobpath )
	{ 	print STDOUT "Directory \"".$vobpath."\" does not exist \n Sorry \n";
		exit(1);
	}
	chk_wdir;
	mkdir ("tmp",0777);

	open(CONF,">>tmp/vob2divx.conf");
	print CONF "#vobpath:".$vobpath." # DO NOT MODIFY THIS LINE\n";
	close(CONF);

	movrip;
	vobsample;

	print ("-> Using \"".$sample."\" to create low quality samples \n");
	print $GREEN."\n Have a look with ".$XINE." on the Vob File ".$lastvob." to get the aspect.\n Look also how long (in seconds) is the movie after the END (so we can remove the end credits), you may also decide the audio stream number and the subtitle number if you want.\n".$NORM;
	print ("Press Enter -> ");
	$junk=<STDIN>;
	system ($XINE." ".$vobpath."/".$lastvob."; clear");
#
# How many second remove from end of movie...
	print $GREEN." How long in seconds are the end credits (so transcode will not encode this to have better bitrate) ? ".$NORM; 
        $last_sec=<STDIN>;
        chomp($last_sec);

        $last_sec=0 if ( $last_sec eq "" || $last_sec < 10 );

	open(CONF,">>tmp/vob2divx.conf");
	print CONF "#endtime:".$last_sec."\n";
	close CONF;

#
#############################SOUND SAMPLE###########################################
	$as=10;
	get_audio_channel;
	print $GREEN;
       	print STDOUT "Do you want to make Sound samples to find which audio channel is the one you want ?(Y/N) ";
       	print $NORM;
       	$rep=<STDIN>;
       	chomp($rep);
	if ( $rep eq "o" ||   $rep eq "O" ||  $rep eq "y" ||  $rep eq "Y" )
	{
		for ($i = 0; $i <= 6; $i ++)
       		{
               		make_sample("-x vob -y ".$DIVX." -V -a ".$i." ", "audio_sample._-a_".$i."_", $audiosample_length);
               		print $GREEN."Ear this audio sample, please\n".$NORM;
               		print "Press Enter ->";
               		$junk=<STDIN>;
               		system($AVIPLAY." audio_sample._-a_".$i."_.avi");
               		print $GREEN."Was the Sound Good ?(Y/N): ".$NORM;
               		$rep= <STDIN>;
               		chomp($rep);
               		if ($rep eq "O" or $rep eq "o" or $rep eq "y" or $rep eq "Y")
               		{     
               			unlink("audio_sample._-a_".$i."_.avi");
               			$good_audio="-a ".$i;
               			$as=$i;
               			last;
               		}
               		unlink("audio_sample._-a_".$i."_.avi");
             	}
       	 }
	while ( ! grep (/$as/,@achannels) )
        {
               	print $GREEN."By default audio stream ".$audio_channel." is encoded. If you want another, enter audio stream number: ".$NORM;
               	$as=<STDIN>;
               	chomp($as);
               	$as = $audio_channel if ( $as eq "" );
		print $RED.$as." : is not an available audio channel.\n".$NORM if  ( ! grep (/$as/,@achannels) );
         }
	$audio_channel=$as;
	$good_audio="-a ".$audio_channel;
	system("touch audio_sample._-a_".$audio_channel.".avi");
	if ( $number_of_ac > 0 )
	{
		print $GREEN."Do you want to have another audio channel in your AVI movie (take care of the Video quality which decrease with 2 audio channels for the same movie size), this audio channel will be encoded at the same bitrate than the first audio channel (Y/N): ".$NORM;
		$rep=<STDIN>;
		chomp($rep);
		if ( $rep eq "y" || rep eq "O" ||  $rep eq "o" ||  $rep eq "Y" )
		{	
			print $GREEN."Enter the other audio channel number you want : ".$NORM;
			$ac2=<STDIN>;
			chomp($ac2);
			open (CONF,">>tmp/vob2divx.conf");
			print CONF "#audiochannel2:".$ac2."\n";
			close(AC2);
		}
	}
# audio bitrate
	a_bitrate;


####################################### CROPPING TOP/BOTTOM #######################
	system("clear");
	findclip;
	print $GREEN."Clipping Top/Bottom \n You must have the smallest black LetterBox at top/bottom \n (It's better to leave black LetterBox at top/bottom if you intend to have SubTitle)\n".$NORM;
	print "Press Enter -->";
	$rep=<STDIN>;
	system("/bin/rm video_s._-j_*.ppm 2> /dev/null ");
	$inc=8;
	while ( $rep ne "O" &&  $rep ne "o" && $rep ne "y" && $rep ne "Y" )
	{
		$sys="transcode -q 0 -z -k -i ".$vobpath."/".$sample." -j ".$tb.",".$lr."  -x vob,null -y ppm -c 10-11 -o video_s._-j_".$tb.",".$lr."_";
		printf $INFO $sys."\n";
        	system ($sys."  > /dev/null");
		$tmp = `/bin/ls -1 video_s._-j_${tb},${lr}_*.ppm`;
		@aclip = split /\n/, $tmp;
		foreach $file ( @aclip  ) { system ($XV." ".$file) }
		print $GREEN."Are Top/Bottom LetterBoxes OK ?(Y), to big (B) or to small (S): ".$NORM;
		$rep= <STDIN>;
		chomp($rep);
		if ($rep eq "O" or $rep eq "o" or $rep eq "y" or $rep eq "Y")
		{	
			system("rm video_s._-j_".$tb.",".$lr."_*.ppm" );
			$top_bot=1;
			last;
		}elsif ( $rep eq "S" || $rep eq "s"  )
		{
			system("rm video_s._-j_".$tb.",".$lr."_*.ppm");
			$top_bot=0;
			$tb=$tb-$inc;
			if ( $tb < 0 ) { $tb = 0;}
		}elsif  ( $rep eq "B" || $rep eq "b"  )
		{
			system("rm video_s._-j_".$tb.",".$lr."_*.ppm" );
			$top_bot=0;
			$tb=$tb+$inc;
		}
	}


#######################################CROPPING LEFT RIGHT #########################
	print $GREEN."Now Clipping Left/Right \n".$NORM;
	print "Press Enter -->";
        $rep=<STDIN>;
	$inc=8;
	while ( $rep && "O" &&  $rep ne "o" && $rep ne "y" && $rep ne "Y" )
	{
		$sys="transcode -q 0 -z -k -i ".$vobpath."/".$sample." -j ".$tb.",".$lr." -x vob,null -y ppm -c 10-11 -o video_s._-j_".$tb.",".$lr."_";
		print $INFO $sys."\n";
       		system ($sys." > /dev/null");
		$tmp = `/bin/ls -1 video_s._-j_${tb},${lr}_*.ppm`;
		@aclip = split /\n/, $tmp;
		foreach $file ( @aclip  ){system ($XV." ".$file)}
		print $GREEN."Are Left/Right LetterBoxes OK ?(Y), to big (B) or to small (S): ".$NORM;
		$rep= <STDIN>;
		chomp($rep);
		if ( $rep eq "O" || $rep eq "o" or $rep eq "y" or $rep eq "Y" )
		{	
			system("rm video_s._-j_".$tb.",".$lr."_*.ppm");
			$left_right=1;
			last;
		}elsif ( $rep eq "B" || $rep eq "b"  )
		{
			system("rm video_s._-j_".$tb.",".$lr."_*.ppm");
			$left_right=0;
			$lr=$lr+$inc;
		}elsif  ( $rep eq "S" || $rep eq "s"  )
		{
			system("rm video_s._-j_".$tb.",".$lr."_*.ppm");
			$left_right=0;
			$lr=$lr-$inc;
			if ( $lr < 0 ) { $lr = 0 ;}
		}
        }

#################################SUBTITLE########################################
	$st=10;
	$SUBT=`tcprobe -i $vobpath -H 15 2> /dev/null`;
	if ( ($SUBT =~ m,detected \((\d+)\) subtitle,))
	{      
		print $RED." Transcode has detected ".$1." subtitle(s)\n".$NORM;
		$number_of_st=$1;
	}
		print $GREEN."Do you want subtitle ?(Y/N) ".$NORM;
		$rep= <STDIN>;
		chomp($rep);
		if ($rep eq "O" or $rep eq "o" or $rep eq "y" or $rep eq "Y")
		{
			while ( $st >= $number_of_st )
			{
				print $GREEN."By default SubTitle 0 is encode, enter SubTitle number if you want other ( MAX = ".$number_of_st."): ".$NORM;
				$st=<STDIN>;
				chomp($st);
				$st = 0 if ( $st eq "" );
			}
			$sub_title=",extsub=".$st.":".$tb.":0:1:0:0:255";
			open (CONF,">>tmp/vob2divx.conf");
			print CONF "#subtitle:".$sub_title."\n";
			close (CONF);
		}


####################     ANTIALIASING & DEINTERLACING   ##################"
	interlaced;
 	print $GREEN."Does your clip need Deinterlacing (slower :-( ? ".$NORM;
       	$rep= <STDIN>;
       	chomp($rep);

        if ( $rep eq "O" || $rep eq "o" || $rep eq "y" || $rep eq "Y" )
        {	
		$PP=`transcode -J pp=lb -c 9-11  2>&1 | grep failed`;
		if ( $PP eq "" ) 
		{
			print $GREEN."Vob2divx has detected that your transcode is compiled with the mplayer postprocessor which include a deinterlacing filter much more faster than the builtin deinterlacing filter\n Do you want to use the Mplayer pp filter ?(Y/N) ".$NORM;
        		$rep= <STDIN>;
        		chomp($rep);
       			if ( $rep eq "O" || $rep eq "o" || $rep eq "y" || $rep eq "Y" )
			{	 $ppdintl=",pp=lb";
				 open (CONF,">>tmp/vob2divx.conf");
				 print CONF "#ppdintl:".$ppdintl."\n";
				 close (CONF);
			}else{
	 			$dintl=" -I 3";
#Sorry, only RGB input allowed for now: $dintl="_-J_smartdeinter=diffmode=2:highq=1:cubic=1";
			}
        	}else
		{ 	print "Vob2divx has detected that your transcode is NOT compiled with the mplayer postprocessor which include a deinterlacing filter much more faster than the builtin deinterlacing filter\n";
			$dintl=" -I 3";
# Sorry, only RGB input allowed for now: $dintl="_-J_smartdeinter=diffmode=2:highq=1:cubic=1";
		}
	}

        print $GREEN."Does your clip need Antialiasing (slower :-( ? ".$NORM;
        $rep= <STDIN>;
        chomp($rep);

	$aalias=" -C 3" if ( $rep eq "O" || $rep eq "o" || $rep eq "y" || $rep eq "Y" );


#    Ask for a Logo
	ask_logo;
		
#
#     Search DVD Title (may be detect thrue movrip ...
	if ( ! defined($dvdtitle))
	{	print $GREEN."Enter the title of this movie (blank space available): ".$NORM;
		$dvdtitle=<STDIN>;
		$dvdtitle =~ s/ /_/g;
		chomp($dvdtitle);
		if ( $dvdtitle eq "" ) { $dvdtitle="movie";}
	}
	open (CONF,">>tmp/vob2divx.conf");
        print CONF "#movietitle:".$dvdtitle." # DO NOT MODIFY THIS LINE\n";
	close(CONF);

#
#	Write parameters
	( $left_right eq 1 && $top_bot eq 1 ) or die $RED."Oups Sorry.. I miss some parameters :-( n\n".$NORM;
	$params = $good_audio." -j ".$tb.",".$lr.$dintl.$aalias;
	open (CONF,">>tmp/vob2divx.conf");
	print CONF "#params:".$params." # YOU MUST KNOW WHAT YOU DO IN THIS LINE\n";
	close(CONF);
}   # END Config 

######################### RIP A DVD ######################################
sub ripdvd
{ 	
	$vobpath = $ARGV[0];
	( -e $vobpath ) or die $RED."Directory \"".$vobpath."\" does not exist \n Sorry\n".$NORM;
	print $GREEN."On which device is your DVD (to detect the main title)[ default: /dev/dvd] ?".$NORM ;
	$dvd=<STDIN>;
	chomp($dvd);
	$dvd="/dev/dvd" if ( $dvd eq "" ); 
	if ( $DVDTITLE ne "" )
	{ 
		$dvdtitle=`$DVDTITLE $dvd`;
		chomp($dvdtitle);
	}else
	{  	
		print $GREEN."Vob2divx does'nt find dvdtitle, please enter this DVD Movie Title: ".$NORM;
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
	$probe = `tcprobe -i \"$dvd\" 2>&1`;
	($probe =~ m,DVD title \d+/(\d+),) or die $RED."Probing DVD failed! - No DVD?\n".$NORM;
	$totalTitles = $1;
	print " titles: total=$totalTitles\n";

	@checkTitles = 1 .. $totalTitles;
# now probe each title and find longest
	$longestLen   = 0;
	$longestTitle = 0;
	for(@checkTitles) {
  # call tcprobe for info
		  $probe = `tcprobe -i \"$dvd\" 2>&1 -T $_`;
  # extract title playback time -> titlelen
  		($probe =~ m,title playback time: .* (\d+) sec,) or die $RED."No time found in tcprobe for title $_ !\n".$NORM;
 		 $titleLen[$_] = $1;
  # extract title set (VTS file) -> titleset
  		($probe =~ m,title set (\d+),) or die $RED."No title set found in tcprobe for title $_!\n".$NORM;
  		$titleSet[$_] = $1;
  # extract angles
  		($probe =~ m,(\d+) angle\(s\),) or die $RED."No angle found in tcprobe for title $_ !\n".$NORM;
  		$angles = $1;
  # extract chapters
  		($probe =~ m,(\d+) chapter\(s\),) or die $RED."No chapter found in tcprobe for title $_ !\n".$NORM;
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
	print "The Main Title seems to be the Title No : $longestTitle, OK ? (y/n) :";
	print $NORM;
	$rep=<STDIN>;
        chomp($rep);
	if ( $rep ne "o" &&  $rep ne "O" && $rep ne "y" && $rep ne "Y" ) 
	{ 	
	    print "Ups... Enter the Title number please : ";
	    $longestTitle=<STDIN>;
	    chomp($longestTitle);
	}
#  Check if this title is multiangle ....
	$probe = `tcprobe -i \"$dvd\" 2>&1 -T $longestTitle`;
	($probe =~ m,(\d+) angle\(s\),) or die $RED."No angle found in tcprobe for title $_ !\n".$NORM;
        $angles = $1;
	if ( $angles > 1 )
        { 	print $RED;
                  print "***************** WARNING!!!! *********************\n\t This is a multi angles video stream. \n";
#A known bug in transcode doesn't permit to encode this type of stream correctly.\n You may try the experimental \"-i ".$dvd." -x dvd -T".$longestTitle.",-1\" option, in place of the usual \" -i ".$vobpath." -x vob\", to encode this\n";
		print"\n\n\n Do you want always rip this DVD ?(Y/N): ";
                print $NORM;
		$rep=<STDIN>;
        	chomp($rep);
        	if ( $rep ne "o" &&  $rep ne "O" && $rep ne "y" && $rep ne "Y" )
        	{
			print "OK .. Bye\n";
                	exit(1);
                }
		print $GREEN;
		print "OK ... we continue ...\n";
		print "There is ".$angles." which one do you want ? :";
		print $NORM;
                $angle=<STDIN>;
                chomp($angle);
	}else{
		$angle=1;
	}
#	
	system("/bin/rm -f ".$vobpath."/*  2> /dev/null");
        open (TITLE,">".$vobpath."/dvdtitle");
	print TITLE $dvdtitle;
	close(TITLE);
	chdir($vobpath);
	$sys="tcprobe -i ".$dvd." -T ".$longestTitle."  >> probe.rip 2>&1 ";
    system ("nice -".$nice." ".$sys);
	$sys="tccat -i /dev/dvd -T ".$longestTitle.",-1,".$angle." | split -b 1024m - ".$dvdtitle."_T".$longestTitle."_" ;
	print ($sys."\n");
	system("nice -".$nice." ".$sys);
	opendir(VOB,$vobpath);
	my(@files)=grep { /$dvdtitle/ && -f "$_" } readdir(VOB);
 	foreach $vob ( @files){rename($vob,$vob.".vob");}
	print $GREEN." OK, your vob Files are now in ".$vobpath."\n";
	print "You may now run vob2divx with yours arguments to encode the vob file(s)\n\n".$NORM;
	exit(0);
} # END ripdvd


################################## MAIN () #####################################

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
                chdir($wdir) or die "$wdir does not exist or is'nt a directory\n ";		
		readconf;
                aviencode;
                exit(0);
        }else{
                  print "Error: Why run vob2divx with runclust option ?\n";
        }
# We do never come here !
exit(1);
}
if ( $ARGV[0] eq "-v" )
{
        print "Vob2divx release ".$release."\n";
        exit(0);
}

if ( $ARGV[0] eq "-h" || $ARGV[0] eq "--help" )
{
        system (" echo \"$usage\" | more ");
        if ( $DVDTITLE eq "" ) { print $urldvdtitle ;}
        exit(0);
}

if ($ARGV[1] eq "rip" )
{ ripdvd;
}	

if ($ARGV[1] eq "continue" || $ARGV[0] eq "continue" || ! defined($ARGV[0]))
{	unlink("tmp/cluster.args");
	get_params;
}  

if (defined ($ARGV[0]) && $ARGV[0] ne  "continue" && ! defined($ARGV[1]))
{ 
	system (" echo \"$usage\" | more ");
  exit(1);
}

if ( $ARGV[1] > 1 && $ARGV[1] ne "sample" )
{
unlink("tmp/vob2divx.conf");
get_params;
}

if (1)
{
	aviencode;	
	merge;
# We do never come here !
	exit(1);
}
-------
