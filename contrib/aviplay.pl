#! /usr/bin/perl -w

use POSIX;

#print $ARGV[0]."\n";
#$usage = "1. Parameter: Film, 2. Parameter: Vergrößerung (optional)\n";

$mpeg2divx = "mpeg2divx -f -b 6000 ";
#$cdrom = "/dvd"

#print $2;
$umount = 0;
if ($ARGV[0])
{	$datei =  $ARGV[0];
} else
{	system("mount /cdrom");
	$datei = `find /cdrom -name "*.avi" -print | head -1`; # | tr -d \\\n;
	chop($datei);
	$ARGV[0] = $datei;
	$umount = 1;
	#print $usage;
	#exit;
}

$ARGV[0] =~ tr/./-/;
@teile = split /-/, $ARGV[0];

print("Film: ".$datei."\n");

$gef = 0;
$f_x = 0;
$f_y = 0;
$letztes = "";

foreach $teil (@teile)
{	#print $teil."\n";
	@tmp = split /x/, $teil;
	if ($gef == 0 && defined($tmp[1]) && ! defined($tmp[2]) &&  $tmp[0] > 1 && $tmp[1] > 1)
	{	#print("======>>>>".$tmp
        	$f_x = $tmp[0];
		$f_y = $tmp[1];
                $gef = 1;
	}
	$letztes = $teil;
}

if ($letztes eq "mpg" || $letztes eq "mpeg" || $letztes eq "MPG" || $letztes eq "MPEG")
{	print ($datei." ist ein MPEG: Wird zuerst in avi konvertiert!!\n");
	$sys = $mpeg2divx."\"".$datei."\" \"DivX.".$datei.".avi\"";
	print ("Kommando: ".$sys."\n\n");
	system($sys);
	#system("mv \"$datei\" \"tmp.$datei.tmp\"");
	$datei = "DivX.".$datei.".avi";
}


if ($gef == 0)
{	#$ARGV[1] = 1;
	$sys = "avidump -i \"$datei\" 2> /tmp/aviplay.pl.tmp";
	system($sys);
	$tmp = `cat /tmp/aviplay.pl.tmp | grep video`;
	$tmp =~ tr/,/ /;
	@teile = split / /, $tmp;
	foreach $teil (@teile)
	{	@sub = split /=/, $teil;
		if (defined($sub[1])&&! defined($sub[2]))
		{	if ($sub[0] eq "width")
			{	#print ("\n".$sub[1]."\n");
				$f_x = $sub[1];
			}
			if ($sub[0] eq "height")
			{	$f_y = $sub[1];
			}
		}
	}

#	$tmp = `avitype \"$datei\" | grep Frames`;
#	@teile = split /of/, $tmp;
#	chomp($teile[1]);
#	@sub = split /x/, $teile[1];
#	$f_x = $sub[0];
#	$f_y = $sub[1];
}

if ($f_x > 0 && $f_y > 0)
{	print "Film:".$f_x."x".$f_y."\n";
} else
{	$ARGV[1] = 1;
}

$dim = `xdpyinfo | grep dimensions`;
@teile = split / /, $dim;
$i = 0;
$gef = 0;
foreach $teil (@teile)
{	#print $i.":".$teil."\n";
	$i++;
	@tmp = split /x/, $teil;
	if ($gef == 0 && defined($tmp[1]) && ! defined($tmp[2]))
	{	$dim_x = $tmp[0];
		$dim_y = $tmp[1];
		print "Bildschirm: ".$dim_x."x".$dim_y."\n";
		$gef = 1;
	}
}

if ($ARGV[1])
{	if ($ARGV[1] eq "h")
	{	print "Horizontal <--->\n";
		$faktor = $dim_x / $f_x;
		$f_x = $dim_x;
		$f_y = $f_y * $faktor;
	} elsif ($ARGV[1] eq "v")
	{	print ("Vertikal\n");
		$faktor = $dim_y / $f_y;
		$f_x = $f_x * $faktor;
		$f_y = $dim_y;

	} else
	{	@tmp = split /x/, $ARGV[1];
		if (defined($tmp[1]) &&! defined($tmp[2]))
		{	$f_x *= $tmp[0];
			$f_y *= $tmp[1];
		} else
		{	$f_x *= $ARGV[1];
			$f_y *= $ARGV[1];
		}
		$faktor = $ARGV[1]
	}
} else
{

	if ($dim_x > $f_x && $dim_y > $f_y)
	{	$fak_x = floor($dim_x / $f_x);
		$fak_y = floor($dim_y / $f_y);
		if ($fak_x > $fak_y)
		{	$faktor = $fak_y;
		} else
		{	$faktor = $fak_x;
	}
	} else
	{	$faktor = 1;
	}
	$f_x *= $faktor;
	$f_y *= $faktor;

}
#$f_x *= $faktor;
#$f_y *= $faktor;
print ("Vergrösserung: ".$faktor." -> ".$f_x."x".$f_y."\n\n");

if ($faktor)
{	$sys = "aviplay  -size ".$f_x." ".$f_y." \"".$datei."\"";
} else
{	$sys = "aviplay \"".$datei."\"";
}
#print($sys);
print($sys."\n");
system($sys);
if ($umount == 1)
{	system ("umount /cdrom");
}
