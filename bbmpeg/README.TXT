 *
 *  Thanks to Brent Beyeler (beyeler.@home.com) for the bbmpeg stuff
 *  on windows, which i have ported to linux and modified to use it
 *  as libbbmpeg.a !
 * 

LINUX-port of bbmpeg1.24-beta18 
-------------------------------
It is splittet in a separate multiplexer tool (bbmplex), 
a separate MPEG1/2 video stream encoder tool (bbencode) and
the "libbbmpeg.a" library to help building your own mpeg1/2-
encoder ! 

the "bbencode" won't compile by default, because it needs a 
special modified version of "libmpeg3.a" and some other stuff 
out of my "Linux Video Editor (lve)"-project (not included here).
But mpeg1/2 encoding is available with "transcode", which 
has it's own export-modul to handle mpeg1/2-encoding based on 
"libbbmpeg.a" !

the standalone video-encoder is capable of:
- reading MPEG-Files directly (incl. VOB)
- reading "Edit-List"-Files produced with
  Linux Video Editor (LVE).
  (in this mode audio portion will be extracted in a
   pseudo-WAV file, which should be encoded  by ../audiomp2/toolame)

Audioencoding is not ported, because of its slow speed
-> please use "toolame" in ../audiomp2 instead.

please call "bbmplex" or "bbencode" without parameters
to get a short usage-description.