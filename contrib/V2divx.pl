#!/usr/bin/perl  -w
# $Revision: 1.2 $
# vi: formatoptions=croq:expandtab:sts=4:sw=4 :
#
# AUTHORS :     Roland SEUHS <rolandATwertkarten.net>
#               Dominique CARON <domiATlpm.univ-montp2.fr> 
#		Tom ROTH <tomATtomroth.de>
#		Luis Mondesi <lemsx1AThotmail_dot_com>
# GPL LICENSE 
# http://www.gnu.org/licenses/gpl.html
#
#
# WEB :         http://v2divx.sourceforge.net
#
# TODO:
#   SEE TODO FILE
#   
#  HACKING: 
#   SEE HACKING FILE
#
#  BUGS:
#   * unable to encode logo in cluster mode if video input is NTSC vob.
#   * due to keyframes and the V2divx method (transcode itself is unable
#       to encode logo in cluster mode) the start time of logo in
#       cluster mode is average. In fact this is the time of the removed
#       begin (and end) credits which are average. So, if removed begin 
#       credits is 0 sec, the asked start time of logo is respected.
#   * transcode doesn't work with kernel 2.6.0 (not yet)

use strict;
use POSIX;
use Env qw(HOME);
use FileHandle;
use File::Temp qw/ tempfile  /;     # Perl 5.6.1: 
                                    # used to get secured temp file
STDOUT->autoflush(1);

# get options. Most of the V2divxrc options (if not all)
# are overriden by this command line ones

use Getopt::Long;
Getopt::Long::Configure('bundling');

# get options
# declare empty variables
# flags
my ($HELP,$VERSION,$RUNCLUST,$SRTSUBRIP,$OKCLUSTER,$TEST) = 0;
# others
my $SVIDEO_FORMAT="";
my $ROOT_DIRECTORY = "";
my $TVREC=0;
# userconf
my $nice="";
my $DIVX="";
my $DIVX_OPT="";
my $XV="";
my $XINE="";
my $CLUSTER_CONFIG="";
my $XTERM="";
my $RMCMD="";
my $AVIPLAY="";
my $LOGO="";
my $POSLOGO="";
my $STARTLOGO="";
my $TIMELOGO="";
my $LANGUAGE="";
my $DEF_AUDIOCHANNEL="";
my $AO="";
my $DEBUG=0;
my $INFO=0;
my $EXTSUB ="";
my $Res_DIVX_OPT=0;
GetOptions(
    # flags
    'h|help'                    =>  \$HELP,
    'v|version'                 =>  \$VERSION,
    'runclust'                  =>  \$RUNCLUST,
    'srtsubrip'                 =>  \$SRTSUBRIP,
    'tv'                        =>  \$TVREC,
    'd|debug!'                  =>  \$DEBUG,
    'i|info!'                   =>  \$INFO,
    'c|with-cluster'            =>  \$OKCLUSTER,
    't|test'                    =>  \$TEST,
    # strings
    'D|directory=s'             => \$ROOT_DIRECTORY,
    's|source-video-format=s'   => \$SVIDEO_FORMAT,
    'x|x_video=s'               => \$DIVX,
    'F|x_video_opts=s'          => \$DIVX_OPT,
    'o|output_audio=s'          => \$AO,
    'V|video_player=s'          => \$XINE,
    'A|avi_player=s'            => \$AVIPLAY,
    'I|image_viewer=s'          => \$XV,
    'C|cluster_config=s'        => \$CLUSTER_CONFIG,
    'r|remote_cmd=s'            => \$RMCMD,
    'L|logo=s'                  => \$LOGO, 
    'l|lang|language=s'         => \$LANGUAGE, 
    'S|SUB_TITLE_opts=s'        => \$EXTSUB,
    # numbers
    'a|default_audio_channel=i' => \$DEF_AUDIOCHANNEL ,
    'n|nice=i'                  => \$nice
);
# reset DIVX_OPT if DIVX is pass as parameter and not DIVX_OPT
$Res_DIVX_OPT=1 if ( $DIVX ne "" && $DIVX_OPT eq "" );

# set debugging or info mode
#

my $userconfigfile="$HOME/.V2divxrc";

my $userconfig="
#		This file is use by V2divx to read your default parameters
#
#		The nice level of transcode ( nice -\$nice transcode ....)
\$nice=10;
#			Choose your preferred encoder
\$DIVX=ffmpeg;	       # ffmpeg is the Transcode include divx encoder but you can use
#  divx4,divx5,xvid,xvid2,xvid3 .... if you have it#			Add your encoder specifics options

#			Add your encoder specifics options
\$DIVX_OPT=-F mpeg4 #  for example '-F mpeg4' for divx ffmpeg output 

#		Image Viewer
\$XV=display;   # you may use \'xv\' 

#     		Video Player
\$XINE=xine; # You may modify your Movie Viewer (xine for example) 

# 		DivX Player
\$AVIPLAY=xine; # You may modify your DivX4 Viewer (mplayer) 
#

#		Cluster config if you intend to use cluster mode
\$CLUSTER_CONFIG=/path/to/.V2divxrc-cluster;  # YOU MUST MODIFY THIS LINE !!!

#               Xterm command (default is 'xterm \%s -e'). \%s is a 
#               series of arguments that will be passed before the
#               execution code '-e' or '-x' according to the given
#               terminal. Xterm requires that all arguments come before
#               -e for instance.
#               You can also set it to things like: gnome-terminal \%s -x, 
#               konsole \%s -x, screen -d -m, etc...
#
\$XTERM=xterm \%s -e ;

# 		remote command if you intend to use cluster mode
\$RMCMD=ssh; 			# change this to rsh if you need

# 	Location of your Image Logo 
# if this file exist the logo will be automatically include when 
# running V2divx /path/to/vob file_size (alias Quick mode ) 
\$LOGO=/where/is/your/logo.img;

#      Your Default Logo Position (1=TopLeft,2=TopRight,3=BotLeft,4=BotRight,5=Center)
\$POSLOGO=4;

#      Default starting time logo ( in second after the movie beginning )
\$STARTLOGO=2;

#     Default Logo Duration ( in second )
\$TIMELOGO=25;

#		Your preferred Language (fr,en,de...) for audio channel encoding 
#			( USE V2divx rip to enable this !!!) 
\$LANGUAGE=en;

# If for some reason V2divx is unable to determine the audio channel 
# for your LANGUAGE, put here the audio channel number
# ( generally 0 is your language, 1 is english, 2 is another ...)
\$DEF_AUDIOCHANNEL=0;   

# Here is the output audio format
# Allowed formats are mp3 or ogg ( if ogmtools are installed )
\$AO=mp3;

# EXT SUBTITLE FILTER 5 LAST OPTIONS (here we just use 3) ...See the docs 8-(
\$EXTSUB=0:0:255;
";

#################### GLOBAL VARIABLES ###################
my $release="2.3.0 (C) 2002-2004 Dominique Caron";
my $v2d="[V2divx]";
my $keyframes = 250;
my $audiosample_length = 1000;
my $PGMFINDCLIP="pgmfindclip"; # New tool of transcode
my $DVDTITLE="dvdtitle";
my $RED="\033[1;31m";
my $GREEN="\033[0;32m";
my $NORM="\033[0;39m";
my $MAJOR=0;
my $MINOR=6;
my $FPS="";         # number of FPS will be detected automatically
my $BITRATE="";     # init by calculate_bitrate() or tv_recorder()
my $RUNTIME=0;      # init by calculate_bitrate()
my $AUDIO_SIZE=0;   # init by calculate_bitrate() or tv_recorder()
my $TOT_FRAMES=0;   # init by calculate_nbrframe()
my $NOAUDIO=0;      # 1=Movie which does not contain audio channel
my $NEED_TIME=0;    # 1=Movie where tcextract is unable to extract info
my $SAMPLE="";      # a chunk of movie for sample
my $REV_VID="-z -k";
my $RGBSWAP=1;
my $VID_OPT="-V";
my $LASTVOB="";
my $Xaxis=0;        # Input Video X axis size
my $Yaxis=0;        # Input Video Y axis size
my $NXaxis=0;       # Output Video X axis size init by zooming()
my $NYaxis=0;       # Output Video Y axis size init by zooming()
my $ASPECT_RATIO=0; # Input aspect ratio init by zooming();
my $Zoom_mode="";   # Zoom mode ( -Z or -B ) init by zooming
my $tb=0;           # Top/Bottom clipping init by zooming()
my $lr=0;           # Left/Right clipping init by zooming()
my $FLAG_GAC=1;     # flag used by get_audio_channel to show only once results

my $node="";
my $EXT="avi"; # extension of final movie ( avi or ogm)
my $clust_percent="--cluster_percentage --a52_dolby_off ";

# --- V2divx.conf Global  vars
my $VOBPATH="";
my $END_SEC=0;
my $START_SEC=0;
my $AUDIO_BITRATE=""; # init in a_bitrate()
# my $AO=""; already define before Getopt
my $FILESIZE="";
my $ADDLOGO="";
my $TITLE="";
my $PARAMS="";
my $SUB_TITLE="";
my $DEINTL="";
my $AC2="";
my $VIDEONORM="PAL";   # Default video format
my $TELECINE="";
my $CLUSTER="";
# For TVrec
my $TV_RUNTIME=0;
my $QUALITY=2;
my $NVREC=0; # By default NVREC is not here
my $OGMTOOL=0; # By default ogmmerge is not here
my $SUBRIP=0; # By default srtsubrip is not here


#  Functions Declarations

# interactive
sub ask_filesize;
sub ask_telecine;
sub ask_logo;
sub ask_clust;
# utilities
sub pinfo;
sub myperror;
sub pdebug;
sub pwarn;
sub smily;
sub get_fps;
sub get_tail;
# system
sub mydie;
sub config;
sub makelogo;
sub audioformat;
sub videoformat;
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
sub ripdvd;
sub readconf;
sub zooming;
sub findclip;
sub interlaced;
sub get_audio_channel;
sub readuserconf;
sub printinfo;
sub chk_wdir;
sub audiorescale;
sub srt_subrip;
sub format_date;
sub sec_to_hour;
sub tv_recorder;
sub ogmerge;
sub check_system;
sub tcsync;

system("clear");

my $start_time = time;
my $f_start_time = format_date($start_time);
pinfo("Start Time: $f_start_time \n");

# Read or create the user configuration file
# we kept .vob2divxrc for backward compatibility
if ( -f $HOME."/.vob2divxrc" )  
{
    rename($HOME."/.vob2divxrc",$userconfigfile);
}

if ( -f $userconfigfile )
{   
    readuserconf;
} else {
    open ( USERCONF,"> $userconfigfile");
    print USERCONF $userconfig;
    close USERCONF;
    readuserconf;
}

my $urldvdtitle=$GREEN."\t V2divx is unable to find dvdtitle in your PATH.\n\t Code Sources of dvdtitle are available at : \n\t http://www.lpm.univ-montp2.fr/~domi/V2divx/dvdtitle.tgz\n ".$NORM."\n";

my $warnclust = "\n ERROR: cluster mode. Please see README file\n\n";

my $usage =
$RED."            *****  Warning  *****$NORM
Please note that you are only allowed to use this program according to fair-use laws which vary from country to country. You are not allowed to redistribute 
copyrighted material. Also note that you have to use this software at your own risk.

------------------------------------------------------
You may want first rip vob files from a DVD :
then use:

% V2divx /path/to/vobs rip
(where /path/to/vobs is the directory where vob files will be ripped)
It is recommended to rip your DVD with V2divx because it save precious informations about the movie to encode. (probe.rip)
---------------------------------------------------

NB: $RED transcode will encode your movie in $DIVX format , to change this,
edit your $userconfigfile and change the \$DIVX variable. $NORM

There are 2 ways of using this program to encode your vob file(s):

1: Easy\n
-----------
% V2divx /path/to/vobs 700
(where 700 is the desired FILESIZE in Megabytes, and /path/to/vobs the directory where are the unencrypted vob files)
This mode (alias Quick mode) take all parameters in your $userconfigfile  which has been created the first time you have run V2divx. 
Take a look in it please.

2: Better\n
----------
% V2divx /path/to/vobs config 
This will ask all what it need to make the movie you want ;-)
( /path/to/vobs is the directory where are the unencrypted vob files).

3) Recording TV\n
----------
% V2divx /path/to/moviefiles --tv
(where /path/to/moviefiles is a directory where V2divx will keep the temporaries movies files.

------------\n
V2divx options (override ~/.V2divxrc variables)
\t -h|--help          show this Help
\t -v|--version       show version
\t --tv               start TV recorder mode (see above)
\t -d|--debug         debug mode
\t -i|--info          info mode ( show transcode commands )
\t -c|--with-cluster  run in cluster mode (V2divx ask for this without this opt)
\t -x|--x-video 'codec'                 encode with this codec
\t -F|--x-video_opts '-F options'       options for the video codec
\t -V|--video_player 'command'          use 'command' to show the input Video  
\t -A|--avi_player 'command'            use 'command' to show avi samples 
\t -I|--image_viewer 'command'          use 'command' to show images samples 
\t -C|--cluster_config 'file'           use 'file' as cluster config file
\t -r|--remote_cmd  'command'           use 'command' as remote commande
\t -n|--nice 'value'                    use 'value' as nice value 
\t -L|--logo 'file'                     use 'file' as logo image file 
\t -l|--lang 'lg'                       use 'lg' as language  (fr,de,es,...)
\t -S|--SUB_TITLE_opts 'value'          use 'value' as extsub transcode filter option
\t -a|--default_audio_channel 'int'     use 'int' as default audiochannel
\t -o|--output_audio 'value'            use 'value' as output audio format (ogg or mp3)


You can interrupt the program anytime. To continue encoding, just run the script
without parameters in the same directory.\n$RED\t You MUST NOT run V2divx from the /path/to/vobs directory.\n$NORM
V2divx v$release
\n\n
";

my $readme="\n See README file or run $0 --help \n\n ";

# You may Modify the Next value but take care
# (see the transcode man pages - about Bits per Pixel)
# it's used to estimate the image size of the encoded clip
my $bpp=0.18;   # This value =  bitrate x 1000 / ( fps x height x width ) 

# ********************* MAIN () ************************ #

if ( defined $ARGV[1] &&  $ARGV[1] eq "config")
{
    # Check tools,lib etc .... on system
    check_system;
    config;
    ask_clust;
}

# We are on a node
if ( $RUNCLUST )
{
    if (defined($ARGV[0]))
    {
    # $ARGV[0] is the dir where user is 
    # working on the master (NOT the $VOBPATH)
    my $wdir=$ARGV[0];
    chdir($wdir) or mydie $wdir." does not exist or isn't a directory";
    # We are on a cluster node all needed parameters ARE known via -->
    readconf;
    chk_wdir; # is needed to know input video format
    # And now we can aviencode  
    zooming;
    aviencode;
    exit(0);
    } else {
        mydie "Error: Why run V2divx with --runclust option ?\n";
    }
    # We do never come here !
    exit(1);
}

# Show release and exit
if ( $VERSION )
{
    print "V2divx v$release\n";
    exit(0);
}

# rip a DVD
if (defined ($ARGV[1]) && $ARGV[1] eq "rip" )
{
    check_system;
    ripdvd;
}

# Record TV
if ( $TVREC )
{
    pdebug ("$0 called with --tv");
    mydie("You want to record TV but it seems that NVrec".
        " is'nt install on this system?\n") if (! $NVREC );
    mydie("There is still a tmp/V2divx.conf , please remove".
        " all tmp files\n (or at least tmp/V2divx.conf) before".
        " running V2divx /path/to/vob --tv") if ( -f "tmp/V2divx.conf") ;
    $VOBPATH= $ARGV[0];
    mydie("Directory \"$VOBPATH\" does not exist \n Sorry") if ( ! -e $VOBPATH);
    pwarn("Warning All files in $VOBPATH will be deleted\n Press Enter");
    my $junk=<STDIN>;
    umask(000);
    mkdir ("tmp",01777);
    check_system;
    tv_recorder;
}

# Make subtitle in a separate file (.srt)
if ( $SRTSUBRIP )
{
    pdebug ("$0 called with --srtsubrip");
    readconf;
    srt_subrip($1,$2) if ( $SUB_TITLE =~ m,SRT_(\d+)_(.*),);
    exit(0);
}

# help or bad V2divx parameters
if (
    ( defined ($ARGV[0])
        && $ARGV[0] ne  "continue"
        && ! defined($ARGV[1]) )
    || $HELP == 1
)
{
    system (" echo \"$usage\" | less -R ");
    exit(1);
}

# No parameters (continue)
if ( defined ($ARGV[1]) && $ARGV[1] eq "continue" 
    || ( defined ($ARGV[0]) && $ARGV[0] eq "continue")
    || ! defined($ARGV[0]))
{
    unlink("tmp/cluster.args");
    # We CONTINUE ....
    get_params;
}

# Quick mode 
if ( defined($ARGV[1])  && $ARGV[1] ne "config" && ! $SRTSUBRIP )
{
    pinfo("$v2d\t Quick Mode:\t\t\t\t   | Yes\n");
    mydie "There is still a tmp/V2divx.conf , please remove".
        " all tmp files\n before running V2divx /path/to/vob".
        " SIZE" if ( -f "tmp/V2divx.conf") ;
    get_params;
}

if (1)
{
    # we are not in cluster mode (quick, config or continue)
    aviencode;
    pinfo("$v2d\t Renaming tmp/2-${TITLE}_0.$EXT tmp/2-${TITLE}_sync.$EXT\n");
    rename("tmp/2-${TITLE}_0.$EXT","tmp/2-${TITLE}_sync.$EXT");
    if ( $AO eq "ogg" )
    {
        pinfo("$v2d\t Merging the ogg audio stream\n") ;
        ogmerge("tmp/2-${TITLE}_sync.$EXT","tmp/ac1.${TITLE}.ogg");
    }
    twoac;
    finish;
    # We do never come here !
    exit(1);
}

######################## FUNCTIONS #########################
sub tcsync
{   # Return the sync fine tunning suggested by tcprobe
    # Regex must be deeper tested
    my $fine_sync="";
    my $sync=`tcprobe -i $VOBPATH 2>&1`;
    my @nsync=split /@_/,$sync;
    $nsync[1]=~m,-D ([^\(]*)\(frame,;
    # WARNING THIS IS NOT ALWAYS VERY GOOD !!!
    $fine_sync="-D $1" if ( $VIDEONORM ne "PAL" ) ;
    return($fine_sync);
}
sub get_tail 
{
    use vars qw($opt_b $opt_c $opt_f $opt_h $opt_n $opt_r);

    # reimplementation of 
    # http://www.perl.com/language/ppt/src/tail/tail.thierry
    # @param 0 str filename
    # @param 1 int number of lines [negative -1 means last line]
    my ($f,$n) = @_;
    my $i = 0;
    my @buf;
    
    open(FH,$f) or pwarn("Could not open $f for reading: $!\n");

    while(<FH>) {
        $i++;
        $buf[$i%(-$n)] = $_;
    }

    my @tail = (@buf[ ($i%(-$n) + 1) .. $#buf ], 
    @buf[  0 .. $i%(-$n) ]);
    @tail = reverse @tail if $opt_r;
    for (@tail) {
        # instead of printing, put in @array and return this
        # instead...
        print if $_; # @tail may begin or end with undef
    }
} # end get_tail

sub get_fps {
    # returns a string with number of frames per second
    # and sets a global $FPS
    my $info=`transcode -i $VOBPATH -c 1-2 2> /dev/null` 
        or mydie "Problem when running \'transcode -i $VOBPATH \'";
    $info =~ m,V:\s+encoding\s+fps.*\|\s+(\d+\.\d+),;
    $FPS = $1;
    chomp( $FPS ); # just in case...
    return $FPS;
}

sub ask_telecine {
    # @param 0 str $FPS := float [25.00 or 23.97 or 29.97 ... ]
    # ask user a yes or no question about whether
    # NTSC is needed and sets proper parameters
    # accordingly. And lastly, returns string $VIDEONORM.
    #
    # Also, we assume NTSC (no telecine) if FPS
    # is 29.970 (pass as argument)
    # example: ask_telecine($FPS);
    print STDOUT (" Is this Movie a NTSC TELECINE movie".
    " (MPlayer is your friend to know that)? [y|N]");
    my $rep=<STDIN>;
    chomp($rep);
    if ( $rep eq "y" || $rep eq "Y" )
    {
        $TELECINE=1 ;
    } else {
        $TELECINE=0;
    }
    pdebug ("video normalization: $VIDEONORM | telecine: $TELECINE");
} # end ask_telecine()

sub check_system
{
    pdebug ("---> Enter check_system");
    # Find the transcode release 
    my $tr_vers=`transcode -v 2>&1 | awk '{print \$2}'| sed s/^v// `;
    my @Vers = split /\./,$tr_vers;
    if (  $Vers[0] < $MAJOR  
        || ( $Vers[0] == $MAJOR 
            && $Vers[1] < $MINOR ) )
    {
        pwarn("This V2divx perl script does not support".
            " your transcode release\n Please upgrade to the latest".
                " transcode release (0.6.11 at least)\n");
                    exit(1);
   }

    # Test if the $DIVX module is OK
    my $module=`transcode -c 2-3 -i /dev/zero -y $DIVX,null $DIVX_OPT 2>&1 | grep 'critical'`;  # Bug (transcode)
    if ( $module ne "" )
    {
        pwarn(" According to your \$DIVX variable, ".
            "you want to encode with $DIVX libraries\n But it ".
            " seems that $DIVX libraries are not installed on ".
            " your system\n Please install it or change ".
            "the \$DIVX variable in your $userconfigfile\n");
        exit(0);
    }
    pinfo("$v2d\t Transcode detected release:\t\t   | $Vers[0].$Vers[1].$Vers[2]\n");

    # Do not check tools on cluster nodes
    if  ( ! $RUNCLUST )
    {
        my $pgm="";
        foreach $pgm ( $XV, $XINE, $AVIPLAY )
        {
            my(@pgm)=split / /,$pgm;
            if ( system("which $pgm[0] > /dev/null 2>&1 ") )
            {
                pwarn(" $pgm is not installed on this System :-( \n Modify your $userconfigfile to reach your configuration (DVD player, DivX player, Image viewer....) \n");
                exit (0);
            }
        }
        $PGMFINDCLIP=(! system("which $PGMFINDCLIP >/dev/null 2>&1 "))?1:0;
    
        $SUBRIP=system("which subtitle2pgm >/dev/null 2>&1 ");
        $SUBRIP=system("which pgm2txt >/dev/null 2>&1 ") if ( $SUBRIP == 0 );
        $SUBRIP=system("which srttool >/dev/null 2>&1 ") if ( $SUBRIP == 0 );
        $SUBRIP=system("which gocr >/dev/null 2>&1 ") if ( $SUBRIP == 0 );
        $SUBRIP=( $SUBRIP == 0 )?1:0;
        $DVDTITLE="" if ( system("which $DVDTITLE >/dev/null 2>&1 ")!=0);
        # are ogmtools on this system ?
        $OGMTOOL=system("which ogmmerge >/dev/null 2>&1 ");
        $OGMTOOL=system("which ogmsplit >/dev/null 2>&1 ") if ( $OGMTOOL == 0 );
        $OGMTOOL=( $OGMTOOL == 0 )?1:0;
        # reset $AO to mp3 if ogmmerge is not found
        if  ( ! $OGMTOOL  && $AO eq "ogg" )
        {
            $AO="mp3";
            pwarn("Audio output format reset to MP3, ogmmerge not found on this system\n (Press Enter)");
            my $junk=<STDIN>;
        }
    
        # Check nvrec
        my $nvrec=`transcode -V -c 0-1 -x nvrec,null -i /dev/null -y $DIVX,null $DIVX_OPT -o /dev/null  2>&1 | grep -i "Unable to detect NVrec version"`;
        $NVREC=1 if ( $nvrec eq "" ); 
    } 

    # set the final movie extension
    $EXT="ogm" if ( $AO eq "ogg");
    pdebug (" <--- check_system");
}   # END check_system

sub ogmerge
{   # this function merge an ogg file and a mpeg4 file
    # the result has the name of the mpeg4 file
    pdebug (" ---> Enter ogmerge");
    my $sys="ogmmerge -o tmp/temp.ogg $_[0] $_[1]";
    my $pid = fork();
    mydie "couldn't fork" unless defined $pid;
    if ($pid)
    {
        system("nice -$nice $sys")==0 
            or  ( system("touch tmp/ogmerge.finish") && mydie "Unable to run\'$sys\'");
        system("touch tmp/ogmerge.finish"); 
        wait;
    } else {
        smily("ogmerge");
    }

    print("$sys && rename tmp/temp.ogg $_[0]\n") if ( $INFO);
    rename("tmp/temp.ogg","$_[0]");
    pdebug (" <--- ogmerge");
}

sub tv_recorder
{
    pdebug ("---> Enter TV Recorder");
    my($heures,$minutes,$w,$sys)="";
    $TVREC=1;
    $FPS=25;
    $CLUSTER="NO";
    # record the TV .
    if ( ! -f "tmp/V2divx.conf")
    {
        print " How many minutes do you want to record ? ";
        $TV_RUNTIME=<STDIN>;
        chomp($TV_RUNTIME);
        print " What is your desired movie file size (in Mo)? ";
        $FILESIZE=<STDIN>;
        chomp($FILESIZE);
        a_bitrate;
        print " Which Quality do you want:\n\t1) High Quality - Which need a VERY big and fast harddisk (~240Mo/mn) and is very long to encode (9fps on a 1.2Ghz CPU)\n";
        print "\t2) Normal Quality - Which is fast and need only 2 * final movie size\n\t (default 2)?:";
        my $QUALITY=<STDIN>;
        chomp($QUALITY);
        $QUALITY=2 if ( $QUALITY eq "" );
        if ( $QUALITY==1 )
        {
            my $diskspace=240 * $TV_RUNTIME/ 1024 ;
            pwarn("\t\t *** WARNING *** \n You need average $diskspace Go to record this Movie\n");
        }
        open (CONF,">>tmp/V2divx.conf");
        print CONF "#TVREC:$TVREC#DO NOT MODIFY THIS LINE\n";
        print CONF "#VOBPATH:$VOBPATH# DO NOT MODIFY THIS LINE\n";
        print CONF "#TV_RUNTIME:$TV_RUNTIME\n";
        print CONF "#FILESIZE:$FILESIZE\n";
        print CONF "#QUALITY:$QUALITY\n";
        print CONF "#CLUSTER:$CLUSTER\n";
        close(CONF);
    }
    ask_logo;
    my $start_frames_logo=floor(($START_SEC+$STARTLOGO)*$FPS); 
    my $end_frames_logo=floor($ADDLOGO*$FPS+$start_frames_logo);
    my $add_logo="logo=file=$LOGO:posdef=$POSLOGO:rgbswap=$RGBSWAP:range=$start_frames_logo-$end_frames_logo," if ( $ADDLOGO );
    $DEINTL="yuvdenoise" if ( $QUALITY==1 );
    $heures=int($TV_RUNTIME/60);
    $minutes=$TV_RUNTIME - $heures * 60;
    $BITRATE=int((139.5 * $FILESIZE)/$TV_RUNTIME - $AUDIO_BITRATE);
    my $NXaxis=512;
    $NXaxis=384 if ( $QUALITY==1);
    my $NYaxis=384;
    $NYaxis=288 if ( $QUALITY==1);
    $TV_RUNTIME=60*$TV_RUNTIME;
    my $AUDIO_SIZE=$AUDIO_BITRATE*$TV_RUNTIME/(8*1024);
    my $fbpp=1000*$BITRATE/($NXaxis*$NYaxis*$FPS);

    my $nbr_frames=$FPS*$TV_RUNTIME;
    print " Your TV channel MUST be selected, you can interrupt recording with\n";
    printinfo;
    system("/bin/rm -rf $VOBPATH/*  2> /dev/null");
    if ( $QUALITY==1 )
    {
       $sys=sprintf("transcode -c 0-%02d:%02d:00  -V -i /dev/video0 -x nvrec=\"-N 32\",null  -y yuv4mpeg,null -g ${NXaxis}x${NYaxis} -u 100 -o $VOBPATH/only_video.mpeg -m $VOBPATH/only_audio.avi -p /dev/dsp -H0 -f %d ",$heures,$minutes,$FPS);
    } else {
        $sys=sprintf("transcode -c 0-%02d:%02d:00  -V -i /dev/video0 -x nvrec=\"-N 32 -deint bob -ddint \",null  -J dilyuvmmx,$add_logo -y $DIVX,null $DIVX_OPT -g ${NXaxis}x${NYaxis} -w %d,250 -u 100 -o $VOBPATH/only_video.avi -m $VOBPATH/only_audio.avi -p /dev/dsp -H0",$heures,$minutes,$BITRATE);
    }

    if ( ! -e "tmp/Record_done")
    {
        unlink("tmp/pass1_done") if ( -e "tmp/pass1_done");
        unlink("tmp/pass2_done") if ( -e "tmp/pass2_done");
        system("$sys"); 
        system("touch tmp/Record_done");
        print "$sys \n"  if ( $INFO);
    } else {
        pwarn("\t Recording already done, remove \"tmp/Record_done\" to record again \n");
        sleep(2);
    }
        
    if ( $QUALITY==1 )
    {
        $sys=sprintf("transcode -i $VOBPATH/only_video.mpeg -p $VOBPATH/only_audio.avi -x yuv4mpeg,mp3 -R 1,tmp/Video.log  -y $DIVX $DIVX_OPT -w %d,250 -o /dev/null -u 100 -J $DEINTL,32detect=force_mode=3,dnr,$add_logo",$BITRATE);
        pinfo("\t\tEncoding  Pass One \n");
        if ( -e "tmp/pass1_done" )
        {   pwarn("\tHigh Quality Pass One already encoded, remove \"tmp/pass1_done\" to reencode \n");
        } else {
            unlink("tmp/pass2_done") if ( -e "tmp/pass2_done");
            print "$sys\n" if ( $INFO );
            system("nice -$nice $sys")==0 or mydie ("Problem to Encode Pass 1");
            system ("touch tmp/pass1_done");
        }
        $sys=sprintf("transcode -i $VOBPATH/only_video.mpeg -p $VOBPATH/only_audio.avi -b $AUDIO_BITRATE,0 -x yuv4mpeg,mp3 -R 2,tmp/Video.log  -y $DIVX $DIVX_OPT -w %d,250 -o movie.avi -u 100 -J dnr,yuvdenoise,32detect=force_mode=3,$add_logo",$BITRATE);
        pinfo("\t\t Pass Two \n");
    } else {
        $sys="transcode -i $VOBPATH/only_video.avi -p $VOBPATH/only_audio.avi -P 1 -b $AUDIO_BITRATE,0 -o movie.avi -y raw";
        pinfo("\t\t Merging Audio+Video\n");
    }
    if ( -e "tmp/pass2_done" )
    {   pwarn(" Pass Two already encoded, remove \"tmp/pass2_done\" to reencode \n");
    } else {
        print "$sys\n" if ( $INFO);
        system("nice -$nice $sys")==0 
            or mydie ("Problem to merge audio and Video Pass 2");
        system ("touch tmp/pass2_done");
    }
    pdebug ("<--- TV Recorder");
    exit(0);
}    

sub format_date 
{
    # returns a nicely formatted string:
    # @param 0 unixtimestamp := number of seconds since 1970
    # @param 1 "hours" := str "hours" [optional]
    # if no second parameter is given, returns a date string 
    # formatted in ISO-8660
    # if second parameter "hours" is given, it converts
    # the number of seconds passed to hours
    if ( exists ($_[1]) && $_[1] eq "hours" ) {
        # we care about hours
        return sprintf("%.2f",$_[0] / ( 60 * 60 )); # 3600 number of seconds in an hour
    }

    # take a UNIX timestamp and returns a nicely formatted string
    my ($sec,$min,$hour,$mday,$mon,$year) = localtime(shift);
    my $ADATE=sprintf("%04d-%02d-%02d %02d:%02d:%02d",
        ($year+=1900),$mon,$mday,$hour,$min,$sec);
    return $ADATE;
} # end format date

sub sec_to_hour 
{
    # convert seconds into a string HH:MM:SS
    # @param 0 number of seconds := integer
    my $seconds = shift;

    my ($days,$hours,$minutes);

    if ( $seconds > 86400) 
    {
        pdebug ("ERROR: sec_to_hour: This value is higher than a day");
        return "00:00:00";
    }

    # Here we don't care about the number of days in $seconds
    # however, I included this for those who care about that value
    # - Luis Mondesi
    #$days = int($seconds / (24 * 60 * 60));
    #$seconds -= ($days * 24 * 60 * 60);

    $hours = int($seconds / ( 60 * 60 ));
    $seconds -= ($hours * 60 * 60);

    $minutes = int($seconds / 60);
    $seconds -= $minutes * 60;

    return sprintf("%02d:%02d:%02d",$hours,$minutes,$seconds);
} # end sec_to_hour

sub srt_subrip
{
    pdebug ("---> Enter srt_subrip");
    pdebug ("subtitle channel=$_[0], subtitle language $_[1]");
    if ( ! -e "$HOME/.V2divx_db" )
    {
        umask(000); # resets current umask values. Luis Mondesi 
        mkdir ("$HOME/.V2divx_db",01777);
        pinfo("$v2d\t PGM2txt database created:\t\t   | ${RED}${HOME}/.V2divx_db\n");
    }
    if ( ! -e "db")
    {
        system("ln -s $HOME/.V2divx_db db");
        pinfo("$v2d\t PGM2txt database ${RED}db${GREEN} linked to \$HOME/.V2divx_db\n");
    } 
    if ( ! -f "tmp/srtpgm_done")
    {
        unlink "tmp/srtpgm2txt_done" if (-f "tmp/srtpgm2txt_done");
        my @def_grey=('255,255,0,255','255,255,255,0','255,0,255,255','0,255,255,255');
        my $i=0;
        my $rep="NO";
        pinfo("$v2d\t In this Terminal we will prepare your subtitle file\n");
        print " As the grey levels of subtitles varied, I first make only one subtitle image.\n"
            ." Subtitle image is good when the background is WHITE and the foreground is BLACK.\n"
            ." Take a look on this subtitle image\n Press Enter ->";
        my $junk=<STDIN>;
        while ( $rep ne 'Y' && $rep ne 'y' && $rep ne 'O' && $rep ne 'o' )
        {
            mydie "Problems with your subtitle image grey level .. Sorry\n Read the transcode man"
                ." pages and change the \$EXTSUB variable in your ~/.V2divxrc \n" if ( $i == 4 );
            my $sys="cat $VOBPATH/* | tcextract -x ps1 -t vob -a 0x2$_[0] | "
                ."subtitle2pgm -c $def_grey[$i] -e 00:00:00,1 -C 15 -o tmp/${TITLE}_test";
            system("$sys")==0 or mydie ("Failed to execute:\n $sys");
            print ("$sys") if ( $INFO );
            system("$XV tmp/${TITLE}_test0001.pgm");
            print " Was this subtitle image good (y|N)? ";
            $rep=<STDIN>;
            chomp($rep);
            $i++;
        }
        system("rm tmp/${TITLE}_test*");
        $i--;
        my $pid = fork();
        mydie "couldn't fork" unless defined $pid;
        if ($pid)
        {
            pinfo("$v2d\t Making subtitle images. This may take a while...\n");
            # try different positions for zero  when output is not optimal (default  255,255,0,255)
            my $sys="cat $VOBPATH/* | tcextract -x ps1 -t vob -a 0x2$_[0] | "
                ."subtitle2pgm -c $def_grey[$i] -C 15 -o tmp/$TITLE";
            print "$sys \n" if ( $INFO);
            system("nice -$nice $sys") == 0  or ( system("touch tmp/srtpgm.finish")==0 
                and mydie "Failed to execute : $sys" ) ;
            system("touch tmp/srtpgm.finish");
            wait;
            system("touch tmp/srtpgm_done");
        } else {
            smily("srtpgm");
        }
    } else {
        pwarn("$v2d\t Subtitle2pgm already done, remove tmp/srtpgm_done, to remake it\n");
    }
    if ( ! -f "tmp/srtpgm2txt_done")
    {
        unlink "$TITLE.srt" if ( -f "$TITLE.srt");
        # run pgm2txt without ocr using a db file (db file can be reused for other rips )
        system ("pgm2txt -v -f $_[1] -d  tmp/$TITLE");
        system("/bin/rm tmp/$TITLE*.pgm");
        system("touch tmp/srtpgm2txt_done");
    } else {
        pwarn("$v2d\t Pgm2txt already done, remove tmp/srtpgm2txt_done  to remake it\n");
    }
    if ( ! -f "$TITLE.srt")
    {
        system ("srttool -s -v -i tmp/$TITLE.srtx -o $TITLE.srt");
        system("/bin/rm tmp/$TITLE*.pgm.txt");
    } else {
        pwarn("$v2d\t $TITLE.srt already done, remove it to remake it\n");
    }	
    #    maybe spell checking should be optional
    my $ilang="-d english"; 
    $ilang="-d french" if ( $_[1] eq 'fr');
    $ilang="-d german" if ( $_[1] eq 'de');
    $ilang="-d spanish" if ( $_[1] eq 'es');

    system ("ispell $ilang  $TITLE.srt");
    pdebug ("<--- srt_subrip");
}

sub mydie
{
    print $RED.$_[0]."\n".$NORM;
    if ( ! $RUNCLUST  && ! $SRTSUBRIP )
    {
        system("mv tmp/dvdtitle $VOBPATH/dvdtitle") if ( -e "tmp/dvdtitle" );
        system("mv tmp/probe.rip $VOBPATH/probe.rip") if ( -e "tmp/probe.rip" );
    } else { 
        system("/bin/rm tmp/$TITLE*.pgm tmp/$TITLE.srtx 2> /dev/null");
        sleep(3);
    }
    unlink("tmp/PB");
    exit(1);
}

use sigtrap qw(handler mydie normal-signals error-signals) ;

sub makelogo
{
    # This function is used to insert logo in cluster mode
    # (the logo duration must be less than 300sec)
    # The result will have the same name (tmp/2-$TITLE.$EXT)
    # than the without logo original file.
    #
    pdebug ("---> Enter makelogo");
    my $add_logo="";
    my ($aogg_output)="";
    $aogg_output="-m tmp/$TITLE.logo.ogg" if ( $AO eq "ogg");
    my $start_frames_logo=floor(($START_SEC+$STARTLOGO)*$FPS);
    my $end_frames_logo=floor($ADDLOGO*$FPS+$start_frames_logo)-1;

    if ( $ADDLOGO > 300 )
    {
        pwarn("Unable to create your Logo in cluster Mode (> 300sec)\n");
        sleep(5);
        return(1);
    }
    
    # first remove the duration of logo from the beginning
    # of merged stream units (made by merge())
    if ( ! -e "tmp/cutoff.finish")
    {
        pinfo("$v2d\t  Cut off the beginning to be replaced by Logo\n");
        my ($in_video_magic,$in_video_codec)=videoformat("$VOBPATH/$SAMPLE");
        my ($in2_video_magic)=videoformat("tmp/2-$TITLE.$EXT");
        my $to_frames=$TOT_FRAMES-$END_SEC*$FPS;
        $PARAMS =~ m/-a (\d) .*/;
        my $audio_params="-a $1";
        my $audio_format=audioformat("$audio_params");
        # Only ONE FILE allowed if input Video type is AVI, QT, DV , MPEG2
        my $vobpath=$VOBPATH;
        $vobpath="$VOBPATH/$SAMPLE" if ( $in_video_magic eq "divx" ||
        $in_video_magic eq "mov" ||
        $in_video_magic eq "mpeg2"  ||
        $in_video_magic eq "dv"  );
        #
        # Let transcode find it, if input audio type is "declared" as PCM
        $audio_format="" if ( $audio_format eq "pcm" );
        $audio_format="vob" if ( $in_video_codec eq "vob" );

        # NTSC opts
        my $fparams="";
        $fparams=" -M 2 " if ( $VIDEONORM eq "NTSC");

        my $pid = fork();
        mydie "couldn't fork" unless defined $pid;
        if ($pid)
        {
            wait;
            system("touch tmp/cutoff.finish");
        } else {
            my $sys = "transcode -p $vobpath $fparams -c $end_frames_logo-$to_frames"
                ." -i tmp/2-$TITLE.$EXT -P 1 -x $in2_video_magic,$audio_format -y raw,null"
                ." -o tmp/$TITLE-withoutlogo.$EXT -u 50";
            print "$sys \n" if ( $INFO);
            system("nice -$nice $sys")==0 or mydie "Unable to cut off beginning of tmp/2-$TITLE.$EXT\n";
            exit();
        }
    } else {
        pwarn(" Remove tmp/cutoff.finish to remake the cutoff of beginning \n");
    }
    
    # Then we make the logo parts itself
    # Then logo parts is encoded WITHOUT audio (this will be make by merge())
    pinfo("$v2d\t Making Logo\n");
    $end_frames_logo++;
    my $final_params="$PARAMS -c 0-$end_frames_logo";
    #$final_params=$final_params." -M 2 --psu_mode --nav_seek tmp/file.nav --no_split " if ( $VIDEONORM eq "NTSC" );
    $final_params=$final_params." -M 2 --psu_mode --no_split " 
        if ( $VIDEONORM eq "NTSC" && !  ( $DEINTL =~ m,ivtc,));
    #$final_params=$final_params." -M 0 -f 23.976 --psu_mode --nav_seek tmp/file.nav --no_split " if ( $DEINTL =~ m,ivtc,);
    $final_params=$final_params." -M 0 -f 23.97,1 --psu_mode --no_split " 
        if ( $DEINTL =~ m,ivtc,);
    $add_logo="logo=file=$LOGO:posdef=$POSLOGO:rgbswap=$RGBSWAP:range=$start_frames_logo-$end_frames_logo,";
    my $filter=$DEINTL.$add_logo.$SUB_TITLE."hqdn3d,";
    if ( ! -e "tmp/logopass1.finish")
    {
        unlink("tmp/logopass2.finish ") if ( -e "tmp/logopass2.finish ");
        my $pid = fork();
        mydie ("couldn't fork") unless defined $pid;
        if ($pid)
        {
            pinfo("$v2d\t Logo pass one ...");
            my $sys = "transcode -q 0 -i $VOBPATH $final_params -w $BITRATE,$keyframes "
                ."-J $filter -y $DIVX,null $DIVX_OPT $VID_OPT -R 1,tmp/$DIVX.logo.log "
                ."-o /dev/null";
            print "$sys \n" if ( $INFO);
            system("nice -$nice $sys") == 0  or ( system("touch tmp/logop1.finish")==0 and mydie "Failed to execute : $sys" ) ;
            system("touch tmp/logop1.finish");
            wait;
            system("touch tmp/logopass1.finish");
        } else {
            smily("logop1");
        }
    } else {	 
        pwarn("\tLogo pass 1 already done,".
        " remove tmp/logopass1.finish to reencode it\n");
    }
    if ( ! -e "tmp/logopass2.finish")
    {
        $filter=$filter."dnr,normalize"; 
        my $pid = fork();
        mydie "couldn't fork" unless defined $pid;
        if ($pid)
        {
            pinfo("$v2d\t Logo pass two ...");
            my $sys = "transcode -q 0 -i $VOBPATH $final_params -w $BITRATE,$keyframes "
                ."-J $filter -y $DIVX,null $DIVX_OPT $VID_OPT "
                ."-R 2,tmp/$DIVX.logo.log -o tmp/$TITLE-logo.$EXT";
            print "$sys \n" if ( $INFO);
            system("nice -$nice $sys")== 0  or ( system("touch tmp/wait.finish")==0 and mydie "Failed to execute: $sys");
            system("touch tmp/logop2.finish");
            wait;
            system("touch tmp/logopass2.finish");
        } else { 
            smily("logop2");
        }
    } else { 	
        pwarn("\tLogo pass 2 already done,".
        " remove tmp/logopass2.finish to reencode it\n");
    }
    
    # And finally merge logo and withoutlogo parts
    pinfo("$v2d\t Merging Logo and 'withoutlogo' parts \n");
    my $sys = "avimerge -i tmp/$TITLE-logo.$EXT tmp/$TITLE-withoutlogo.$EXT -o tmp/2-${TITLE}.$EXT";
    print "$sys \n"  if ( $INFO);
    system("nice -$nice $sys")==0 or mydie "Failed to execute: $sys";;
    pdebug ("<--- makelogo");
}

sub audiorescale
{     
    pdebug ("---> Enter audiorescale");
    my $audio_rescale=1;
    if ( $CLUSTER ne "NO" )
    {	
        create_extract if ( ! -f "tmp/extract.text" || ! -f "tmp/extract-ok" ) ;
        my $info=`cat tmp/extract.text`;
        ( $info =~ m,suggested volume rescale=(\d+.*\d+),) or mydie "Unable to find Suggested volume rescal in tmp/extract.text";
        if ( $1  > 1)
        {
            $audio_rescale = $1;
        }
    } else { 
        if ( ! -e "tmp/astat" )
        {
            pwarn("Unable to find a suggested Volume rescale !\n 1 is use for -s parameter\n");
            sleep(2);
        } else {
            $audio_rescale=`cat tmp/astat`;
            chomp($audio_rescale);
        }
    }
    return($audio_rescale);
    pdebug ("<--- audiorescale");
}  # END audiorescale

sub smily
{
    # This function just display a clock to wait
    unlink("tmp/$_[0].finish");
    my @t=('|','/','-','\\');
    my $i=0;
    while(! -e "tmp/$_[0].finish")
    {
        $i=0 if ($i >3);
        print $t[$i]."\b";
        sleep(1); 
        $i++;
    }
    sleep(1);
    unlink("tmp/$_[0].finish");
    exit(0);
}

sub chk_wdir
{	
    my($tmp,$rep);
    my ($in_video_magic,$in_video_codec)="";
    pdebug ("---> Enter chk_wdir");
    # We check if user is not working in /path/to/vobs
    chomp($VOBPATH);
    my $wdir=`pwd`;
    #chdir($VOBPATH); #unecessary
    ( $VOBPATH ne $wdir ) 
        or mydie "You MUST NOT run V2divx from the ".
    " /path/to/vob directory ...\nPlease cd to another directory";
    chomp($wdir);
    #chdir($wdir);
    pdebug ("Working directory ".`pwd`);

    # Move probe.rip and dvdtitle
    system ("mv $VOBPATH/dvdtitle tmp/dvdtitle") if ( -f "$VOBPATH/dvdtitle");
    system ("mv $VOBPATH/probe.rip tmp/probe.rip") if ( -f "$VOBPATH/probe.rip");
    # Verify that there is no alien files in VOBPATH 
    # AND GET the Video Input Format
    # TODO
    # Luis Mondesi <lemsx1AT_NO_SPAM_hotmail.com> 
    # 2003-12-14 17:35 EST 
    # We should also check for valid filetypes extensions.
    # the most common ones at least.
    # Or, remove a given set of files from here... like any
    # text file. Perhaps use the "file" UNIX utility to
    # determine if a file is text data or whatever
    # and exclude it from this @files array?
    
    opendir(VOB,$VOBPATH);
    my(@files)=grep {! /^\./ & -f "$VOBPATH/$_"} readdir(VOB);
    closedir(VOB);
    # sort in dictionary order...
    my $da;
    my $db;
    @files = sort { 
        ($da = lc $a) =~ s/[\W_]+//g;
        ($db = lc $b) =~ s/[\W_]+//g;
        $da cmp $db;
    } @files;
    #@files=sort @files;
    my $i=0;
    my $file="";
    foreach $file (@files)
    {   
        ($in_video_magic,$in_video_codec)=videoformat("$VOBPATH/$file");
        if ( $i == 0 )
        {
            $tmp=$in_video_codec;
            pinfo("$v2d\t Video Input Format:\t\t\t   | $in_video_codec\n");
            mydie "$VOBPATH/$file has not a valid video format" if ( $in_video_codec eq "null" );
        }
        mydie "$VOBPATH/$file is not a valid file. All $VOBPATH files (except dvdtitle and/or probe.rip) MUST have the same format" if ( $tmp ne $in_video_codec );
        $i++;
        print "File $i: $file\n" if ( $INFO );
    }
    mydie ("Only ONE AVI, DV  or QT file is allowed to transcode at a time.\n Please merge those $i avi (dv or qt ) files\n")  if (( $in_video_magic eq "divx" || 
    $in_video_magic eq "mov" || 
    $in_video_magic eq "mpeg2"  || 
    $in_video_magic eq "dv"  ) && 
    $i > 1 );
    $SAMPLE = $files[floor($i / 2)];
    pdebug ("Sample : $SAMPLE");
    $LASTVOB = $files[$i-1];
    pdebug ("Last file : $LASTVOB");
    ( -f "$VOBPATH/$SAMPLE" and -f "$VOBPATH/$LASTVOB" ) or 
    mydie "Unable to find samples files in \"$VOBPATH\" ".
    "(files MUST be have the same Video format)";

    # Vrfy probe.rip
    if ( -e "tmp/probe.rip")
    {
        # We need to verify if user has not remove some file(s) since the rip
        print "\t Number of vob files:$i\n" if ( $INFO);
        closedir(VOB);
        open (PROBE,"<tmp/probe.rip");
        my $flag=1; 
        while(<PROBE>)
        {
            if ( $_=~ m,Number of vob files:(\d+),)
            {
                if ( "$i" ne "$1" )
                {
                    # If user has remove some files we cannot use probe.rip anymore
                    pwarn("Number of vob files in probe.rip is not exact, V2divx will create extract.txt \n");
                    system ("touch tmp/probe.rip-BAD");
                }
             $flag=0; last;
            }
        }
        close(PROBE);
        pwarn("Oups...no number of Vob files in the probe.rip file!\n") if ( $flag eq 1 );
    }

    if ( $TITLE eq "" && -e "tmp/dvdtitle")
    {
        $TITLE=`cat tmp/dvdtitle`;
        chomp($TITLE);
    }
    pdebug ("<--- chk_wdir");
} # End Check Working directories


sub get_audio_channel
{	
    pdebug ("---> Enter get_audio_channel");
    my ($in_video_magic,$in_video_codec)=videoformat("$VOBPATH/$SAMPLE");
    my $number_of_ac=0;
    my $findaudio_channel="";
    my $audio_channel="";
    my @achannels;
    my($i)=0;
    my $probe="";
    if ( -e "tmp/probe.rip" )
    {
        open (RIP,"<tmp/probe.rip");
        while(<RIP>)
        {       
            chomp;
            if ( $_ =~ m,(?:ac3|mpeg2ext|lpcm|dts|mp3) ([^\s]+) ,)
            {
                pinfo("$v2d\t Language of audio stream $i:\t\t   | $1\n") if ( ! defined($_[0]) && $FLAG_GAC); 
                $findaudio_channel=$i if ( $1 eq $LANGUAGE && ! defined($findaudio_channel));
                $i++;
            }
        }
    }
    # get_audio_channel has been call by printinfo ;-)
    if ( $_[0] eq "findaudio_channel" )
    {
        $FLAG_GAC=0 ;
        return($findaudio_channel); 
    }
    if ( $in_video_magic eq 'mov' )
    {
        $probe=`tcprobe -i $VOBPATH/$SAMPLE 2>&1 | grep 'audio track:'` or mydie "Problem when running \'tcprobe -i $VOBPATH/$SAMPLE\'";
    } else {
        $probe=`tcprobe -i $VOBPATH 2>&1 | grep 'audio track:'` or mydie "Problem when running \'tcprobe -i $VOBPATH\'";
    }
    # If this is so complicated this is because ... YES sometime audio channel are not in order !!
    my @line=split /\n/,$probe;
    my $tmp=$line[0];
    # set flag to false to test for audio
    my $flag_ac_detected = "0"; # this flag is just for this
    # following loop and nothing else

    foreach $tmp ( @line )
    {	
        if ( $tmp =~ m,-a (\d) \[, )
        {
            #pdebug ("-a DIGIT found");
            $achannels[$number_of_ac]=$1;
            print "\t + Channel: $achannels[$number_of_ac]\n" if ($INFO && $FLAG_GAC ) ;
            $number_of_ac++;
            $tmp=$line[$number_of_ac];
            $flag_ac_detected = 1;

        } elsif ( $tmp =~ m,no audio track,) {	 
            $number_of_ac++;
            $tmp=$line[$number_of_ac];
            $number_of_ac--;
            #			 mydie " There is NO audio track in your clip,\n Sorry, at this moment, I'm unable to work on it ....";
            $NOAUDIO=1;
            pwarn("$v2d\t No audio channels in this Clip\n") if ($FLAG_GAC);
            pdebug ("  <--- get_audio_channel");
            $FLAG_GAC=0;
            return(0);
        } elsif ( $flag_ac_detected eq 0) { 
            mydie "Unable to get audio track info ?\n";
        }
    }
    pinfo("$v2d\t Number of audio channels detected:\t   | $number_of_ac\n") if ($FLAG_GAC);
    $number_of_ac--;
    if ( $findaudio_channel ne "" )
    {
        $audio_channel=$findaudio_channel;
        pinfo("$v2d\t Audio channel for $RED$LANGUAGE$GREEN language:\t\t   | $audio_channel\n\t (You may modify your \$LANGUAGE variable in your $userconfigfile)\n") if ($FLAG_GAC);
    } else {
        pwarn("$v2d\t Unable to find your Language ($LANGUAGE) in:\t   | tmp/probe.rip \n") if ($FLAG_GAC) ;
        pinfo("$v2d\t Default audio channel is set to ") if ($FLAG_GAC);
        if ( $DEF_AUDIOCHANNEL <= $achannels[$number_of_ac])
        { 
            $audio_channel = $DEF_AUDIOCHANNEL;
            print $GREEN.":\t   | ".$RED if ($FLAG_GAC);
        } else {
            $audio_channel=0;
            print ":\t   | ".$RED if ($FLAG_GAC);
        }	
        print $audio_channel."\n".$NORM if ($FLAG_GAC);
    }
    $FLAG_GAC=0;
    return($number_of_ac++,$audio_channel,@achannels) if ( $_[0] eq "all");
    pdebug ("<--- get_audio_channel");
} # end get_audio_channel

sub readuserconf
{
    if ( -f $userconfigfile ) {
        open (USERCONF,"<$userconfigfile");
    } else {
        open (USERCONF,"<$HOME/.vob2divxrc");
    }

    while (<USERCONF>)
    {
        chomp;
        # On an idea of tom roth <tom AT tomroth.de>
        s[/\*.*\*/][];      #  /* comment */
        s[//.*][];          #  // comment
        s/#.*//;            #  # comment
        s/^\s+//;           #  whitespace before stuff
        s/\s+$//;           #  whitespace after stuff
        next unless length; #  If our line is empty, we ignore it
        s/^\$//;
        s/\;$//;
        # Please Luis DO NOT REMOVE the next two lines .
        # sometimes users can have $VAR= value ; # comment
        # We need to remove the white space before and after value 
        s/^\s+//;           #  whitespace before value 
        s/\s+$//;           #  whitespace after value

        s/["']+//g;         # remove all quotes
        
        my ($var_name, $value) = split(/\s*=\s*/, $_);
        $nice=($var_name eq "nice" && $nice eq "" )?$value:$nice;
        $DIVX=($var_name eq "DIVX" && $DIVX eq "" )?$value:$DIVX;
        $DIVX_OPT=($var_name eq "DIVX_OPT" && 
             $DIVX_OPT eq "" && !$Res_DIVX_OPT)?$value:$DIVX_OPT;
        $XV=($var_name eq "XV" && $XV eq "" )?$value:$XV;
        $XINE=($var_name eq "XINE" && $XINE eq "" )?$value:$XINE;
        $CLUSTER_CONFIG=($var_name eq "CLUSTER_CONFIG" && $CLUSTER_CONFIG eq "" )?$value:$CLUSTER_CONFIG;
        $XTERM=($var_name eq "XTERM" && $XTERM eq "" )?$value:$XTERM;
        $RMCMD=($var_name eq "RMCMD" && $RMCMD eq "" )?$value:$RMCMD;
        $AVIPLAY=($var_name eq "AVIPLAY" && $AVIPLAY eq "" )?$value:$AVIPLAY;
        $LOGO=($var_name eq "LOGO" && $LOGO eq "" )?$value:$LOGO;
        $POSLOGO=($var_name eq "POSLOGO" && $POSLOGO eq "" )?$value:$POSLOGO;
        $STARTLOGO=($var_name eq "STARTLOGO" && $STARTLOGO eq "" )?$value:$STARTLOGO;
        $TIMELOGO=($var_name eq "TIMELOGO" && $TIMELOGO eq "" )?$value:$TIMELOGO;
        $LANGUAGE=($var_name eq "LANGUAGE" && $LANGUAGE eq "" )?$value:$LANGUAGE;
        $DEF_AUDIOCHANNEL=($var_name eq "DEF_AUDIOCHANNEL" && $DEF_AUDIOCHANNEL eq "" )?$value:$DEF_AUDIOCHANNEL;
        $AO=($var_name eq "AO" && $AO eq "" )?$value:$AO;
        $EXTSUB=($var_name eq "EXTSUB" && $EXTSUB eq "" )?$value:$EXTSUB;
        pdebug ("$var_name  = $value");
        pdebug ("DIVX_OPT = $DIVX_OPT");
        pwarn("-- WARNING --\n  \$DEBUG variable is no more use, use $0 --debug instead.\n") if ( $var_name eq "DEBUG");
        pwarn("-- WARNING --\n \$INFO variable is no more use, use $0 --info instead.\n") if ( $var_name eq "INFO");
        
        #if ( defined $$var_name 
        #    && $var_name ne "DEBUG" 
        #    && $var_name ne "INFO"
        #)
        #{
        #    print "WARNING : $userconfigfile variable $var_name ".
        #        "value not used, set to $$var_name\n".$NORM ;
        #    next;
        #}

    }
    pwarn("\t Warning :  DIVX_OPT reset to \"$DIVX_OPT\"\n") if ( $DEBUG && $Res_DIVX_OPT);
    close (USERCONF);
    
    $XV="xv" if ( $XV eq "" );
    $XINE="xine" if ( $XINE eq "" );
    $AVIPLAY="aviplay" if ( $AVIPLAY eq "" );
    $RMCMD="ssh" if ( $RMCMD eq "" );
    $XTERM="xterm \%s -e" if ( $XTERM eq ""  ); 
    $POSLOGO=4 if ( $POSLOGO eq "" );
    $CLUSTER_CONFIG="/see/your/V2divxrc" if ( $CLUSTER_CONFIG eq "" );
    $TIMELOGO=25 if ( $TIMELOGO eq "" );
    $LOGO="/see/your/V2divxrc" if ( $LOGO eq "" );
    $DEF_AUDIOCHANNEL=0 if ( $DEF_AUDIOCHANNEL eq "" );
    $LANGUAGE="fr"  if ( $LANGUAGE eq "" );
    $STARTLOGO=2 if ( $STARTLOGO eq "" );
    $nice=10 if ( $nice eq "" );

    $DIVX="ffmpeg" if ( $DIVX eq "" );
    if ( $DIVX_OPT  eq "" )
    {    
        $DIVX_OPT="";
        $DIVX_OPT="-F mpeg4" if ( $DIVX eq "ffmpeg");  
        open (USERCONF,">>$userconfigfile");
        print USERCONF "# Add your encoder specifics options \n\$DIVX_OPT=$DIVX_OPT ; #for example '-F mpeg4' for divx ffmpeg output";
        close(USERCONF);
    }
    #pdebug ("\n++++++++++ AO1 -> $AO ");
    if ( $AO eq ""  )
    {
        #pdebug ("\n++++++++++ AO -> $AO ")
        $AO="mp3";
        open (USERCONF,">>$userconfigfile");
        print USERCONF "\n# This is the output Audio format\n# allowed formats are : mp3 or ogg (if ogmtools are installed )\n\$AO=$AO;";
        close(USERCONF);
    }

    if ( $EXTSUB eq ""  )
    {
        open (USERCONF,">>$userconfigfile");
        print USERCONF "# EXT SUBTITLE FILTER 5 LAST OPTIONS (here we just use 3) ...See the docs 8-(\n\$EXTSUB=0:0:255;";
        close(USERCONF);
        $EXTSUB="0:0:255";
    } 

    pdebug (" <--- readuserconf");
} # END readuserconf


sub printinfo
{	
    my ($start_frames_logo,$end_frames_logo,$endlogo)="";
    my $subtitle=""; # -J opts OR srtsubrip 
    my $add_logo=""; # -J opts 
    system("clear") if (  ! $DEBUG && ! $INFO );	
    print "\t*********************************************************\n";
    if ( $TVREC )
    {
        print $v2d." A/V:\tTV Recorder Quality:\t   | $RED";
        print "High" if ( $QUALITY==1);
        print "Normal"  if ( $QUALITY==2);
        print $NORM."\n";
    } else {
        print $v2d."   V:\tVideo Input Norm:\t   | $VIDEONORM \n";
    }
    print $v2d."   V:\tVideo Output Format:\t(1)| $DIVX\n";
    print $v2d."   V:\tVideo Input interlaced:\t   | ";
    if ( $DEINTL ne  ""  )
    {
        print $RED."YES\n".$NORM;
        print $v2d."   V:\tDeinterlaced with:\t(2)| ";
        print "MPlayer postproc\n" if ( $DEINTL =~ m,pp, );
        print "YUVdenoise \n" if ( $DEINTL =~ m,yuvdenoise,); 
        print "Smartdeinter \n" if ( $DEINTL =~ m,smartdeinter,); 
        print "ivtc \n" if ( $DEINTL =~ m,ivtc,); 
        print "32detect\n" if (($DEINTL=~ m,32detect,) && ! ($DEINTL =~ m,ivtc,) )
    } else {
        print "NO\n";
    }

    print $v2d."   V:\tLogo file name:\t\t(1)| $LOGO\n";
    if ( ( $ADDLOGO && $CLUSTER eq "NO" ) || 
        ( $ADDLOGO != 0 && $ADDLOGO <= 300 && $CLUSTER ne "NO"))
    {
        $start_frames_logo=floor(($START_SEC+$STARTLOGO)*$FPS);
        $end_frames_logo=floor($ADDLOGO*$FPS+$start_frames_logo);
        $add_logo="logo=file=$LOGO:posdef=$POSLOGO:rgbswap=$RGBSWAP:range=$start_frames_logo-$end_frames_logo,";
        $endlogo=$STARTLOGO+$ADDLOGO;
        print  $v2d."   V:\tLogo starting time:\t(2)| $STARTLOGO s.\n";
        print  $v2d."   V:\tLogo ending time:\t(2)| $endlogo s.\n";
    } else {
        print $v2d."   V:\tLogo inserted:\t\t   | ${RED}NO${NORM}\n";
    }
    my $findaudio_channel=get_audio_channel("findaudio_channel") ;
    print $v2d."   A:\tLanguage Audio channel:\t(1)| $LANGUAGE\n" if ($findaudio_channel ne "" ) ;
    print $v2d."   C:\tCluster config file:\t(1)| $CLUSTER_CONFIG\n" if ( $CLUSTER ne "NO");
    print $v2d."   C:\tCluster remote cmd:\t(1)| $RMCMD\n" if ( $CLUSTER ne "NO");
    open (CC,"<$CLUSTER_CONFIG");
    my $i=1;
    while(<CC>)
    {
        if ( $_=~ m/([^\s#]*)#*/)
        {
            print $v2d."   C:\tCluster node $i:\t\t(3)| $1% frames to process\n" if ( $CLUSTER ne "NO" && $1 ne "" );
            $i++ if ( $CLUSTER ne "NO" && $1 ne "" );
        }	
    }
    close CC;   
    my $fps=($DEINTL =~ m,ivtc,)?23.97:$FPS;
    my $nbr_frames=calculate_nbrframe; 
    print $v2d."   V:\tFrames to encode:\t   | $nbr_frames, @ $fps frames per/sec\n";
    printf($v2d." A/V:\tRuntime to encode:\t   | %d hours:%d minutes:%d sec\n",int($RUNTIME/3600),int($RUNTIME-int($RUNTIME/3600)*3600)/60,$RUNTIME-int($RUNTIME/60)*60);
    if ( ! $TVREC )
    {
        $PARAMS =~ m,-a (\d) .*,;
        print $v2d."   A:\tPrimary Audio channel:\t   | $1\n";
        if ( $AC2 ne "" )
        {
            chomp($AC2);
            print $v2d."   A:\tSecundary Audio Channel:(2)| $AC2\n";
        }
    }
    printf($v2d."   A:\tAudio Output format:\t   | $AO \n",$AUDIO_SIZE);
    printf($v2d."   A:\tAudio Size:\t\t   | %.2f Mb @ (2) $AUDIO_BITRATE Kb/s\n",$AUDIO_SIZE);
    my $videosize=$BITRATE*1000*$RUNTIME/(1024*1024*8);
    printf($v2d."   V:\tEstimated Video Size:\t   | %.2f Mb @ %d Kb/s\n",$videosize,$BITRATE);
    my $totalsize=$videosize+$AUDIO_SIZE;
    printf($v2d." A/V:\tEstimated Total Size:\t(2)| %.2f Mb\n",$totalsize);
    if ( ! $TVREC )
    {
        print $v2d."   V:\tInput Frame Size:\t   | ${Xaxis}x$Yaxis\n";
        printf($v2d."   V:\tClipped Frame Size:\t   | %dx%d \n",$Xaxis-2*$lr,$Yaxis-2*$tb);
        printf($v2d."   V:\tOriginal aspect ratio:\t   | %.2f:1\n",$ASPECT_RATIO);
    }
    print $v2d."   V:\tOutput Frame Size:\t   | ${NXaxis}x$NYaxis\n";
    printf($v2d."   V:\tFinal aspect ratio:\t   | %.2f:1\n",$NXaxis/$NYaxis);
    my $AR=abs(100-($NXaxis*100/($NYaxis*$ASPECT_RATIO)));
    printf($v2d."   V:\tAspect ratio error:\t   | %.2f ",$AR);
    print "%\n";
    my $fbpp=1000*$BITRATE/($NXaxis*$NYaxis*$FPS);
    printf($v2d."   V:\tBits Per Pixel:\t\t   | %.3f\n",$fbpp);
    print $v2d." A/V:\tFinal Video file name:\t   | $RED$TITLE$NORM.$EXT\n";
    print  $v2d."   T:\tTrcode main parameters:\t(2)| $PARAMS\n" if ( $INFO );
    $subtitle=$SUB_TITLE if ( $SUB_TITLE =~ m,extsub,);
    my $filter=$DEINTL.$add_logo.$subtitle;
    print $v2d."   T:\tOptional Filters:\t(2)| $filter\n" if ( $INFO );
    print "\n";
    print "(1)This value can be modify in your $userconfigfile\n";
    print "(2)This value can be modify in tmp/V2divx.conf\n";
    print "(3)This value can be modify in your $CLUSTER_CONFIG\n" if ( $CLUSTER ne "NO");
    print $RED."\tYou can say \'no\' at this time, modify by hand some parameters \n\t in the tmp/V2divx.conf(BUT TAKE CARE!) \n\t or in your $userconfigfile,\n\t and then rerun V2divx without parameters\n".$NORM;
    print " Ready to encode (y|N)? ";
    my $rep=<STDIN>;
    chomp($rep);
    mydie "" if ( $rep ne "y" && $rep ne "Y" );
    my $pid= fork();
    mydie "couldn't fork" unless defined $pid;
    if ( $pid == 0 )
    {
        if ( ! -e "$TITLE.srt" && $SUB_TITLE =~ m,SRT_(\d+)_(.*),)
        {
            system(sprintf("$XTERM $0 --srtsubrip &",""))==0 or print STDERR "couldn't exec $XTERM $0 --srtsubrip\n";
        }
        exit(0);
    } 
    wait;
    $SUB_TITLE="" if ( $SUB_TITLE =~ m,SRT_,);
}

sub interlaced
{
    my ($is_interlaced)="";
    pinfo("$v2d\t Detecting if frames are interlaced:\t   | ");
    my $pid = fork();
    mydie "couldn't fork" unless defined $pid;
    if ($pid)
    { 
        $is_interlaced=`transcode -i "$VOBPATH/$SAMPLE" -J 32detect=verbose -c 0-500 2>&1 | grep interlaced`;
        system("touch tmp/intl.finish");
        wait;
    } else {
        smily("intl");
    }
    if ( ! ( $is_interlaced =~  m,interlaced = (yes),))
    {
        ( $is_interlaced =~  m,interlaced = (no),) or mydie "Unable to Detect Interlacing in $VOBPATH/$SAMPLE";
        $is_interlaced="no";
        pinfo("NO\n");
    } else {
        $is_interlaced="yes";
        pwarn("YES\n");
    }
    return($is_interlaced);
}


sub findclip
{
    my ($sys)="";
    if ( $PGMFINDCLIP )
    {
        my ($in_video_magic,$in_video_codec)=videoformat("$VOBPATH/$SAMPLE");
        my $file="";
        pinfo("$v2d\t Trying to detect best Clipping:\t   | ");
        opendir(VOB,$VOBPATH);
        my(@files)=grep {! /^\./ & -f "$VOBPATH/$_"}readdir(VOB);
        closedir(VOB);
        my $da;
        my $db;
        @files = sort { 
            ($da = lc $a) =~ s/[\W_]+//g;
            ($db = lc $b) =~ s/[\W_]+//g;
            $da cmp $db;
        } @files;
        #@files=sort @files;
        my $i=0;
        my $pid = fork();
        mydie "couldn't fork" unless defined $pid;
        if ($pid)
        { 	
            foreach $file (@files)
            {	
                $sys="transcode -M 1 -q 0 -z -K -x $in_video_codec,null -i \"$VOBPATH/$file\" -y ppm,null -c 130-135  -o autoclip$i  >/dev/null 2>&1";
                # DO WE REALLY NEED video_magic ?
                # $sys="transcode -M 1 -q 0 -z -K -i\
                # \"$VOBPATH/$file\" -y ppm,null -c 130-135\
                # -o autoclip$i  >/dev/null 2>&1";
                pdebug ("$sys");
                system ("$sys")==0 or 
                ( system("touch tmp/fndclip.finish && /bin/rm autoclip*.pgm ")==0 and  
                mydie "Unable to encode to ppm file ($VOBPATH/$file)" );
                $i++;
            }
            system("touch tmp/fndclip.finish");
            wait;
        } else {
            smily("fndclip");
        }
        my $clip=`pgmfindclip -b 8,8 autoclip*.pgm` or ( system("/bin/rm autoclip*.pgm") and mydie "Problem to run \'pgmfindclip -b 8,8\'\n Your pgmfindclip release is may be too old ...\n");
        chomp($clip);
        my @clip=split /,/,$clip;

        # We put the clipping border same size ( the smallest )
        $clip[0]=$clip[2] if ( $clip[2] < $clip[0] ) ;
        $clip[1]=$clip[3] if ( $clip[3] < $clip[1] ) ;
        system("/bin/rm autoclip*.pgm");
        $tb=$clip[0];
        $lr=$clip[1];
        pinfo("-j $tb,$lr\n");
    } else {
        pwarn("$v2d\t pgmfindclip in your PATH:\t\t   | NO\n");
        print "You may find pgmfindclip at http://www.lallafa.de/bp/pgmfindclip.html\n" if ( $INFO );
        sleep(2);
        $tb=0;
        $lr=0;
    }
}

sub ask_clust
{
    # Asks if Cluster is used 
    # and initialize the number of stream units if yes

    my ($rep)='n';
    pdebug ("--->  Enter ask_clust");
    $CLUSTER="NO";
    open(CONF,">>tmp/V2divx.conf");
    unlink("tmp/cluster.args");
    pinfo("$v2d\t Cluster config file exist:\t\t   |");
    if ( ! -f $CLUSTER_CONFIG )
    {
        pwarn(" NO\n");
    } else {
        pinfo(" Yes\n");
        if ( ! $OKCLUSTER )
        {
            print " Do you want to use a cluster (y|N)? ";
            $rep=<STDIN>;
            chomp($rep);
        } else {
            $rep='y';
        }
        if ( $rep eq "y" || $rep eq "Y" || $rep eq "o" || $rep eq "0")
        {	
            # first find out number of FPS and then create_nav
            if ( $FPS eq "" )
            {
                $FPS = get_fps();
            }
            # we don't need to worry if the movie is telecine yet...
            $VIDEONORM = ( $FPS =~ m,2[39]\.9, ) ? "NTSC" : $VIDEONORM;
            create_nav($VIDEONORM);
            # TODO use Perl to read this line
            #get_tail("tmp/file.nav",-1);
            my $stream_units=`tail -1 tmp/file.nav | awk '{print \$1}'`;
            chomp($stream_units);
            # Take care if there are too many stream units !!
            if ( $stream_units > 10 )
            {
                pwarn("\tThere are too many stream units".
                " to encode this clip \n".
                "\t in cluster mode with a good ".
                "video quality\n\tReversing to NO".
                " CLUSTER...\n");
                $stream_units="NO";
                sleep(2);
            }
            # WE need create-extract in CLUSTER Mode 
            # to have $audio_rescale :-(
            create_extract if ( $stream_units ne "NO" 
                && ( ! -f "tmp/extract-ok" || ! -f "tmp/extract.text"));
            # Initialize the $CLUSTER to the number of stream Units or to NO if there
            # are too many Stream Units 
            $CLUSTER=$stream_units;
        }
    }
    print CONF "#CLUSTER:$CLUSTER # YOU CAN REMOVE BUT NOT MODIFY THIS LINE\n";
    close(CONF);
    pdebug ("<--- ask_clust");
} # end ask_clust

sub cluster
{ 	
    # ============= Cluster MODE =================== # 
    my ($sys)="";
    pdebug ("--->  Enter cluster");
    # Need to know working directory 
    # and pass it to the cluster nodes (to find tmp/*)
    my $wdir=`pwd`;
    chomp($wdir);
    ( -f $CLUSTER_CONFIG ) or mydie $warnclust;

    # $addpower is the percentage of frames
    # that available nodes need to encode 
    # more than requested (due to unavailable nodes)
    my $addpower=0;

    my $localhost=`hostname`;
    chomp($localhost);


    # Open a secure temporary file in /tmp/ to 
    # rebuild cluster config file
    # which contain ONLY available nodes
    # returns a filehandle and filename
    #my $s_file = new File::Temp(
    my ($s_file,$n_file) = tempfile(
                                "UNLINK"    => 0, # do not unlink
                                "TEMPLATE"  => "XXXXXXXXX",
                                "DIR"       => "tmp",
                                "SUFFIX"    => ".txt"
                            );
    pdebug ("echo temp filename = $n_file");

    # $node_counter is the counter of AVAILABLE nodes
    my $node_counter=`grep -v "^[[:space:]]*#" $CLUSTER_CONFIG | grep -v '^[[:space:]]*$$' | wc -l`;

    if ( $node_counter != 0 )
    {
        # $poweroff is the percentage of frame 
        # that we can't encode on affected nodes 
        # (cause they are unavailable)
        my $poweroff=0;

        # $poweron is the percentage of frame
        # that we can encode on affected nodes
        my $poweron=0;

        # TODO use Perl code to read this file:
        #   @allnodes=custom_read_file(); where this function returns a string
        # read the cluster config file
        my @allnodes=`grep -v "^[[:space:]]*#" $CLUSTER_CONFIG | grep -v '^[[:space:]]*$$'| awk -F"#" '{print \$1}'`;

        foreach $node  ( @allnodes )	
        {
            # chost[0] is the node name
            # chost[1] is the percent of frames to encode
            my @chost=split /:/, $node;
            my $rhost=$chost[0];
            my $P=$chost[1];
            chomp($P);

            if ( $rhost ne "$localhost" )
            {	
                my $rs="";  
                pinfo("$v2d\t Checking node $rhost :\t\t   | ");
                my $pid = fork();
                mydie "couldn't fork [function: cluster]" unless defined $pid;
                if ($pid)
                {
                    $sys="$RMCMD $rhost 'cd ".$wdir." && ".$0." -v' >/dev/null 2>&1";  
                    print "$sys \n" if ( $INFO);
                    $rs=system ("$sys");
                    system("touch tmp/checknode.finish");
                    wait;
                } else {
                    smily("checknode");
                }
                # $rs != 0 means node unreachable 
                # or 'V2divx -v' exit with non 0 status
                # or master working directory does not exist on node
                # so we don't run on it
                if ( $rs != 0 )
                {
                    pwarn ("Bad\n");
                    $node_counter = $node_counter - 1;
                    $poweroff = $poweroff + $P;
                    pwarn("\tNeed to calculate $poweroff % of frames on other node(s)\n");
                    last if ( $poweroff >= 100 );
                } else {
                    # This node is OK we print it in the good nodes temporary file
                    pinfo ("OK\n");
                    print $s_file "$rhost:$P\n";
                    $poweron = $poweron + $P;
                }
            } else { 
                # This is the master (localhost) 
                pdebug ("Host : ".$chost[0]." , Pow = $P");
                print $s_file "$rhost:$P\n";
                $poweron = $poweron + $P;
            }
        }
        $addpower = $poweroff / $node_counter;

        close($s_file);
    } else {
        # we don't need cluster-mode?
        pdebug ("-> Unlinking file $n_file");
        unlink("$n_file");
    }

    pinfo("$v2d\t Number of available Nodes:\t\t   |$node_counter\n");

    # Now we have number of a available nodes
    # percentages of frames that each nodes
    # need to encode more
    # So we run V2divx on nodes ...
    if ( -f "$n_file" )
    {	
        # tabpower is just a table to know when nodes 
        # have finished to encode
        my @tabpower="";

        # percentage of frames already encode 
        my $sumpow=0;

        # percentage of frames encoded by a node
        my $pow=0; 

        my  $max=0;	
        my $i=0;
        my @allnodes=`cat $n_file`; # TODO read file using Perl
        unlink("$n_file"); 

        foreach $node (@allnodes )     
        { 
            my @chost=split /:/, $node; 
            my $rhost=$chost[0];
            if ( $sumpow < 100 )
            {	
                # we add the percentage of frames to encode more
                $pow=$chost[1] + $addpower;
                # percentage of frames which will be encoded
                $max=$sumpow + $pow;
                # is this is > %100 just encode to reach 100% .
                $pow = 100 - $sumpow if ( $max > 100 );

                open(CLUSTERARGS,">tmp/cluster.args");
                print CLUSTERARGS "$sumpow,$pow\n";
                close(CLUSTERARGS);
                pinfo("$v2d\t Encoding on node $rhost:\t\t   | -W $sumpow,$pow\n");
                my $this_arg=""; # reset arguments
                $this_arg="-n $rhost" if $XTERM =~ /xterm/i;
                $this_arg="-t $rhost" if $XTERM =~ /gnome-terminal/i;
                my $this_cmd=""; # reset command string
                if ( $rhost ne "$localhost" &&  $rhost ne "localhost" ) 
                { 	
                    # Nodes just need -o $AO to know $EXT (they DO NOT encode audio)
                    $this_cmd=sprintf("$XTERM $RMCMD $rhost $0 --runclust $wdir -o $AO -i &",$this_arg);
                    print "+ executing $this_cmd\n" if ( $INFO );
                    system ( $this_cmd );
                } else {
                    $this_cmd=sprintf("$XTERM $0 --runclust $wdir -o $AO -i &",$this_arg);
                    #$this_cmd=sprintf("$XTERM ls --runclust $wdir &",$this_arg);
                    print "+ executing $this_cmd\n" if ( $INFO );
                    system ( $this_cmd );
                }
                pinfo("$v2d\t V2divx on the node $rhost starting\t   | ");
                $tabpower[$i]=$sumpow;
                $i++;
                $sumpow= $sumpow + $pow;
                # we have to be SURE that V2divx has started on the remote node 
                # before continue ( we wait for 25 secondes max)
                my $j=0;
                while ( ! -f "tmp/node_started" && $j < 25 )
                {
                    sleep(1);
                    $j++;
                }
                unlink("tmp/node_started" );
                pwarn("NO\n") if ( $j > 24  );
                mydie("Seems that there is a problem on node $rhost\n $0 has not started correctly\n Unable to continue.\n") if ( $j > 24  );
                pinfo("OK\n");
            }
        }
        # If percentage of frames < 100%
        # We encode the rest on localnode
        if  ( $sumpow <  100 )
        {
            $pow = 100 - $sumpow;
            open(CLUSTERARGS,">tmp/cluster.args");
            print  CLUSTERARGS "$sumpow,$pow\n";
            close(CLUSTERARGS);
            pinfo("$v2d\t Encoding on localnode with:\t\t\t   | -W $sumpow,$pow to finish\n");
            system ( sprintf("$XTERM $0 --runclust $wdir -o $AO &","") );
            $tabpower[$i]=$sumpow;
            sleep(3);
        }

        pinfo("$v2d\t Waiting for nodes to finish ....\n");
        
        my $endnode="";
        my $NODE_ERROR=0;
        foreach $endnode ( @tabpower )
        {
            while ( ! -e "tmp/2-$TITLE${endnode}_0.finish" 
                && ! -e "tmp/2-$TITLE${endnode}.PB" 
                && ! -e "tmp/1-$TITLE${endnode}.PB")
            {
                print "\r|"; 
                sleep(1); 
                print "\r/"; 
                sleep(1); 
                print "\r-"; 
                sleep(1); 
                print "\r\\"; 
                sleep(1);
            }
            pinfo("$v2d\t Node $endnode has finished encoding\n") if ( -e "tmp/2-$TITLE${endnode}_0.finish");
            pwarn("\tCritical error on node: $endnode during Pass One\n") if ( -e "tmp/1-$TITLE${endnode}.PB");
            pwarn("\tCritical error on node: $endnode during Pass Two \n") if ( -e "tmp/2-$TITLE${endnode}.PB");
            $NODE_ERROR=1 if ( -e "tmp/2-$TITLE${endnode}.PB" || -e "tmp/1-$TITLE${endnode}.PB");
            unlink("tmp/2-$TITLE${endnode}.PB");
            unlink("tmp/1-$TITLE${endnode}.PB");
        }	
        mydie("\tCould'nt continue, critical error on one or more nodes ...\n") if ( $NODE_ERROR );
        merge;
        twoac;
        finish;
        unlink("tmp/cluster.args");
    } else {
        pwarn("\tNothing to do :-(\n");
    }
    pdebug ("<--- cluster");
    exit(0);
    # End cluster Sub routine
}


sub create_nav
{
    # Create Nav File (For cluster)
    # @param 0 str videonorm := NTSC or PAL [default]
    # takes a string to determine what type of demux (map)
    # to do for clusters 
    return()  if (  -f "tmp/filenav-ok" &&  -f "tmp/file.nav" );
    my ($sys)="";
    my $demux="";
    my $video_norm_str = shift; # get VIDEONORM from this if passed
    $demux=" -M 2 " if ( $VIDEONORM eq "NTSC" 
        or $video_norm_str eq "NTSC" );
    pinfo("$v2d\t Creating File Navigation:\t\t   | tmp/file.nav\n");
    my $pid = fork();
    mydie "couldn't fork [function: create_nav]\n" unless defined $pid;
    if ($pid)
    {
        $sys = "cat $VOBPATH/* | tcdemux $demux -W > tmp/file.nav";
        print ($sys."\n") if ( $INFO );
        system ("nice -$nice $sys") == 0  
            or ( system ("touch tmp/filenav.finish")== 0 
            and mydie "Unable to create file nav\n" );
        system("touch tmp/filenav.finish");	
        wait;
        system("touch tmp/filenav-ok");
    } else {
        smily("filenav");
    }
} # end create_nav

sub create_extract
{
    #  ****** Create extract info (to calculate bitrate) ****** #
    my ($rep,$sys,$frames)="";
    my ($in_video_magic,$in_video_codec)=videoformat("$VOBPATH/$SAMPLE");
    pdebug ("--->  Enter create_extract");

    # we need only $audio_channel but ...
    my ($number_of_ac,$audio_channel,@achannels)=get_audio_channel("all");
    my $audio_format=audioformat("-a $audio_channel") if ( $NOAUDIO != 1 );	
    a_bitrate if ( $AUDIO_BITRATE eq "" );
   
    if ( $FPS eq "" )
    {
        $FPS = get_fps();
    } 
    
    $VIDEONORM = ( $FPS =~ m,2[39]\.9, ) ? "NTSC" : $VIDEONORM;
    if ( $FPS =~ m,29\.97, )
    {
        # since 29.970 is the detected frames per second
        # ask user whether this is a NTSC movie and set variables
        # accordingly
        ask_telecine($FPS) if ( $TELECINE eq "" );
        pdebug ("Telecine in create_extract: $TELECINE");
    } 

    if ( $NOAUDIO  || $NEED_TIME )  
    {
        # || $in_video_magic eq 'mpeg2'(OK V=mpeg2,A=mp3.mpg)  
        # || $in_video_magic eq 'vdr' )
        pwarn(" A tcextract bug do not allow me to extract correct informations from"
            ." this $in_video_magic streams\n") if ( $NEED_TIME  );
        pwarn(" As there is no audio channel, V2divx is unable to extract some informations"
            ." from this Clip\n") if ( $NOAUDIO == 1 ); 
        print " So please, say me how long is this movie (in seconds)?:";
        $rep=<STDIN>;
        chomp($rep);
        $frames=floor($rep*$FPS);
        my $audiosize=$AUDIO_BITRATE*1000*$rep/(1024*1024*8);
        open(EXTRACT,">tmp/extract.text");
        print EXTRACT "[V2dscan] audio frames=$frames, estimated clip length=$rep seconds
        [V2dscan] V: $frames frames, $rep sec @ $FPS fps
        [V2dscan] A: $audiosize MB @ $AUDIO_BITRATE kbps";
        close(EXTRACT);
        system("touch tmp/extract-ok");

    } elsif ( $in_video_magic eq 'mov' ) {
        my $info=`tcprobe -i $VOBPATH/$SAMPLE 2>&1` or mydie "Error when running 'tcprobe -i $VOBPATH/$SAMPLE'";
        $frames = $1 if ( $info =~ m, length: (\d+) frames,);
        my $len = floor($frames/$FPS);
        my $audiosize=$AUDIO_BITRATE*1000*$len/(1024*1024*8);
        pdebug (" Frames = $frames , lenght = $len , audiosize = $audiosize");
        open(EXTRACT,">tmp/extract.text");
        print EXTRACT "[V2dscan] audio frames=$frames, estimated clip length=$len seconds
        [V2dscan] V: $frames frames, $len sec @ $FPS fps
        [V2dscan] A: $audiosize MB @ $AUDIO_BITRATE kbps";
        close(EXTRACT);
        system("touch tmp/extract-ok");
    } else {
        pinfo("$v2d\t Creating :\t\t\t\t   | tmp/extract.text\n");
        my $pid = fork();
        mydie "couldn't fork" unless defined $pid;
        if ($pid)
        {
            my $audio_rate="48000,16,2";
            my $tmp = `tcprobe -i $VOBPATH/$SAMPLE 2>&1` or mydie "Error when running 'tcprobe -i $VOBPATH/$SAMPLE'";
            if ( $tmp =~ m,-e (\d+)\,(\d+)\,(\d+) \[\d+\,\d+\,\d+\],)
            {
                my $bits=$2;
                $bits=16 if ( $bits == 0 );
                my $tmp_channels=($3 > 2) ? 2 : $3;
                $audio_rate="$1,$bits,$tmp_channels";
            }
            pdebug ("\nAudio rate = $audio_rate");
            if ( $audio_format eq 'pcm' ) # Do not need to decode audio
            { 
                if (  $in_video_magic eq 'avi' )
                {
                    chk_wdir if ( $SAMPLE eq "" );
                    $sys = "nice -$nice tcextract -i \"$VOBPATH/$SAMPLE\" -x $audio_format "
                        ."| nice -$nice tcscan -b $AUDIO_BITRATE -x pcm -f $FPS -e $audio_rate "
                        ."2>> tmp/extract.text  >> tmp/extract.text";
                } else {
                    $sys = "cat $VOBPATH/* | nice -$nice tcextract -x $audio_format -t $in_video_magic "
                        ."| nice -$nice tcscan -b $AUDIO_BITRATE -x pcm -f $FPS -e $audio_rate "
                        ."2>> tmp/extract.text  >> tmp/extract.text";
                }
            } elsif (  $audio_format eq "mp3"  
                || $in_video_magic eq "avi") {
                chk_wdir if ( $SAMPLE eq "" );
                $sys = "nice -$nice tcextract -i \"$VOBPATH/$SAMPLE\" -x $audio_format | nice -$nice "
                    ."tcdecode -x $audio_format | nice -$nice tcscan -b $AUDIO_BITRATE -x pcm "
                    ."-e $audio_rate -f $FPS 2>> tmp/extract.text  >> tmp/extract.text";
            } else {
                $sys = "cat $VOBPATH/* | nice -$nice tcextract -x $audio_format -t $in_video_magic "
                    ."| nice -$nice tcdecode -x $audio_format | nice -$nice tcscan -b $AUDIO_BITRATE"
                    ." -x pcm -e $audio_rate -f $FPS 2>> tmp/extract.text  >> tmp/extract.text";
            }
            print "$sys \n" if ( $INFO);
            pdebug ("+ Working directory ".`pwd`);
            system ("nice -$nice $sys") == 0  
                or ( 
                system("touch tmp/extract.finish") == 0 
                    and mydie "Unable to create extract.finish. $!" 
            ) ;
            system("touch tmp/extract.finish");
            wait;
            system("touch tmp/extract-ok");
        } else {
            smily("extract");
        }
    }
    print "\n";
    my $duree=`cat tmp/extract.text`;
    $duree =~ m,estimated clip length=(\d+.\d+) seconds,;
    pinfo("$v2d\t Estimated Clip Length:\t\t\t   | $1 sec\n");
    pdebug ("<--- create_extract");
} # end create_extract

sub calculate_nbrframe
{
    #  ***** Calculate How many Frames to encode ***** #
    my ($nbr_frames,$info,$log)="";
    pdebug ("---> Enter calculate_nbrframe");
    # We need Info about Clip

    if ( -e "tmp/probe.rip" && ! -e "tmp/probe.rip-BAD")
    {  	
        $info = `cat tmp/probe.rip`;
        $log="tmp/probe.rip";
    } else {
        create_extract if (! -f "tmp/extract-ok" 
        || ! -f "tmp/extract.text" );
        $info = `cat tmp/extract.text`;
        $log="tmp/extract.text";
    }

    ($info =~ m,V: (\d+) frames,) 
        or mydie "Unable to find number of frames to encode in $log" ;
    $TOT_FRAMES = $1;

    if ( $FPS eq "" )
    {
        $FPS=get_fps();
    }

    $VIDEONORM = ( $FPS =~ m,2[39]\.9, ) ? "NTSC" : $VIDEONORM;
    if ( $FPS =~ m,29\.97,  )
    { 
        ask_telecine if ( $TELECINE eq "" );
        pdebug ("+++ Telecine in calculate_nbrframe: $TELECINE");
    }
    $nbr_frames= floor($TOT_FRAMES - ($START_SEC+$END_SEC)*$FPS);
    pdebug ("Number of frames: $nbr_frames");
    pdebug ("<--- calculate_nbrframe");
    return($nbr_frames);
} # end calculate_nbrframe

sub calculate_bitrate
{	
    #  ********** Calculate Bitrate ****************
    pdebug ("--->  Enter calculate_bitrate");
    my($info,$log)="";
    # We need Audio Bitrate
    a_bitrate if ( $AUDIO_BITRATE eq ""  );

    # And Also Info about Clip
    if ( -e "tmp/probe.rip" && ! -e "tmp/probe.rip-BAD" )
    {       
        $info = `cat tmp/probe.rip`;
        $log="tmp/probe.rip";
    } else {
        if (! -e "tmp/extract-ok" ||  ! -e "tmp/extract.text" )
        {
            create_extract;
        }
        $info = `cat tmp/extract.text`;
        $log="tmp/extract.txt";
    }

    ( $info =~ m,frames\, (\d+) sec @ ,) or mydie "Unable to find Video Runtime in $log";
    my $fulltime=$1;
    mydie " ERROR : You said end credits is $END_SEC sec. long,".
        " but this movie in only $fulltime sec." if ( $fulltime < $END_SEC );
    $RUNTIME=$fulltime - ($START_SEC+$END_SEC);
    if ( $NOAUDIO != 1 )
    {
        ( $info =~ m, A: (\d+\.*\d*) MB @ ,) or mydie "Unable to find Audio Size in $log";
        $AUDIO_SIZE = $1*$RUNTIME/$fulltime;
        ( $info =~ m, A: .* MB @ (\d+) kbps,) or mydie "Unable to find audio bitrate in $log";
        $AUDIO_SIZE = $AUDIO_SIZE*$AUDIO_BITRATE/$1;
        $AUDIO_SIZE=2*$AUDIO_SIZE if ( $AC2 ne "" );
    } else {
        $AUDIO_SIZE = 0;
    }

    ask_filesize if (  $FILESIZE eq "" );

    $BITRATE = floor(($FILESIZE - $AUDIO_SIZE)/$RUNTIME * 1024 * 1024 * 8 / 1000);
    if ($BITRATE < 20)
    {	
        pwarn("\n#### ATTENTION ####\n\tCalculated bitrate is $BITRATE kbps, \n".
        "which does not make much sense, I'll use 700 kbps instead. \nFilesize will".
        " not match your preferred FILESIZE. Sorry\n");
        print " Press Enter ->";
        my $junk=<STDIN>;
        $BITRATE = 700;
    }
    # audio_rescale for CLUSTER mode
    # $audio_rescale=audiorescale();
    pdebug ("Bitrate : $BITRATE");

    pdebug ("<--- calculate_bitrate");

} # END calculate_bitrate


sub aviencode
{
    # ********** Main Avi encode ***************
    pdebug ("--->  Enter aviencode");
    my ($in_video_magic,$in_video_codec)=videoformat("$VOBPATH/$SAMPLE");
    my ($sys,$stream_opt,$final_params,$filter)="";
    my ($stream_unit)=0;
    my $add_logo="";

    # Zooming MUST have been call before aviencode
    # zooming will give us $FILESIZE $BITRATE and 	Zoom
    # zH zW and row are the -B parameters
    if ($Zoom_mode eq "B")
    {   
        my $row=16;
        my $zH=floor(($Yaxis-2*$tb-$NYaxis)/$row);
        my $zW=floor(($Xaxis-2*$lr-$NXaxis)/$row);
        $PARAMS .=" -$Zoom_mode $zH,$zW,$row";
    } else { # $Zoom_mode eq Z 
        $PARAMS .=" -$Zoom_mode ${NXaxis}x$NYaxis";
    }
    printinfo if ( ! -f "tmp/cluster.args");
    # We are in Cluster Mode and there is no cluster args file ???
    # It means that we are in cluster mode on the master 
    # We call the cluster subroutine and will never return here
    cluster if ( $CLUSTER ne "NO" && ! -f "tmp/cluster.args");

    my $from_frames=$START_SEC*$FPS;
    my $to_frames=$TOT_FRAMES-$END_SEC*$FPS;
    if (  $CLUSTER ne "NO" )	
    {
        # We are on an encoding node
        # We get the W parameters from tmp/cluster.args
        my $cluster=`cat tmp/cluster.args`;
        chomp($cluster);
        $node=`cat tmp/cluster.args| awk -F, '{print \$1}'`;
        # we can free the tmp/cluster.args for other nodes now
        system("touch tmp/node_started");
        chomp($node);
        $final_params="$PARAMS -W $cluster,tmp/file.nav";
        chomp($CLUSTER);
        $stream_unit=$CLUSTER;
    } else {
        # Encode all frames 
        # (only if $END_SEC AND $START_SEC < 600 ) , we'll split after
        if ( $START_SEC < 600 )
        {  
            $from_frames=0;
        }
        if ( $END_SEC < 600 )
        { 
            $to_frames=$TOT_FRAMES; 
        }
        $final_params="$PARAMS -c $from_frames-$to_frames";
    }

    system("rm tmp/*.done  2> /dev/null");

    my $start_frames_logo=floor(($START_SEC+$STARTLOGO)*$FPS);
    my $end_frames_logo=floor($ADDLOGO*$FPS+$start_frames_logo);

    # In cluster mode we MUST encode each sequence unit separatly 
    # In normal mode $stream_unit==0
    my $i=0;

    for ( $i=$stream_unit ; $i >= 0 ; $i--  )
    {      
        print("*** SEQ UNIT = $i ********\n***  Cluster NODE ".
            "number : $node ******* \n")  if (  $CLUSTER ne "NO" );
        if ( $ADDLOGO && $i == 0 && $CLUSTER eq "NO" )
        {
            $add_logo="logo=file=$LOGO:posdef=$POSLOGO:rgbswap=$RGBSWAP".
                ":range=$start_frames_logo-$end_frames_logo,";
        } else {
            $add_logo="";
        }
        my $subtitle=""; # -J opts OR srtsubrip 
        $subtitle=$SUB_TITLE if ( $SUB_TITLE =~ m,extsub,);  # Bug 
        $filter=$DEINTL.$add_logo.$subtitle."hqdn3d,";# Use now hqdn3d filter                        
        # WE NEED the next 4 lines  because in non cluster mode 
        # we do not need $stream_unit value, 
        # And WE want encode all the sequences unit (so, no -S option) .
        if ( $stream_unit != 0 )
        {	
            $stream_opt="-S $i,all";
        } else {
            $stream_opt="";
        }
        $PARAMS =~ m/-a (\d) .*/;
        my $audio_params="-a $1";
        my $audio_format=audioformat("$audio_params");
        # Only ONE FILE ($SAMPLE) allowed if input Video type is AVI, QT, DV , MPEG2
        my $vobpath=$VOBPATH;
        $vobpath="$VOBPATH/$SAMPLE" if ( $in_video_magic eq "divx" || 
            $in_video_magic eq "mov" || 
            $in_video_magic eq "mpeg2"  || 
            $in_video_magic eq "dv"  );
        # Let transcode find it, if input audio type is "declared" as PCM
        $audio_format="" if ( $audio_format eq "pcm" );
        $audio_format="vob" if ( $in_video_codec eq "vob" );
        # Add NTSC demuxer opts
        $final_params=$final_params." -M 2" if ( $VIDEONORM eq "NTSC" && !  ( $DEINTL =~ m,ivtc,));
        # export fps is 23.9 if ivtc
        $final_params=$final_params."  -M 0 -f 23.97,1 " if ( $DEINTL =~ m,ivtc,);
        # if $CLUSTER = NO use psu_mode. I know this is longer and not always necessary
        # but there is no way to know if it is or not necessary
        $final_params=$final_params." --psu_mode  --no_split" if ( $CLUSTER eq "NO" && $VIDEONORM eq "NTSC" );
        #$final_params=$final_params." -M 0 -f 23.976" if ( $DEINTL =~ m,ivtc,);
        # $final_params=$final_params." --psu_mode --nav_seek tmp/file.nav --no_split " if ( $CLUSTER eq "NO" && $VIDEONORM eq "NTSC" );
        $clust_percent=" --a52_dolby_off " if ( $VIDEONORM eq "NTSC" && $CLUSTER eq "NO" );
        $AO="" if ( $AO eq "mp3" );;
        # We DO NOT encode audio on nodes
        $AO="null" if ( $CLUSTER ne "NO" );
        $audio_format="null" if ( $CLUSTER ne "NO" );
        my ($aogg_output)="";
        $aogg_output="-m tmp/ac1.$TITLE.ogg" if ( $AO eq "ogg" );
        my $fine_sync="";
        $fine_sync=tcsync($audio_params) if ( $CLUSTER eq "NO");
        pdebug ("Fine sync =  $fine_sync");

        # And Now ... Let the Rolling Stones

        if (! -e "tmp/1-${TITLE}${node}_${i}.finish")
        {
            unlink("tmp/2-${TITLE}${node}_${i}.finish");
            unlink ("tmp/1-${TITLE}${node}.PB");
            pinfo("$v2d\t Encode: $vobpath Pass One ....\n");
            $sys = "transcode -i $vobpath $stream_opt $fine_sync $clust_percent $final_params -w $BITRATE,$keyframes".
                " -J ${filter}astat=\"tmp/astat\" -y $DIVX,null $DIVX_OPT -x ".
                "$in_video_codec,$audio_format $VID_OPT -R 1,tmp/$DIVX.${TITLE}${node}_${i}.log ".
                " -o /dev/null"; 
            print "$sys \n" if ( $INFO);
            my $pid = fork();
            mydie "couldn't fork" unless defined $pid;
            if ($pid)
            {
                wait;
                if ( -f "tmp/1-${TITLE}${node}.PB")
                {    
                    mydie("\t\t**** Pass One failed on node(${node}),seq(${i}) ****.".
                        "\n\tFailed to execute:\n $sys")
                }
                system("touch tmp/1-${TITLE}${node}_${i}.finish");
            } else {
                system("nice -$nice $sys")==0 or system("touch tmp/1-${TITLE}${node}.PB");
                print "\n";
                exit(0);
            }
        } else {
            pwarn("${TITLE}${node}_${i} already encoded, remove ".
                "\"tmp/1-${TITLE}${node}_${i}.finish\" to reencode \n");
        }
        my $audio_rescale=audiorescale();
        if (! -e "tmp/2-${TITLE}${node}_${i}.finish")
        {
            unlink ("tmp/2-${TITLE}${node}.PB");
            $filter=$filter."dnr,normalize";	
            unlink("tmp/merge.finish");	
            unlink("tmp/finish");
            pinfo("$v2d\t Encode: $vobpath Pass Two ....\n");
            $sys = "transcode -i $vobpath $fine_sync $stream_opt $clust_percent $final_params -s $audio_rescale ".
                "-w $BITRATE,$keyframes -b $AUDIO_BITRATE,0 -y $DIVX,$AO $aogg_output ".
                "$DIVX_OPT -x $in_video_codec,$audio_format $VID_OPT -J $filter ".
                "-R 2,tmp/$DIVX.${TITLE}${node}_${i}.log -o tmp/2-${TITLE}${node}_${i}.$EXT";
            print "$sys \n" if ( $INFO);
            my $pid = fork();
            mydie "couldn't fork" unless defined $pid;
            if ($pid)
            {    
                wait;
                if ( -f "tmp/2-${TITLE}${node}.PB") 
                { 
                    mydie("\t\t**** Pass Two failed on node(${node}),seq(${i}) ****.".
                        "\n\tFailed to execute:\n $sys"); 
                }        
                system("touch tmp/2-${TITLE}${node}_${i}.finish");
            } else {	
                system("nice -$nice $sys")==0 or system("touch tmp/2-${TITLE}${node}.PB");
                print"\n";
                exit(0);
            }
        } else {	
            pwarn("${TITLE}${node}_${i} already encoded, remove ".
                "\"tmp/2-${TITLE}${node}_${i}.finish\" to reencode \n");
        }
    } # end boucle for
    if ( $CLUSTER ne "NO")
    {       
        print ("Finish ... Wait \n ");
        sleep(3);
    }
    pdebug ("<--- aviencode");
} # END Aviencode


sub merge
{	
    # ********* MERGING ( and syncing )Function **************#
    # Do not merge sequence unit if not cluster mode

    my ($sys)="";
    # from now the output is 23.97 if we have use ivtc
    $FPS=23.970 if ( $DEINTL =~ m,ivtc,);

    pdebug ("--->  Enter merge");
    if (! -e "tmp/merge.finish"  && $CLUSTER  ne "NO" )
    {       
        unlink("tmp/sync.finish"); 
        unlink("tmp/finish");
        unlink("tmp/cutoff.finish");
        pinfo("$v2d\t Merging the sequence units\n");
        my $pid = fork();
        mydie "couldn't fork" unless defined $pid;
        if ($pid)
        {	
            wait;
            system("touch tmp/merge.finish");
        } else {	
            my $i=0;
            # $CLUSTER  is known because we've pass through aviencode before
            for ( $i=$CLUSTER ; $i >= 0 ; $i-- )
            { 	
                pinfo("$v2d\t Seq. unit :\t\t\t   | $i\n");
                $sys = "avimerge -i tmp/2-*_$i.$EXT -o tmp/tmp_movie_$i.$EXT";
                print "$sys \n" if ( $INFO);
                system("nice -$nice $sys 1> /dev/null");
            }
            if ( $CLUSTER > 0 )
            {
                $sys = "avimerge -i tmp/tmp_movie_*.$EXT -o tmp/2-$TITLE.$EXT ".
                    "&& rm tmp/tmp_movie_*.$EXT ";
                print "$sys \n" if ( $INFO);
                system("nice -$nice $sys 1> /dev/null");
            } else {
                print "Renaming tmp/tmp_movie_0.$EXT tmp/2-$TITLE.$EXT\n" if ( $INFO );
                rename("tmp/tmp_movie_0.$EXT","tmp/2-$TITLE.$EXT");
            }
            exit(0);
        }
    } else {       
        pwarn("*.$EXT of $TITLE are already merge ... remove ".
            "\"tmp/merge.finish\" to re-merge it\n") if ( $CLUSTER ne "NO");
    }
    pdebug ("<--- merge");

    # Now if in cluster mode with Logo, we need to cut off the begining of the
    # without logo encoded movie ( we DO NOT use avisplit to do that )
    if ( $CLUSTER ne "NO" && $ADDLOGO != 0  )
    {
        makelogo;
    }

    ######## add audio in cluster mode or in OGM container #############

    pdebug ("--->  Enter synchro");
    my $audio_rescale=audiorescale();
    if (! -e "tmp/sync.finish" )
    {	
        unlink("tmp/finish");
        unlink("tmp/sync.done") if ( -e "tmp/sync.done" );

        my $to_frames=floor($TOT_FRAMES-$END_SEC*$FPS);

        my ($in_video_magic,$in_video_codec)=videoformat("$VOBPATH/$SAMPLE");
        my ($in2_video_magic)=videoformat("tmp/2-$TITLE.$EXT");

        $PARAMS =~ m/-a (\d) .*/;
        my $audio_params="-a $1";
        my $audio_format=audioformat("$audio_params");

        # Only ONE FILE allowed if input Video type is AVI, QT, DV , MPEG2
        my $vobpath=$VOBPATH;
        $vobpath="$VOBPATH/$SAMPLE" if ( $in_video_magic eq "divx" ||
            $in_video_magic eq "mov" ||
            $in_video_magic eq "mpeg2"  ||
            $in_video_magic eq "dv"  );
        # Let transcode find it, if input audio type is "declared" as PCM
        $audio_format="" if ( $audio_format eq "pcm" );
        $audio_format="vob" if ( $in_video_codec eq "vob" );
        
        my $pid = fork();
        mydie "couldn't fork" unless defined $pid;
        if ($pid)
        {       
            wait;
            system("touch tmp/sync.finish");
        } else {
            if ( $AO eq "ogg" )
            {
                $sys = "transcode -q 0 -i $vobpath $audio_params -b $AUDIO_BITRATE ".
                    "-c 0-$to_frames -s $audio_rescale  -y raw,ogg ".
                    "-m tmp/ac1.${TITLE}.ogg";
                pinfo("$v2d\t Extracting Ogg Audio (this can be long)\n");
            } else {
                $sys = "transcode -q 0 -p $vobpath $audio_params -b $AUDIO_BITRATE ".
                    "-c 0-$to_frames -s $audio_rescale -i tmp/2-$TITLE.$EXT ".
                    "-P 1 -x $in2_video_magic,$audio_format -y raw -o tmp/2-${TITLE}_sync.$EXT -u 50";
                pinfo("$v2d\t Merging Video and Audio stream\n");
            }
            my $pid = fork();
            mydie "couldn't fork" unless defined $pid;
            if ($pid)
            {   print "$sys \n" if ( $INFO);
                system("nice -$nice $sys")==0 
                    or ( system("touch tmp/extract_ogg1.finish") && mydie "Unable to run\'$sys\'");
                system("touch tmp/extract_ogg1.finish");
                wait;
            } else {
                smily("extract_ogg1");
            }

            if ( $AO eq "ogg" )
            {
                pinfo("$v2d\t Merging Video and Ogg Audio stream\n");
                ogmerge("tmp/2-$TITLE.$EXT","tmp/ac1.$TITLE.ogg");
                rename("tmp/2-$TITLE.$EXT","tmp/2-${TITLE}_sync.$EXT");
            }
            
            # remove the 2 audio channel finish flag to permit to remerge it
            unlink("tmp/enc_audiochannel2.finish") if ( -e "tmp/enc_audiochannel2.finish");
            exit(0);
        }
    } else {	
        pwarn("$TITLE is already sync, remove \"tmp/sync.finish\" to re-sync it\n");
    }
    pdebug ("<--- synchro");

} # END merge

sub twoac
{
    # ######## Encode the optionnal second audio channel ########## #
    pdebug ("---> Enter twoac");
    my($sys);
    return() if ( $AC2 eq "" );
    audioformat("-a $AC2");
    my $audio_rescale=audiorescale();
    $AO="" if ( $AO eq "mp3");
    my ($ac2_output)="-o add-on-AC2.$EXT";
    $ac2_output="-m add-on-AC2.$EXT";

    if ( ! -e "tmp/enc_audiochannel2.finish" )
    { 	 
        unlink("tmp/finish") if ( -e "tmp/finish");
        unlink("tmp/audiochannel2.finish" ) if ( -e "tmp/audiochannel2.finish" );
        pinfo("$v2d\t Extracting and encoding the second audio channel\n");
        $sys="transcode -i $VOBPATH -x null -s $audio_rescale -b $AUDIO_BITRATE ".
            "-g 0x0 -y raw,$AO -a $AC2 $ac2_output -u 50";
        print "$sys \n" if ( $INFO);
        system("nice -$nice $sys")==0 or mydie "Unable to encode the second audio channel";
        system("touch tmp/enc_audiochannel2.finish") ;
        print"\n";
    }
    if ( ! -e "tmp/audiochannel2.finish" )
    {
        unlink("tmp/finish") if ( -e "tmp/finish");
        pinfo("$v2d\t Merging the second audio channel\n");
        $sys="avimerge -i tmp/2-${TITLE}_sync.$EXT -o tmp/3-${TITLE}_2ac.$EXT"
            ."  -p add-on-AC2.$EXT";
        $sys="ogmmerge -o tmp/3-${TITLE}_2ac.$EXT tmp/2-${TITLE}_sync.$EXT"
            ."  add-on-AC2.$EXT" if ( $AO eq "ogg" );
        print "$sys \n" if ( $INFO);
        system("nice -$nice $sys 1> /dev/null")==0 
            or mydie "Unable to merge movie and second audio channel";
        rename("tmp/3-${TITLE}_2ac.$EXT","tmp/2-${TITLE}_sync.$EXT") 
            && system("touch tmp/audiochannel2.finish") ;
    }
    pdebug ("<--- twoac");
} # END 2ac

sub finish
{
    # ############ Finish the work ############# #
    pdebug ("---> Enter finish");
    my($sys)="";
    if (! -e "tmp/finish")
    {
        my $nbr_frames=calculate_nbrframe;
        my $from_frames=$START_SEC*$FPS;
        my $to_frames=$nbr_frames+$from_frames;
        if ( $CLUSTER eq "NO" 
            && ( $END_SEC == 0 || $END_SEC > 600) 
            &&  ($START_SEC == 0  || $START_SEC > 600)
             )
        {
            # We do not split if: 
            # user wants to encode all the movie
            # or end credits or begin credits are
            # longer than 5 minutes (aviencode has no encode it)
            #
            pinfo("$v2d\t Renaming tmp/2-${TITLE}_sync.$EXT $TITLE.$EXT\n");
            rename("tmp/2-${TITLE}_sync.$EXT","$TITLE.$EXT");
        } else {
            # If the movie is 2 Hours long and you want to cut from
            # 30 seconds up to 1:55 hours, then you will pass
            # START_SEC = 30;
            # END_SEC = ( 5 * 60 ); # to cut 5 minutes from the end 
            #                        # the movie
            my $start_of_file_hour = sec_to_hour($START_SEC);
            my $end_of_file_hour = 
            sec_to_hour( int($TOT_FRAMES / $FPS) - $END_SEC );

            $sys="avisplit -t $start_of_file_hour-$end_of_file_hour -i  tmp/2-${TITLE}_sync.$EXT ".
                "-o $TITLE.$EXT && mv $TITLE.$EXT-0000 $TITLE.$EXT ";
            $sys="ogmsplit -c $start_of_file_hour-$end_of_file_hour tmp/2-${TITLE}_sync.$EXT ".
                "-o $TITLE.$EXT && mv $TITLE-000001.$EXT $TITLE.$EXT" if ( $AO eq "ogg" );
            pinfo("$v2d\t Splitting the result to $nbr_frames frames.\n");
            print "$sys \n" if ( $INFO);
            system("nice -$nice $sys");
        }
        system("touch tmp/finish");
    }

    # substract unixtimestamp from our original timestamp
    my $end_time = time - $start_time;
    my $f_end_time = format_date($end_time,"hours");
    print "Number of Hours: $f_end_time \n";
    if ( -x "/usr/bin/flite" )
    {
        # say it outloud using "flite" speech synthetizer
        system("/usr/bin/flite -t 'Number of hours $f_end_time'");
    }

    print " Now take a look at the end of $TITLE.$EXT\n".
    "\t If for some reason the movie file doesn't reach the end credits,".
    " just edit tmp/V2divx.conf, decrease the \$END_SEC value, ".
    "remove the tmp/finish file and then run V2divx without ".
    "parameters.";
    print " Is you divx file OK? (y/N): ";
    my $rep=<STDIN>;
    chomp($rep);
    if ($rep eq "O" or $rep eq "o" or $rep eq "y" or $rep eq "Y")
    {
        print "May I clean the tmp directory and other temporaries and log files ? (y/N): ";
        $rep=<STDIN>;
        chomp($rep);
        if ($rep eq "O" or $rep eq "o" or $rep eq "y" or $rep eq "Y")
        {
            rename("tmp/dvdtitle",$VOBPATH."/dvdtitle") if ( -e "tmp/dvdtitle" );
            rename("tmp/probe.rip",$VOBPATH."/probe.rip") if ( -e "tmp/probe.rip" );
            system("/bin/rm -rf tmp/*  db *.bak video_s._-j_* audio_sample* add-on-AC2.$EXT");
        }
    }
    pdebug ("<--- finish");
    mydie $NORM."Bye !!!";
}  # ENd finish

sub a_bitrate
{ 
    #   *************   Get Audio Bitrage ************ #  	
    pdebug ("--->  Enter Audio_bitrate");
    $AO="null"  if ( $NOAUDIO == 1 ); 
    $AUDIO_BITRATE ='null' if ( $NOAUDIO == 1 );
    while ( $AUDIO_BITRATE ne 'null' 
        && $AUDIO_BITRATE ne 32 
        && $AUDIO_BITRATE ne 48 
        && $AUDIO_BITRATE ne 64 
        && $AUDIO_BITRATE ne 96 
        && $AUDIO_BITRATE ne 128 
        && $AUDIO_BITRATE ne 256 )
    {
        print " Enter the desired Output Audio Bitrate (Kb/s) [default:128]: ";
        $AUDIO_BITRATE=<STDIN>;
        chomp($AUDIO_BITRATE);
        if ( $AUDIO_BITRATE eq "" )
        {
            $AUDIO_BITRATE=128;
            last;
        }
    }
    $AUDIO_BITRATE=0 if  ( $AUDIO_BITRATE eq 'null');
    open (CONF,">>tmp/V2divx.conf");
    print CONF "#AUDIO_BITRATE:$AUDIO_BITRATE\n";
    print CONF "#AO:$AO\n";
    close CONF;	
    pdebug ("<--- Audio_bitrate");
}

sub readconf
{
    # ******** Read actual conf ********** #
    pdebug ("---> Enter readconf");
    if ( -f "tmp/V2divx.conf")
    {
        open (CONF,"<tmp/V2divx.conf");
        while (<CONF>)
        {       
            chomp;
            s[/\*.*\*/][];      #  /* comment */
            s[//.*][];          #  // comment
            s/^#//;		    # 	Remove the first #
            s/#.*//;            #  # comment
            s/^\s+//;           #  whitespace before stuff
            s/\s+$//;           #  whitespace after stuff
            next unless length; #  If our line is empty, we ignore it
            my ($var_name, $value) = split(/:/, $_, 2);


            pdebug (" + $var_name  = $value");
            $VOBPATH=$value if ($var_name eq "VOBPATH");
            $END_SEC=$value if ($var_name eq "END_SEC");
            $START_SEC=$value if ($var_name eq "START_SEC");
            $AUDIO_BITRATE=$value if ($var_name eq "AUDIO_BITRATE");
            $FILESIZE=$value if ($var_name eq "FILESIZE");
            $ADDLOGO=$value if ($var_name eq "ADDLOGO");
            $STARTLOGO=$value if ($var_name eq "STARTLOGO");
            $POSLOGO=$value if ($var_name eq "POSLOGO");
            $TITLE=$value if ($var_name eq "TITLE");
            $PARAMS=$value if ($var_name eq "PARAMS");
            $SUB_TITLE=$value if ($var_name eq "SUB_TITLE");
            $DEINTL=$value if ($var_name eq "DEINTL");
            $AC2=$value if ($var_name eq "AC2");
            $VIDEONORM=$value if ($var_name eq "VIDEONORM");
            $TELECINE=$value if ($var_name eq "TELECINE");
            $CLUSTER=$value if ($var_name eq "CLUSTER");
            $DIVX=$value if ($var_name eq "DIVX");
            $DIVX_OPT=$value if ($var_name eq "DIVX_OPT");
            $AO=$value if ($var_name eq "AO");
            $EXTSUB=$value if ($var_name eq "EXTSUB");
            $TV_RUNTIME=$value if ($var_name eq "TV_RUNTIME");
            $TVREC=$value if ($var_name eq "TVREC");
            $QUALITY=$value if ($var_name eq "QUALITY");

        #    $$var_name = $value;
        #    pdebug (" \$$var_name = $value");
        }
        close(CONF);
    }
    pdebug (" VOBPATH = $VOBPATH");
    # We need to retest $AO in case user has change
    # in tmp/V2divx.conf so we recall check_sys
    check_system;

} # end readconf


sub get_params
{  	
    #  ************* Get Needed parameters ************ #
    pdebug ("--->  Enter get_params");
    if ( ! -f "tmp/V2divx.conf")
    {
        # We are in Quick Mode
        my $i = 0;
        if ( $ARGV[$i])
        {	
            $VOBPATH= $ARGV[$i];
            $i ++;
        } else {	
            system ("echo \"$readme\" | less -R ");
            if ( $DVDTITLE eq "" ) 
            {
                print $urldvdtitle;
            }
            exit(0);
        }
        mydie "Path: $VOBPATH does not exist." if (! -e $VOBPATH);
        umask(000); # resets current umask values. Luis Mondesi
        mkdir ("tmp",01777);
        # Luis Mondesi <lemsx1AT_NOSPAM_hotmail.com> 
        # how does the script knows that the "sample" keyword
        # was given?
        if ($ARGV[$i] > 1)
        {	
            $FILESIZE = $ARGV[$i];
        } else {
            mydie "\'$ARGV[$i]\' is not a valid FILESIZE.\n Please supply FILESIZE".
                " \n\t or \"sample\" if you want to create samples for cropping.";
        }
         check_system;
    } else {   
        # We are in 'continue' mode
        readconf;
    }

    $AUDIO_BITRATE=128 if ( $AUDIO_BITRATE eq "" );	
    tv_recorder if ( $TVREC );
    chk_wdir;
    ask_clust if ( $CLUSTER eq ""  );
    create_nav if  ( $CLUSTER ne "NO" );

    #   For Quick mode only .....	

    if ( $PARAMS eq ""  ) 
    {
        $TITLE="movie" if ( $TITLE eq "" );
        findclip;
        # we need only $audio_channel but 
        my ($number_of_ac,$audio_channel,@achannels)=get_audio_channel("all") ;
        my $is_interlaced=interlaced;
        if ( $is_interlaced eq "yes" )
        {
            my $PP=`transcode -J pp=lb -c 9-11  2>&1 | grep failed`;
            if ( $PP eq "" )
            {	
                $DEINTL="pp=lb,";
            } else {
                $DEINTL="32detect=force_mode=3,";
            } 
        }
        $PARAMS .= "-a $audio_channel " if ( $audio_channel ne '' );
        $PARAMS .= "-j $tb,$lr";
    }
    zooming;
    # reset the deinterlacer to ivtc in case this is a NTSC 
    $DEINTL="ivtc,32detect=force_mode=3,decimate," if ( $TELECINE && $DEINTL ne "" );
    if ( $ADDLOGO eq ""  && -e $LOGO )
    {
        my $LG=`transcode -J logo=help -c 9-11  2>&1 | grep rgbswap`;
        if ( $LG ne "" )
        {
            if ( $TIMELOGO+$STARTLOGO > $RUNTIME)
            {
                $ADDLOGO=$RUNTIME-$STARTLOGO;
            } else {
                $ADDLOGO=$TIMELOGO;
            }
        } else {
            $STARTLOGO="";
            $POSLOGO="";
            pwarn("Transcode is not compile with ImageMagick.\nUnable to encode your Logo $LOGO\n"); 
            $ADDLOGO=0;
            sleep(1);
        }
    }

    # End Quick mode Configuration


    $TITLE="movie" if ( $TITLE eq "" );

    open(CONF,">tmp/V2divx.conf");
    if ( $VOBPATH ne ""  ) {
        print CONF "#VOBPATH:$VOBPATH# DO NOT MODIFY THIS LINE\n";
    }
    if ( $END_SEC ne ""  ) {
        print CONF "#END_SEC:$END_SEC\n";
    }
    if ( $START_SEC ne ""  ) {
        print CONF "#START_SEC:$START_SEC\n";
    }
    if ( $AUDIO_BITRATE ne ""  ) {
        print CONF "#AUDIO_BITRATE:$AUDIO_BITRATE\n";
    }
    if ( $AO ne ""  ) {
        print CONF "#AO:$AO\n";
    }
    if ( $FILESIZE ne ""  ) {
        print CONF "#FILESIZE:$FILESIZE\n";
    }
    if ( $ADDLOGO ne ""  ){
        print CONF "#ADDLOGO:$ADDLOGO # THIS VALUE IS THE DURING TIME OF LOGO (in sec.)\n";
    }
    if ( $STARTLOGO ne "" ){
        print CONF "#STARTLOGO:$STARTLOGO\n";
    }
    if ( $POSLOGO ne ""  ){
        print CONF "#POSLOGO:$POSLOGO\n";
    }
    if ( $TITLE ne "" ){
        print CONF "#TITLE:$TITLE # DO NOT MODIFY THIS LINE\n";
    }
    if ( $PARAMS ne "" ) {
        print CONF "#PARAMS:$PARAMS# YOU MUST KNOW WHAT YOU DO IN THIS LINE\n";
    }
    if ( $CLUSTER ne "" ) {
        print CONF "#CLUSTER:$CLUSTER # YOU CAN REMOVE BUT NOT MODIFY THIS LINE\n";
    }
    if ( $SUB_TITLE ne ""  ) {
        print CONF "#SUB_TITLE:$SUB_TITLE\n";
    }
    if ( $DEINTL ne ""  ) {
        print CONF "#DEINTL:$DEINTL\n";
    }
    if ( $AC2 ne "" ) {
        print CONF "#AC2:$AC2\n";
    }
    if ( $TELECINE ne ""  ) {
        print CONF "#TELECINE:$TELECINE\n";
    }
    if ( $TVREC ne ""  ) {
        print CONF "#TVREC:$TVREC\n";
    }
    if ( $QUALITY ne ""  ) {
        print CONF "#QUALITY:$QUALITY\n";
    }
    if ( $TV_RUNTIME ne ""  ) {
        print CONF "#TV_RUNTIME :$TV_RUNTIME\n";
    }
    if ( $DIVX ne ""  ) {
        print CONF "#DIVX:$DIVX\n";
        print CONF "#DIVX_OPT:$DIVX_OPT\n";
    }

    close(CONF);

    pdebug ("<--- get_params");
} # end get PARAMS


sub audioformat
{
    # ##################### Audio Input format ###################
    pdebug ("---> Enter audioformat");
    if ( $NOAUDIO == 1 )
    {
        my $audio_format="null";
        return($audio_format);
        pdebug ("<--- Audio_format");
    }
    $_[0] ="-a 0" if (! defined($_[0]));
    return("null") if ( $_[0] eq "-a null");
    my $audio_format=`tcprobe -i "$VOBPATH/$SAMPLE" 2> /dev/null` 
        or mydie "Problem when running \'tcprobe -i ".$VOBPATH."/".$SAMPLE."\'";
    ( $audio_format =~ m,audio track: $_[0] [^n]*n 0x(\d+) .*,) 
        or mydie "Unable to find audio channel ".$_[0]." format";
    my $tmp=$1;
    SWITCH: 
    {
        # FIXME
        if ( $tmp == 2000 ) {
            $audio_format="ac3" ; last SWITCH;
        }
        if (  $tmp == 1 ) {
            $audio_format="pcm" ; last SWITCH;
        }
        if ( $tmp == 10001 ) {
            $audio_format="pcm" ; last SWITCH;
        }
        if ( $tmp eq "1000F" ) {
            $audio_format="dts" ; last SWITCH;
        }
        if ( $tmp == 55  || $tmp == 50 ) # mpeg2ext is mp3
        {
            $audio_format="mp3";
            my $MP3=`transcode -x null,mp3 -c 0-1 2>&1 | grep failed`;
            mydie("\n *******   WARNING !! *************\n It seems that your transcode ".
                "is'nt compiled with lame , it's not able to encode this audio channel \n\n")
                 if  ( $MP3 ne "" );
            last SWITCH;
        }
        mydie "Unable to find a known audio format ($tmp is unknown)";
    }
    pdebug ("<--- Audio_format");
    return($audio_format);
}

sub videoformat
{    
    my $in_video_magic=""; # reset var
    my $in_video_codec=""; # reset var
    pdebug ("---> Enter videoformat");
    pdebug ("transcode -c 0-1  -i $_[0]");
    my $probe=`transcode -c 0-1 -i $_[0] 2>&1 `;
    $in_video_magic=$1 if ( $probe =~ m,V=([^\|.]*)\|A=,);
    pdebug ("Type of video: $in_video_magic");
    if ( $in_video_magic eq 'mov' && $DIVX_OPT ne "mpeg4" )
    {
        # FIXME .. at least for dvc video format ?
        $VID_OPT='-z -k';
        $REV_VID=' ';
        # Make nothing ?? FIXME		$RGBSWAP=0;
    }
    if ( $in_video_magic =~ m/^.{0,1}null.{0,1}$/ && $SVIDEO_FORMAT eq "" )
    { 
        # Trying with tcprobe .. 
        # in this case We need audio_codec also !!
        pwarn($v2d."\tTranscode failed to find Video format, trying with tcprobe\n ".
            "You could also override with --source-video-format='TYPE'. See Help for more\n");
        my $probe2=`tcprobe -i $_[0] 2>&1`;
        $in_video_magic='divx' if ( $probe2 =~ m,codec=[dD][xX]50,);
        $in_video_magic='divx' if ( $probe2 =~ m,codec=[dD][iI][vV][3xX],);
        # DOES NOT WORK $in_video_magic='avi' if ( $probe2 =~ m,codec=[Ii][vV]32,);
        pdebug ("tcprobe has detect : $in_video_magic");
    }
 
    # assigned the command line option --source-video-format
    # to in_video_codec
    if ( $SVIDEO_FORMAT ne '' ) 
    {
        # backward compatibility:
        $in_video_magic=$SVIDEO_FORMAT;
        $in_video_codec=$SVIDEO_FORMAT;
    }
   
    $in_video_codec="$in_video_magic";
    pdebug ("<--- videoformat : $in_video_magic");
    mydie "$v2d\t$_[0]: Unknown file type. ".
    "Please use --source-video-format='TYPE' to force a given type" 
    if ( $in_video_magic =~ m/^.{0,1}null.{0,1}/ && $SVIDEO_FORMAT eq '' );

    if ( $in_video_magic eq 'dv' )   # Need to know the container (raw,avi?)
    {
        pdebug ("DV File ... need to know the container ...");
        #        my $probe2=`tcprobe -i $_[0] 2>&1`;
        #        
        #        NEED_TIME = 1  means that tcextract does not work for this
        #        Video format and then user MUST enter video run time :-( 
        $NEED_TIME=1; #   if ( $probe2 =~ m,Digital Video,);
        $VID_OPT="";    # Bug transcode ? Crash if we use -V
        # PB with Logo if input is DV ? 
    }

    # WARNING Sometime transcode is unable to find formats ... 
    # use tcprobe (videoformat_old) in this case to 
    return($in_video_magic,$in_video_codec);
}


sub make_sample
{
    #******************* Make Audio sample **************
    my($sys)="";
    my @actmp=split /-a/,$_[0];
    @actmp=split / /,$actmp[1];
    pdebug ("Params for sound Samples are: $_[0], on file  $_[1], frames=$_[2]");
    pinfo("$v2d\t Making a sound sample audio channel:\t   | $actmp[1]\n");
    $_[2] = 1000 if (! defined($_[2]));
    audioformat ("-a ".$actmp[1]);
    my $pid = fork();
    mydie "couldn't fork" unless defined $pid;
    if ($pid)
    {
        $sys = "transcode -q 0 -i \"$VOBPATH/$SAMPLE\" $_[0] -w 100,$_[2] -c 0-$_[2] ".
            "-o $_[1].avi 2> /dev/null";
        print "$sys \n" if ( $INFO);
        system ("nice -$nice $sys") == 0 
            or ( system("touch tmp/sample.finish") && mydie "Unable to run\'$sys\'");
        system("touch tmp/sample.finish");
        wait;
    } else {
        smily("sample");
    }
}

sub ask_filesize
{
    print " Enter the maximal avifile size (in MB): ";
    $FILESIZE=<STDIN>;
    chomp($FILESIZE);
    open(CONF,">>tmp/V2divx.conf");
    print CONF "#FILESIZE:$FILESIZE\n";
    close(CONF);
}


sub ask_logo
{
    my $LG=`transcode -J logo=help -c 9-11  2>&1 | grep rgbswap`;
    if ( $LG ne "" )
    {
        if ( -r $LOGO )
        {       
            print " Do you want to add the Logo $LOGO at the beginning of this movie (Y/n)? ";
            my $rep= <STDIN>;
            chomp($rep);
            if ( $rep ne "N" && $rep ne "n" )
            {
                print " How many seconds after the movie beginning must be your ".
                    "Logo displayed (MAX=$RUNTIME - see your $userconfigfile for [default:$STARTLOGO]): ";
                my $rep=<STDIN>;
                chomp($rep);
                $STARTLOGO=$rep if ( $rep ne  ""  ) ;
                while ( $ADDLOGO eq ""  || $ADDLOGO + $STARTLOGO > $RUNTIME )
                {
                    my $max=$RUNTIME-$STARTLOGO;
                    $TIMELOGO=$max if ( $max < $TIMELOGO);
                    print "How long (in sec.) should your Logo be displayed (MAX=$max - ".
                        "see your $userconfigfile for [default:$TIMELOGO])? ";
                    $rep=<STDIN>;
                    chomp($rep);
                    $ADDLOGO=$rep ;
                    $ADDLOGO=$TIMELOGO if ( $ADDLOGO eq "" || $ADDLOGO == 0 ) ;
                }
                pwarn("\t**** WARNING *****\t As your logo timing is > 300 sec., ".
                    "it will NOT be encoded in CLUSTER mode !!!\n") if ( $ADDLOGO > 300 );
                print " Where must appear your Logo (1=TopLeft,2=TopRight,3=BotLeft,".
                    "4=BotRight,5=Center, see your $userconfigfile for [default:$POSLOGO]): ";
                $rep=<STDIN>;
                chomp($rep);
                $POSLOGO=$rep if ( $rep =~ m,[12345], );
            } else {
                $ADDLOGO=0;
            }
        } else {
            pwarn("\tIf you want to add a Logo at the beginning of this movie \n\t ".
                "You must modify the \$LOGO variable, which point to your image file ".
                "(actually $LOGO), in $userconfigfile\n(Press Enter) ");
            my $junk=<STDIN>;
            $ADDLOGO=0;
        }
    } else
    {
        pwarn("Transcode is not compile with ImageMagick... Unable to encode your Logo $LOGO\n") if  ( -r $LOGO );
        $ADDLOGO=0;
    }      
    open(CONF,">>tmp/V2divx.conf");
    print CONF "#ADDLOGO:$ADDLOGO # THIS VALUE IS THE DURING TIME OF LOGO (in sec.)\n";
    print CONF "#POSLOGO:$POSLOGO\n" if ( $POSLOGO ne "" );
    print CONF "#STARTLOGO:$STARTLOGO\n" if ( $STARTLOGO ne "" );
    close(CONF);
} # END ask_logo

sub zooming
{
    #******************Evaluate the Zoom*********************
    pdebug ("---> Enter Zooming");
    # We need the Bitrate to calculate new image Size for the bpp
    calculate_bitrate;
    #   We need also the Frame rate $FPS
    my $nbr_frames=calculate_nbrframe;

    my $probe = `tcprobe -i  "$VOBPATH/$SAMPLE" 2> /dev/null `
        or mydie "Problem when running \'tcprobe -i $VOBPATH/$SAMPLE\'";

    ($probe =~ m,import frame size: -g (\d+)x,) or mydie "Unable to find Width image size";
    $Xaxis=$1;

    ( $probe =~ m,import frame size: -g \d+x(\d+).*,) or mydie "Unable to find Hight image size";
    $Yaxis=$1;

    ( $probe =~ m,aspect ratio: (\d+):(\d+).*,) or ( $probe =~ m,frame size: -g (\d+)x(\d+).*,)
        or mydie "Unable to find Image Aspect ratio";
    $ASPECT_RATIO=$1/$2;
    pdebug (" PARAMS = $PARAMS");
    $PARAMS =~ m/-j (\d+),(\d+).*/;
    $tb=$1;
    $lr=$2;
    pdebug ("Top/Bot crop= $tb, Lef/Right crop = $lr");

    # New in 1.0.2
    my $visual_Yaxis=$Xaxis/$ASPECT_RATIO;
    # recalculate the aspect ratio with clipping 
    $ASPECT_RATIO=($Xaxis-2*$lr)/($visual_Yaxis*(1-2*$tb/$Yaxis));

    if ( $Yaxis-2*$tb > 0 && $Xaxis-2*$lr > 0 )
    {
        $bpp=$bpp*$Yaxis*$Xaxis/(($Yaxis-2*$tb)*($Xaxis-2*$lr));
    } else {	
        mydie  "Something crazy !! Your image has a null or negative Size?\n".
            "Are you trying holographics movie ;-)?\n transcode is bad to do that ....";
    }

    # New Width Image = SQRT (Bitrate * aspect / QualityRatio x FPS )
    $NXaxis=sqrt(1000*$BITRATE*$ASPECT_RATIO/($bpp*$FPS));
    pdebug ("$NXaxis=sqrt(1000*(Vbitrate)$BITRATE*(A_R)$ASPECT_RATIO/((bpp)$bpp*(fps)$FPS))");
    # Finale Image MUST have a multiple of 16 size
    my @NXaxis="";
    my @NYaxis="";
    $NXaxis[1]=16*floor($NXaxis/16);
    $NXaxis[2]=16*ceil($NXaxis/16);
    # Limits 	
    my ($i,$j)=0;
    for ( $i=1 ; $i<3 ; $i++ )
    {
        $NXaxis[$i]= 16*floor(($Xaxis-2*$lr)/16) if ( $NXaxis[$i] > $Xaxis-2*$lr );
        $NXaxis[$i]= 720 if ( $NXaxis > 720 );
        $NXaxis[$i]= 320 if ( $NXaxis < 320);
    }

    #                       New Height
    # Finale Image MUST have a multiple of 16 size
    $NYaxis[1]=16*floor(($NXaxis[1]/$ASPECT_RATIO)/16);
    $NYaxis[2]=16*ceil(($NXaxis[1]/$ASPECT_RATIO)/16);
    $NYaxis[3]=16*floor(($NXaxis[2]/$ASPECT_RATIO)/16);
    $NYaxis[4]=16*ceil(($NXaxis[2]/$ASPECT_RATIO)/16);

    # If we can find similar AR with better BPP ... get it !
    # Avec 1 poids de 110, si AR varie de 1% , BPP ne doit pas varier de plus de 0.009 bpp (1/110)
    my $weight=110;
    # Quality = BPP*weight-%ASPECT_RATIO_error 
    my $Quality=$weight*1000*$BITRATE/($NXaxis[1]*$NYaxis[1]*$FPS)
        -abs($NXaxis[1]/$NYaxis[1]-$ASPECT_RATIO)*100/$ASPECT_RATIO;
    for ( $i=1; $i < 5 ; $i++ )
    {	
        for ( $j=1; $j<3; $j++ )
        {
            pdebug ("---------------------");
            pdebug (" NXaxis[$j] = $NXaxis[$j] , NYaxis[$i] = $NYaxis[$i], asp-rat=$ASPECT_RATIO");
            printf(STDOUT " bpp=%.3f \n",1000*$BITRATE/($NXaxis[$j]*$NYaxis[$i]*$FPS)) if ( $DEBUG );
            printf(STDOUT " and AR_err = %.3f ",abs(($NXaxis[$j]/$NYaxis[$i])-$ASPECT_RATIO)*100/$ASPECT_RATIO) if ( $DEBUG );
            pdebug ("%");
            my $tmp=$weight*1000*$BITRATE/($NXaxis[$j]*$NYaxis[$i]*$FPS)-abs($NXaxis[$j]/$NYaxis[$i]-$ASPECT_RATIO)*100/$ASPECT_RATIO;
            printf(STDOUT "Quality = %.6f\n",$tmp) if ( $DEBUG );
            if ( $tmp >= $Quality)
            { 	
                $Quality=$tmp;
                $NXaxis=$NXaxis[$j];
                $NYaxis=$NYaxis[$i];
                pdebug ("X($j)=$NXaxis, Y($i)=$NYaxis <--CATCH !");
            }
        }
    }
    # Limits but normally impossible to fall into
    if ( $NXaxis > $Xaxis )
    { 
        $NXaxis=16*ceil(($Xaxis-2*$lr)/16);
        $NYaxis=16*ceil(($NXaxis/$ASPECT_RATIO)/16);
    }
    if ( $NYaxis > $Yaxis )
    {
        $NYaxis=16*ceil(($Yaxis-2*$tb)/16);
        $NXaxis=16*ceil(($NYaxis*$ASPECT_RATIO)/16);
    }

    if ( 
        ($Xaxis - 2*$lr)/16 == floor(($Xaxis - 2*$lr)/16) 
        && ($Yaxis - 2*$tb)/16 == floor (($Yaxis - 2*$tb)/16) 
    )
    {
        pinfo("$v2d\t Slow Zooming is necessary:\t\t   | NO\n") if ( ! -e "tmp/cluster.args");
        $Zoom_mode="B";
    } else {
        pinfo("$v2d\t Slow Zooming is necessary:\t\t   |$RED YES\n") if ( ! -e "tmp/cluster.args");
        $Zoom_mode="Z";
    }
    sleep(1);
    pdebug ("<--- Zooming");
} # end zooming

sub config
{       
    my $rep="";
    # ********************** Config ****************************
    pdebug ("--->  Enter config");
    mydie "There is still a tmp/V2divx.conf , please remove all tmp files".
        "\n (or at least tmp/V2divx.conf) before running V2divx /path/to/vob sample" 
        if ( -f "tmp/V2divx.conf") ;
    $VOBPATH= $ARGV[0];
    mydie "Directory \"$VOBPATH\" does not exist \n Sorry" if ( ! -e $VOBPATH);
    umask(000); #reset current umask values. Luis Mondesi
    mkdir ("tmp",01777);
    chk_wdir;

    open(CONF,">>tmp/V2divx.conf");
    print CONF "#VOBPATH:$VOBPATH# DO NOT MODIFY THIS LINE\n";
    print CONF "#DIVX:$DIVX# \n";
    print CONF "#DIVX_OPT:$DIVX_OPT# \n";
    close(CONF);

    print "\n You will have a look with \'$XINE\' on the File $LASTVOB.\n Look how ".
        "long (in seconds) is the end credits (so we can remove it),\n you also may ".
        "find which audio stream and subtitle number you will choose.\n";
    print " Press Enter -> ";
    my $junk=<STDIN>;
    system ("$XINE $VOBPATH/$LASTVOB >/dev/null 2>&1");
    # How many second remove from end of movie...
    print " How long (in seconds) are the end credits (we will not process it and so ".
        "increase video bitrate) [default:0]? "; 
    $END_SEC=<STDIN>;
    chomp($END_SEC);

    $END_SEC=0 if ( $END_SEC eq "" || $END_SEC < 10 );
    print " How many seconds will you remove from the beginning [default:0]? ";
    $START_SEC=<STDIN>;
    chomp($START_SEC);
    $START_SEC=0 if ( $START_SEC eq "" );

    pwarn("\t**** WARNING ****\nIn cluster mode we split the movie after it's completly encoded\n")
        and sleep(5) if ( $END_SEC > 600 or $START_SEC > 600 ); 

    open(CONF,">>tmp/V2divx.conf");
    print CONF "#END_SEC:$END_SEC\n";
    close CONF;

    #*************SOUND SAMPLE**********************
    my $as=20;
    my ($number_of_ac,$audio_channel,@achannels)=get_audio_channel("all");
    if (  $NOAUDIO != 1 )
    {
        print " Do you want to make Sound samples to find which audio channel is the one you want (y|N)? ";
        $rep=<STDIN>;
        chomp($rep);
        my $pcm_swb=""; # PCM swap bytes
        my($chkpcm)=0;
        if ( $rep eq "o" ||   $rep eq "O" ||  $rep eq "y" ||  $rep eq "Y" )
        {
            my $i=0;
            for ($i = 0; $i <= $number_of_ac; $i ++)
            {
                make_sample(" -y $DIVX $DIVX_OPT $VID_OPT -a $i $pcm_swb ",
                    "audio_sample._-a_${i}_", $audiosample_length);
                print "\n To hear this audio sample, please press Enter ->";
                $junk=<STDIN>;
                system("$AVIPLAY audio_sample._-a_${i}_.avi > /dev/null 2>&1 ")==0 
                    or mydie "Problem to run \'$AVIPLAY audio_sample._-a_${i}_.avi\'";
                my($audio_format)=audioformat("-a $i");
                if ($audio_format eq "pcm" && $chkpcm eq 0 )
                {
                    pinfo("$v2d\t Audio channel $i format:\t\t   | $audio_format\n");
                    print " Was the sound completly noisy (y|N)? ";
                    $rep= <STDIN>;
                    chomp($rep);
                    if ( $rep eq "O" or $rep eq "o" or $rep eq "y" or $rep eq "Y")
                    {
                        pinfo("$v2d\t Remake this sample with option:\t   | -d\n");
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
                    $as=$i;
                    last;
                } elsif ($i == $number_of_ac )
                {
                    pwarn("\tNo more Audio channels !\n");
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
            pwarn("$as : is not an available audio channel.\n") if  ( ! grep (/$as/,@achannels) );
        }
        $audio_channel=$as;
        my($audiofmt_ch1)=audioformat("-a $as");
        pinfo("$v2d\t Audio channel $as format:\t\t   | $audiofmt_ch1\n");
        if ($audiofmt_ch1 eq 'pcm' && $chkpcm eq 0 )
        {
            pwarn("$v2d\t As this audio channel is PCM format, it may be completly noisy\n");	
            make_sample(" -y $DIVX $DIVX_OPT $VID_OPT -a $as",
                "audio_sample._-a_${as}_", $audiosample_length);
            print " Ear this audio sample, please press Enter ->";
            $junk=<STDIN>;
            system("$AVIPLAY audio_sample._-a_${as}_.avi > /dev/null 2>&1 ") ;
            unlink("audio_sample._-a_${as}_.avi");
            print " Was this sample completly noisy (y|N)? ";
            $rep=<STDIN>;
            chomp($rep);
            if ( $rep eq "O" or $rep eq "o" or $rep eq "y" or $rep eq "Y")
            {
                $pcm_swb="-d";
            }
        }
        $PARAMS="-a $audio_channel $pcm_swb";


        if ( $number_of_ac > 0 )
        {
            print " Do you want to have another audio channel in your AVI movie (take care ".
                "of the Video quality which decrease with 2 audio channels for the same ".
                "movie size), this audio channel will be encoded at the same bitrate ".
                "than the first audio channel (y|N)? ";
            $rep=<STDIN>;
            chomp($rep);
            if ( $rep eq "y" || $rep eq "O" ||  $rep eq "o" ||  $rep eq "Y" )
            {
                $AC2=100;
                while($AC2>$number_of_ac || $AC2 == $audio_channel )
                {	
                    print " Enter the other audio channel number you want(MAX=$number_of_ac): ";
                    $AC2=<STDIN>;
                }
                chomp($AC2);
                my $audiofmt_ch2=audioformat("-a $AC2");
                pinfo("$v2d\t Audio channel $AC2 format:\t\t   | $audiofmt_ch2\n");
                open (CONF,">>tmp/V2divx.conf");
                print CONF "#AC2:$AC2 $pcm_swb\n";
                close(CONF);
            }
        }
        a_bitrate;
    }

    #****************CROPPING TOP/BOTTOM ***********************
    findclip;
    my($top_bot)=0;
    print " Clipping Top/Bottom \n You must have the smallest black LetterBox at top/bottom \n".
        " (It's better to leave black LetterBox at top/bottom if you intend to have SubTitle)\n";
    print " To see the first sample, please press Enter -->";
    $rep=<STDIN>;
    system("/bin/rm video_s._-j_*.ppm 2> /dev/null ");
    my $inc=8;
    my ($in_video_magic,$in_video_codec)=videoformat("$VOBPATH/$SAMPLE");
    while ( $rep ne "O" &&  $rep ne "o" && $rep ne "y" && $rep ne "Y" )
    {
        my $sys="transcode -q 0 -M 1 $REV_VID -x $in_video_codec,null -i \"$VOBPATH/$SAMPLE\" -j $tb,$lr -y ppm,null -c 10-11 -o video_s._-j_$tb,${lr}_";
        print "$sys \n" if ( $INFO);
        system ($sys."  > /dev/null");
        my $tmp = `/bin/ls -1 video_s._-j_$tb,${lr}_*.ppm`;
        my @aclip = split /\n/, $tmp;
        my $file="";
        foreach $file ( @aclip  ) 
        {
            system ("$XV $file");
        }
        print " (Don't care about colors or upside down please)".
            "\n Are Top/Bottom LetterBoxes (-j $RED$tb$NORM,$lr) OK ?(y), to big (b) or to small (s): ";
        $rep= <STDIN>;
        chomp($rep);
        if ($rep eq "O" or $rep eq "o" or $rep eq "y" or $rep eq "Y")
        {	
            system("rm video_s._-j_$tb,${lr}_*.ppm" );
            $top_bot=1;
            last;
        } elsif ( $rep eq "S" || $rep eq "s"  ) {
            system("rm video_s._-j_$tb,${lr}_*.ppm");
            $top_bot=0;
            $tb=$tb-$inc;
            if ( $tb < 0 ) 
            {
                $tb = 0;
            }
        } elsif  ( $rep eq "B" || $rep eq "b"  ) {
            system("rm video_s._-j_$tb,${lr}_*.ppm" );
            $top_bot=0;
            $tb=$tb+$inc;
        }
    }


    # ***************** CROPPING LEFT RIGHT **************************
    my ($left_right)=0;
    print " Now Clipping Left/Right \n";
    print " To see the first sample, please press Enter -->";
    $rep=<STDIN>;
    while ( $rep ne "O" &&  $rep ne "o" && $rep ne "y" && $rep ne "Y" )
    {
        my $sys="transcode -q 0 -M 1 $REV_VID -x $in_video_codec,null -i ".
            "\"$VOBPATH/$SAMPLE\" -j $tb,$lr -y ppm,null -c 10-11 -o video_s._-j_$tb,${lr}_";
        print "$sys \n" if ( $INFO);
        system ("$sys > /dev/null");
        my $tmp = `/bin/ls -1 video_s._-j_$tb,${lr}_*.ppm`;
        my @aclip = split /\n/, $tmp;
        my $file="";
        foreach $file ( @aclip  )
        { 
            system ("$XV $file")
        }
        print " (Don't care about colors or upside down please)\n Are Left/Right LetterBoxes ".
            "(-j $tb,$RED$lr$NORM) OK ?(y), to big (b) or to small (s): ";
        $rep= <STDIN>;
        chomp($rep);
        if ( $rep eq "O" || $rep eq "o" or $rep eq "y" or $rep eq "Y" )
        {	
            system("rm video_s._-j_$tb,${lr}_*.ppm");
            $left_right=1;
            last;
        } elsif ( $rep eq "B" || $rep eq "b"  ) {
            system("rm video_s._-j_$tb,${lr}_*.ppm");
            $left_right=0;
            $lr=$lr+$inc;
        } elsif  ( $rep eq "S" || $rep eq "s"  ) {
            system("rm video_s._-j_$tb,${lr}_*.ppm");
            $left_right=0;
            $lr=$lr-$inc;
            if ( $lr < 0 ) 
            {
                $lr = 0 ;
            }
        }
    }
    $PARAMS = $PARAMS." -j $tb,$lr";

    #************************* SUBTITLE ***********************
    my $st=20;
    my $SUBT="";
    if ( $in_video_magic eq 'mov' )
    {
        $SUBT=`tcprobe -i $VOBPATH/$SAMPLE -H 15 2> /dev/null` 
            or mydie "Problem when running \'tcprobe -i $VOBPATH/$SAMPLE -H 15 \'";
    } else {
        $SUBT=`tcprobe -i $VOBPATH -H 15 2> /dev/null` 
            or mydie "Problem when running \'tcprobe -i $VOBPATH \'";
    }

    my $number_of_st=`cat tmp/probe.rip | grep subtitle | wc -l ` if ( -f "tmp/probe.rip" );
    if ( ($SUBT =~ m,detected \((\d+)\) subtitle,) 
        || ( defined($number_of_st) && $number_of_st != 0))
    {      
        $number_of_st=$1 if ( ! defined($number_of_st));
        pinfo("$v2d\t Number of subtitles detected:\t\t   | $number_of_st\n");
        $number_of_st=$number_of_st-1;
        if ( -f "tmp/probe.rip")
        {
            open(PROBE,"<tmp/probe.rip");
            while(<PROBE>)
            { 
                print $GREEN."$v2d\t Subtitle $1 language:\t\t\t   | $2\n".$NORM 
                    if ( $_=~ m, subtitle (\d+)=(.*),)	;
            }
            close(PROBE);
        }
        print " Do you want subtitle (y|N)? ";
        $rep= <STDIN>;
        chomp($rep);
        if ($rep eq "O" or $rep eq "o" or $rep eq "y" or $rep eq "Y")
        {
            while ( $st > $number_of_st )
            {
                print " SubTitle number (MAX=$number_of_st)[default:0]? ";
                $st=<STDIN>;
                chomp($st);
                $st = 0 if ( $st eq "" );
            }
            pinfo("$v2d\t Detected subrip Transcode contrib:\t   | ");
            if ( $SUBRIP && $in_video_magic eq 'vob' )
            {
                print "yes\n".$NORM;
                print " Do you want subtitles to be in the movie Encoded or in a separate File (E|f)? ";
                $rep= <STDIN>;
                chomp($rep);
            } else {	
                print $RED."No\n".$NORM;
            }
            if ($rep eq "F" or $rep eq "f")
            { 
                if ( $START_SEC)
                {   
                    pwarn($v2d."\t Due to synchro we MUST keep begin credits:\n");
                    $START_SEC=0; 
                }
                my $stlang="en";
                if ( -f "tmp/probe.rip")
                {
                    open(PROBE,"<tmp/probe.rip");
                    while(<PROBE>)
                    {
                        $stlang=$1 if ( $_=~ m, subtitle 0${st}=<(.*)>,);
                    }
                    close(PROBE);
                } else {
                    print " What is the language of this subtitle ?\n\t1) en\n\t2) fr\n".
                        "\t3) de\n\t4) es\n";
                    $rep=<STDIN>;
                    SWITCH:
                    {
                        if ( $rep == 2 ) { $stlang = "fr" ; last SWITCH ;}
                        if ( $rep == 3 ) { $stlang = "de" ; last SWITCH ;}
                        if ( $rep == 4 ) { $stlang = "es" ; last SWITCH ;}
                    }
                }
                $SUB_TITLE="SRT_${st}_${stlang}";
            } else {
                $SUB_TITLE="extsub=$st:$tb:0:1:$EXTSUB,";
            }
            open (CONF,">>tmp/V2divx.conf");
            print CONF "#SUB_TITLE:$SUB_TITLE\n";
            close (CONF);
        }
    }
    open (CONF,">>tmp/V2divx.conf");
    print CONF "#START_SEC:$START_SEC\n";
    close (CONF);


    zooming;

    #************** ANTIALIASING & DEINTERLACING ******************** 
    my $is_interlaced=interlaced;
    print " Do you want to deinterlace this movie(";
    print "Y|n)? " if ( $is_interlaced eq "yes" );
    print "y|N)? " if ( $is_interlaced eq "no") ;
    $rep= <STDIN>;
    chomp($rep);
    

    if ( $rep eq "O" 
        || $rep eq "o" 
        || $rep eq "y" 
        || $rep eq "Y" 
        || ($is_interlaced eq "yes" && $rep ne 'N' && $rep ne 'n') ) 
    {	
        open (CONF,">>tmp/V2divx.conf");
        if ( $TELECINE )
        {
            $DEINTL="ivtc,32detect=force_mode=3,decimate,";
            print CONF "#DEINTL:$DEINTL\n";
        } else {
            my $PP=`transcode -J pp=lb -c 9-11  2>&1 | grep failed`;
            pinfo("$v2d\t Mplayer postproc. enable:\t\t   | ");
            print "YES\n".$NORM if ( $PP eq "" ) ;
            print "NO\n".$NORM if ( $PP ne "" ) ;
            print " To deinterlace, do you want to use:\n";
            print "\tA) The -I 3 transcode option (a|A)\n\tB) The YUVdenoiser (b|B)";
            print "\n\tC) the smart deinter filter (c|C)" ;
            print "\n\tD) the Mplayer pp filter (d|D)" if ( $PP eq "" ) ;
            print " [default:A]?:";
            $rep= <STDIN>;
            chomp($rep);
            if ( ($rep eq "D" || $rep eq "d") && $PP eq "" )
            {
                $DEINTL="pp=lb,";
                print CONF "#DEINTL:$DEINTL\n";
            } elsif  ( $rep eq "B" || $rep eq "b" )
            {
                $DEINTL="yuvdenoise=sharpen=100:deinterlace=1,";
                print CONF "#DEINTL:$DEINTL\n";
            } elsif  ( $rep eq "C" || $rep eq "c" )
            {
                $DEINTL="smartdeinter=diffmode=2:highq=1:cubic=1,";
                print CONF "#DEINTL:$DEINTL\n";
            } else {
                $DEINTL="32detect=force_mode=3,";
                print CONF "#DEINTL:$DEINTL\n";
            }
            close (CONF);
        } 

    }
    print " Does your clip need Antialiasing (slower) (y|N)? ";
    $rep= <STDIN>;
    chomp($rep);
    my $aalias=""; 
    $aalias=" -C 3" if ( $rep eq "O" || $rep eq "o" || $rep eq "y" || $rep eq "Y" );
    #       Write parameters
    ( $left_right && $top_bot ) or mydie "Oups Sorry.. I miss some parameters :-(";
    $PARAMS = $PARAMS.$aalias;
    open (CONF,">>tmp/V2divx.conf");
    print CONF "#PARAMS:$PARAMS # YOU MUST KNOW WHAT YOU DO IN THIS LINE\n";
    close(CONF);

    #    Ask for a Logo
    ask_logo;
    #     Search DVD Title
    if ( $TITLE  eq  "" )
    {
        print " Enter the title of this movie (blank space available): ";
        $TITLE=<STDIN>;
        $TITLE =~ s/ /_/g;
        chomp($TITLE);
        if ( $TITLE eq "" ) 
        {
            $TITLE="movie";
        }
    }
    open (CONF,">>tmp/V2divx.conf");
    print CONF "#TITLE:$TITLE # DO NOT MODIFY THIS LINE\n";
    print CONF "#TELECINE:$TELECINE# DO NOT MODIFY THIS LINE\n";
    close(CONF);

}   # END Config 

sub ripdvd
{ 	
    # *************** RIP A DVD ***********************
    my ($angle,$angles,$chapter,$chapters)=1;
    my (@titleSet,@titleLen,$sys,$vob,$sec,$hour,$min,$dvd)="";
    $VOBPATH= $ARGV[0];
    ( -e $VOBPATH ) or mydie "Directory \"$VOBPATH\" does not exist \n Sorry";
    print " On which device is your DVD [default: /dev/dvd]? " ;
    $dvd=<STDIN>;
    chomp($dvd);
    $dvd="/dev/dvd" if ( $dvd eq "" ); 
    if ( $DVDTITLE ne "" )
    { 
        $TITLE=`$DVDTITLE $dvd 2> /dev/null` or die "Problem when running \'dvdtitle $dvd\'";
        chomp($TITLE);
    } else {  	
        print " V2divx does'nt find dvdtitle, please enter this DVD Movie Title: ";
        $TITLE=<STDIN>;
        $TITLE =~ s/ /_/g;
        chomp($TITLE);
        $TITLE ="VT" if ( $TITLE eq "" );
    }
    print $RED;
    print "******* WARNING *********\n";
    print "All files in $VOBPATH will be deleted !!!\n";
    print "Press Enter to continue or <Ctrl-C> to Abort\n";
    print $NORM;
    my $junk=<STDIN>;
    my $probe = `tcprobe -i \"$dvd\" 2>&1` or die "Problem when running \'tcprobe -i $dvd\'";
    ($probe =~ m,DVD title \d+/(\d+),) or die "Probing DVD failed! - No DVD?";
    my $totalTitles = $1;
    print " titles: total=$totalTitles\n";

    my @checkTitles = 1 .. $totalTitles;
    # now probe each title and find longest
    my $longestLen   = 0;
    my $longestTitle = 0;
    for(@checkTitles) 
    {
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
        printf("%02d: len=%02d:%02d:%02d titleset=%02d angles=%02d ".
            "chapters=%02d\n", $_,$hour,$min,$sec,$titleSet[$_],$angles,$chapters);

        #  find largest title
        if($titleLen[$_] > $longestLen) 
        {
            $longestLen   = $titleLen[$_];
            $longestTitle = $_;
        }
    }
    print " The Main Title seems to be the Title No : $longestTitle, OK ? (y/n) :";
    my $rep=<STDIN>;
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
        pwarn("***************** WARNING!!!! *********************\n".
            "\t This is a multi angles video stream. \n");
        print " Do you know which angle number you want to rip (y|N)? ";
        $rep=<STDIN>;
        chomp($rep);
        die "OK ... Have a look on your DVD to find which angle you like\n Bye" 
            if ( $rep ne "o" &&  $rep ne "O" && $rep ne "y" && $rep ne "Y" );
        print " OK ... we continue ...\n";
        print " There is $angles which one do you want? ";
        print $NORM;
        $angle=<STDIN>;
        chomp($angle);
    }
    #  Check if this title is multichapter
    ($probe =~ m,(\d+) chapter\(s\),) or die "No chapter found in tcprobe for title $longestTitle !";
    $chapter=$1;
    if ( $chapter > 1 )
    { 	
        print " Do you want to rip this title chapter by chapter (y|N)? ";
        $rep=<STDIN>;
        chomp($rep);
    }

    system("/bin/rm -rf $VOBPATH/*  2> /dev/null");
    open (H_TITLE,">$VOBPATH/dvdtitle");
    print H_TITLE $TITLE;
    close(H_TITLE);
    opendir(VOB,$VOBPATH);
    chdir($VOBPATH) 
        or die "Unable to chdir to $VOBPATH.. please DO NOT USE the ~ character in the /path/to/vob";
    $sys="tcprobe -i $dvd -T $longestTitle >> probe.rip 2>&1 ";
    system ("nice -$nice $sys");
    if ( $rep eq "y" || $rep eq "Y" || $rep eq "o" || $rep eq "0")
    {
        my $i=0;
        for ( $i=1;$i<=$chapter;$i++)
        {
            $sys="tccat -i $dvd -T $longestTitle,$i,$angle ".
                "| split -b 1024m - ${TITLE}_T${longestTitle}_C${i}_" ;
            print "$sys \n" if ( $INFO);
            system("nice -$nice $sys");
        }
    } else {
        $sys="tccat -i $dvd -T $longestTitle,-1,$angle ".
            "| split -b 1024m - ${TITLE}_T${longestTitle}_" ;
        print "$sys \n" if ( $INFO);
        system("nice -$nice $sys");
    }
    # Check if $TITLE is well in the vob file name AND 
    # the vob file is well in the current directory 
    my(@files)=grep {
        /$TITLE/ && -f "$_"
    } readdir(VOB);
    closedir(VOB);
    my $i=0;
    foreach $vob (@files) 
    {
        rename($vob,$vob.".vob");
        $i++;
    }
    open (PROBE,">>probe.rip");
    print PROBE "Number of vob files:$i" ;
    close(PROBE);
    pinfo("$v2d\t Video files are in:\t\t   | $VOBPATH\n");
    print " You may now run V2divx with yours arguments to encode the vob file(s)\n\n";
    exit(0);
} # END ripdvd

sub pinfo
{
    # takes a string a prints a green message:
   print STDOUT $GREEN."@_".$NORM ;
}

sub myperror
{
    # takes a string and prints a red error message
    print $RED."@_".$NORM  if ( $DEBUG );
}

sub pdebug 
{   # prints a string only if $DEBUG is true
    print " + @_ \n" if ( $DEBUG );
}

sub pwarn
{
    my $red="\033[1;31m";
    my $norm="\033[0;39m";
    # takes a string and prints a red warn message
    print STDOUT $red."@_".$norm;
}

# ************ End of Functions Declarations *********** #
#EOF#