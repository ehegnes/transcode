#! /usr/bin/perl

use POSIX;

if ($#ARGV < 0)
{
  print("\n\n");
  print("Usage:\n");
  print("    %> tc_rescale_audio film1.avi film2.avi ...\n");
  print(" or %> tc_rescale_audio myAviDir/*.avi \n");
  print("Each avi file is analyed and reencoded if necessary.\n");
  print("\n\n");
}

while ($#ARGV >= 0) 
{
  $FilmName = shift(@ARGV);
  print ("Analyzing File ".$FilmName."\n");

  $tmp = `tcextract -x mp3 -i $FilmName | tcdecode -x mp3 | tcscan -x pcm -b 96 -c 695 | grep "volume rescale"`;
  @tmp = split /rescale=/, $tmp;
  if ($tmp[1] > 1.2)
    {   chomp ($tmp[1]);
        $audio_rescale = $tmp[1];
	# do audio rescaling:
	$sys = "transcode -s ".$audio_rescale." -i ".$FilmName." -P 1 -y raw -o new-".${FilmName};
	system ("nice -10 ".$sys);
    } else
    {       print("Audio rescaling for ".$FilmName." not necessary.\n");
    }
}

