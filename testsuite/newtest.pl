#!/usr/bin/perl -w
#
# Simple test suite for transcode
# Written by Andrew Church <achurch@achurch.org>
#
# This file is part of transcode, a video stream processing tool.
# transcode is free software, distributable under the terms of the GNU
# General Public License (version 2 or later).  See the file COPYING
# for details.

use constant CSP_YUV => 1;
use constant CSP_RGB => 2;
use constant WIDTH => 704;
use constant HEIGHT => 576;
use constant NFRAMES => 3;

my $IN_AVI = "in.avi";
my $OUT_AVI = "out.avi";
my $STDOUT_LOG = "stdout.log";
my $STDERR_LOG = "stderr.log";
my $TMPDIR = undef;

my @Tests = ();       # array of test names in order to run
my %Tests = ();       # data for tests (key is test name)

my $Verbose = 0;      # let transcode write to stdout/stderr instead of logs
my $KeepTemp = 0;     # keep temporary directory/files around?
my %TestsToRun = ();  # user-specified list of tests to run
my %VideoData;        # for saving raw output data to compare against

########
# Initialization
&init();

########
# First make sure that -x raw -y raw works, and get an AVI in transcode's
# output format for comparison purposes
&add_test("raw/raw (YUV)", [],
          \&test_raw_raw, CSP_YUV);
&add_test("raw/raw (RGB)", [],
          \&test_raw_raw, CSP_RGB);
# Alias for both of the above
&add_test("raw", ["raw/raw (YUV)", "raw/raw (RGB)"]);

# Test colorspace conversion
&add_test("raw/raw (YUV->RGB)", ["raw"],
          \&test_raw_raw_csp, CSP_YUV, CSP_RGB);
&add_test("raw/raw (RGB->YUV)", ["raw"],
          \&test_raw_raw_csp, CSP_RGB, CSP_YUV);
&add_test("raw-csp", ["raw", "raw/raw (YUV->RGB)", "raw/raw (RGB->YUV)"]);

# Test import_ffmpeg on test video
&add_test("-x ffmpeg (raw)", ["raw"],
          \&test_import, "ffmpeg", \$VideoData{&CSP_YUV}, CSP_YUV);

# Test various export modules by running their output through ffmpeg
&add_test("-y af6", ["-x ffmpeg (raw)"],
          \&test_export_x_ffmpeg, "af6", CSP_YUV, "Uncompressed I420");
&add_test("-y ffmpeg", ["-x ffmpeg (raw)"],
          \&test_export_x_ffmpeg, "ffmpeg", CSP_YUV, "mpeg4");
&add_test("-y mjpeg", ["-x ffmpeg (raw)"],
          \&test_export_x_ffmpeg, "mjpeg", CSP_YUV);
&add_test("-y xvid4", ["-x ffmpeg (raw)"],
          \&test_export_x_ffmpeg, "xvid4", CSP_YUV);

########
# Run all (or specified) tests
foreach $test (@Tests) {
    &run_test($test) if !%TestsToRun || $TestsToRun{$test};
}

########
# Finished, clean up
&cleanup();

###########################################################################
###########################################################################

# Initialization

sub init
{
    for (my $i = 0; $i < @ARGV; $i++) {
        if ($ARGV[$i] =~ /^(--(.*)|-(.)(.*))/) {
            my $option = $2 ? "--$2" : "-$3";
            my $optval = $4;
            if ($option eq "-h" || $option eq "--help") {
                print STDERR "Usage: $0 [-kv] [-t test [-t test...]]\n";
                print STDERR "     -k: don't delete temporary directory\n";
		print STDERR "     -v: verbose (transcode output to stdout/stderr\n";
                print STDERR "     -t: specify test(s) to run\n";
                exit 0;
            } elsif ($option eq "-k") {
                $KeepTemp = 1;
            } elsif ($option eq "-t") {
                $optval = $ARGV[++$i] if $optval eq "";
                $TestsToRun{$optval} = 1;
	    } elsif ($option eq "-v") {
		$Verbose = 1;
            } else {
                &fatal("Invalid option `$option' ($0 -h for help)");
            }
        }
    }

    $TMPDIR = $ENV{'TMPDIR'} || "/tmp";
    $TMPDIR .= "/tctest.$$";
    mkdir $TMPDIR, 0700 or &fatal("mkdir($TMPDIR): $!");
    print STDERR "Using temporary directory $TMPDIR\n";
}

###########################################################################

# Cleanup

sub cleanup
{
    if (!$KeepTemp && $TMPDIR && -d $TMPDIR) {
	foreach $file ("$IN_AVI","$OUT_AVI","$STDOUT_LOG","$STDERR_LOG") {
	    if (-f "$TMPDIR/$file") {
		unlink "$TMPDIR/$file"
		    or print STDERR "unlink($TMPDIR/$file): $!\n";
	    }
	}
        rmdir $TMPDIR or print STDERR "rmdir($TMPDIR): $!\n";
    }
}

###########################################################################

# Fatal error (plus cleanup)--use this instead of die

sub fatal
{
    print STDERR "$_[0]\n";
    &cleanup();
    exit(-1);
}

###########################################################################

# Run transcode with the given input file and arguments, and return the
# contents of the output file.  Returns undef if transcode exits with an
# error or the output file does not exist.

sub transcode
{
    my ($indata,@args) = @_;
    my $outdata;
    local *F;
    local $/ = undef;

    open F, ">$TMPDIR/$IN_AVI" or &fatal("create $TMPDIR/$IN_AVI: $!");
    syswrite(F, $indata) == length($indata) or &fatal("write $TMPDIR/$IN_AVI: $!");
    close F;
    my $pid = fork();
    &fatal("fork(): $!") if !defined($pid);
    if (!$pid) {
        open STDIN, "</dev/null";
        open STDOUT, ">$TMPDIR/$STDOUT_LOG" if !$Verbose;
        open STDERR, ">$TMPDIR/$STDERR_LOG" if !$Verbose;
	@args = ("transcode", "-i", "$TMPDIR/$IN_AVI",
                              "-o", "$TMPDIR/$OUT_AVI", @args);
	print join(" ",@args)."\n" if $Verbose;
	exec @args or die;
    }
    &fatal("waitpid($pid): $!") if !waitpid($pid,0);
    return undef if $? != 0;
    open F, "<$TMPDIR/$OUT_AVI" or return undef;
    $outdata = <F>;
    close F;
    return $outdata;
}

###########################################################################

# Add a test to the global list of tests.  Pass the test name in $name, the
# the list of dependent tests (array reference) in $deps, the function to
# call in $func, and any parameters to the function in @args.  The function
# should return undef for success or an error message (string) for failure.
#
# If $func and @args are omitted, the test becomes an "alias" for all tests
# it depends on; when run, its dependencies are executed but nothing is
# done for the test itself.

sub add_test
{
    my ($name, $deps, $func, @args) = @_;
    $Tests{$name} = {deps => $deps, func => $func, args => [@args], run => 0};
    push @Tests, $name;
}

###########################################################################

# Run a transcode test (including any dependencies) and print the result.
# Pass the test name in $name.  Returns 1 if the test succeeded, 0 if it
# (or any dependencies) failed.

sub run_test
{
    my ($name) = @_;
    my $result;
    local $| = 1;

    if (!$Tests{$name}) {
        print "$name... NOT FOUND (bug in script?)\n";
        $Tests{$name}{'run'} = 1;
        $Tests{$name}{'succeeded'} = 0;
        return 0;
    }
    $Tests{$name}{'recurse'}++;
    foreach $dep (@{$Tests{$name}{'deps'}}) {
        my $res2 = 0;
        if (!$Tests{$dep}) {
            $res2 = &run_test($dep);  # will print error message
        } elsif (!$Tests{$dep}{'run'}) {
            if ($Tests{$dep}{'recurse'}) {
		$result = "dependency loop in test script";
            } else {
		$res2 = &run_test($dep);
	    }
        } else {
	    $res2 = $Tests{$dep}{'succeeded'};
	}
        if (!$res2) {
	    $result = "dependency `$dep' failed" if !defined($result);
	    last;
        }
    }
    my $func = $Tests{$name}{'func'};
    my $args = $Tests{$name}{'args'};
    if ($func) {
	print "$name... ";
	if (!defined($result)) {
	    $result = &$func(@$args);
	}
    }
    $Tests{$name}{'run'} = 1;
    if (defined($result)) {
        print "FAILED ($result)\n" if $func;
        $Tests{$name}{'succeeded'} = 0;
    } else {
        print "ok\n" if $func;
        $Tests{$name}{'succeeded'} = 1;
    }
    $Tests{$name}{'recurse'}--;
    return $Tests{$name}{'succeeded'};
}

###########################################################################
###########################################################################

# Various tests.

###########################################################################

# Test "-x raw,null -y raw,null", to ensure raw input and output works.
# Pass a CSP_* constant in $csp.  The output frame will be stored in
# $VideoData{$csp}.

sub test_raw_raw
{
    my ($csp) = @_;

    my $raw_in = &gen_raw_avi(WIDTH, HEIGHT, NFRAMES, $csp);
    my $i = index($raw_in, "movi00db");  # find first frame
    &fatal("***BUG*** can't find frame in test input") if $i < 0;
    my $raw_frame = substr($raw_in, $i+4,
                           8+unpack("V",substr($raw_in,$i+8,4)));
    my @colorspace_args = ();
    push @colorspace_args, "--use_rgb" if $csp == CSP_RGB;
    $VideoData{$csp} = &transcode($raw_in, "-x", "raw,null", "-y",
                                   "raw,null", @colorspace_args);
    return "transcode failed" if !$VideoData{$csp};
    $i = index($VideoData{$csp}, "movi00db");
    return "can't find any frames in output file" if $i < 0;
    return "bad output data"
        if (substr($VideoData{$csp}, $i+4, length($raw_frame)*NFRAMES)
            ne $raw_frame x NFRAMES);
    return undef;
}

###########################################################################

# Test raw input/output with colorspace conversion.

sub test_raw_raw_csp
{
    my ($csp_in, $csp_out) = @_;

    my $data = $VideoData{$csp_in};
    &fatal("***BUG*** missing input data for CSP $csp_in") if !$data;
    &fatal("***BUG*** missing output data for CSP $csp_out")
        if !$VideoData{$csp_out};
    my $outcsp_arg = $csp_out==CSP_RGB ? "rgb" : "i420";
    my @colorspace_args = ();
    push @colorspace_args, "--use_rgb" if $csp_in == CSP_RGB;
    $data = &transcode($data, "-x", "raw,null", "-y", "raw,null",
                       "-F", $outcsp_arg, @colorspace_args);
    return "transcode failed" if !$data;
    return "bad output data" if $data ne $VideoData{$csp_out};
    return undef;
}

###########################################################################

# Test a generic video import module.  Pass the module name (with any
# parameters) in $vimport_mod, and a CSP_* colorspace constant in $csp
# (this is the target colorspace to be used by transcode).

sub test_import
{
    my ($vimport_mod, $dataref, $csp) = @_;

    &fatal("***BUG*** missing output data for CSP $csp")
        if !$VideoData{$csp};
    my @colorspace_args = ();
    push @colorspace_args, "--use_rgb" if $csp == CSP_RGB;
    $data = &transcode($$dataref, "-x", "$vimport_mod,null", "-y", "raw,null",
                       @colorspace_args);
    return "transcode failed" if !$data;
    return "bad output data" if $data ne $VideoData{$csp};
    return undef;
}

###########################################################################

# Test a generic video export module, by running the output back through
# -x ffmpeg.  Pass the module name (with any parameters) in $vexport_mod, a
# CSP_* colorspace constant in $csp, 

sub test_export_x_ffmpeg
{
    my ($vexport_mod, $csp, $F_arg) = @_;

    my $csp_data = $VideoData{$csp};
    &fatal("***BUG*** missing input data for CSP $csp") if !$csp_data;
    my @extra_args = ();
    push @extra_args, "-F", $F_arg if defined($F_arg);
    my @colorspace_args = ();
    push @colorspace_args, "--use_rgb" if $csp == CSP_RGB;
    my $export_data = &transcode($csp_data, "-x", "raw,null", "-y",
                                 "$vexport_mod,null", @extra_args,
                                 @colorspace_args);
    return "transcode (export) failed" if !$export_data;
    my $import_data = &transcode($export_data, "-x", "ffmpeg,null", "-y",
                                 "raw,null", @colorspace_args);
    return "transcode (import) failed" if !$import_data;
    return "bad data" if $import_data ne $VideoData{$csp};
    return undef;
}

###########################################################################
###########################################################################

# Video data generation.

###########################################################################

sub gen_raw_avi
{
    my ($width,$height,$nframes,$csp) = @_;

    my $frame = $csp==CSP_RGB ? &gen_rgb_frame($width,$height)
                               : &gen_yuv_frame($width,$height);
    my $frame_chunk = "00db" . pack("V", length($frame)) . $frame;
    my $movi =
        "LIST" .
        pack("V", 4 + length($frame_chunk)*$nframes) .
        "movi" .
        ($frame_chunk x $nframes);

    my $strl =
        "LIST" .
        pack("V", 0x74) .
        "strl" .
        "strh" .
        pack("V", 0x38) .
        "vids" .
        ($csp==CSP_RGB ? "RGB " : "I420") .
        pack("V", 0) .
        pack("V", 0) .
        pack("V", 0) .
        pack("V", 1) .   # frame rate denominator
        pack("V", 25) .  # frame rate numerator
        pack("V", 0) .
        pack("V", $nframes) .
        pack("V", length($frame)) .
        pack("V", 0) .
        pack("V", 0) .
        pack("V", 0) .
        pack("V", 0) .
        "strf" .
        pack("V", 0x28) .
        pack("V", 0x28) .
        pack("V", $width) .
        pack("V", $height) .
        pack("vv", 1, 24) .  # planes and bits per pixel, in theory
        ($csp==CSP_RGB ? "RGB " : "I420") .
        pack("V", $width*$height*3) .  # image size, in theory
        pack("V", 0) .
        pack("V", 0) .
        pack("V", 0) .
        pack("V", 0);

    my $hdrl =
        "LIST" .
        pack("V", 0x44 + length($strl)) .
        "hdrl" .
        "avih" .
        pack("V", 0x38) .
        pack("V", 1/25 * 1000000) .  # microseconds per frame
        pack("V", 0) .
        pack("V", 0) .
        pack("V", 0x100) .           # AVIF_ISINTERLEAVED
        pack("V", $nframes) .
        pack("V", 0) .
        pack("V", 1) .               # number of streams
        pack("V", 0) .
        pack("V", $width) .
        pack("V", $height) .
        pack("V", 0) .
        pack("V", 0) .
        pack("V", 0) .
        pack("V", 0) .
        $strl;

    return
        "RIFF" .
        pack("V", 4 + length($hdrl) + length($movi)) .
        "AVI " .
        $hdrl .
        $movi;
}

###########################################################################

# The test frame uses the following colors:
#     RGB (253,0,1)      YUV (81,91,239)
#     RGB (2,255,1)      YUV (145,54,35)
#     RGB (2,0,255)      YUV (41,240,111)
#     RGB (0,0,0)        YUV (16,128,128)   (exact conversion)
#     RGB (85,85,85)     YUV (89,128,128)   (exact conversion)
#     RGB (170,170,170)  YUV (162,128,128)  (exact conversion)
#     RGB (255,255,255)  YUV (235,128,128)  (exact conversion)
# which were chosen because they can be converted accurately between RGB
# and YUV colorspaces with 8 bits per component, assuming rounding.

sub gen_yuv_frame
{
    my ($width,$height) = @_;

    # Size of a vertical color bar (1/4 of the frame, multiple of 16 pixels)
    my $barsize = int($width/64) * 16;
    # Size of the black/gray/white bar (everything remaining)
    my $whitesize = $width - 3*$barsize;
    # Height of top 3 sections (1/4 of the frame, multiple of 16 pixels)
    my $height1 = int($height/64) * 16;
    # Height of the bottom section (everything remaining)
    my $height2 = $height - 3*$height1;

    # Color bar part of Y rows
    my $Yright = chr(81)x$barsize . chr(145)x$barsize . chr(41)x$barsize;
    # Y rows (shades of gray from black to white on the right)
    my $Yrow0 = chr( 16)x$whitesize . $Yright;
    my $Yrow1 = chr( 89)x$whitesize . $Yright;
    my $Yrow2 = chr(162)x$whitesize . $Yright;
    my $Yrow3 = chr(235)x$whitesize . $Yright;
    # U and V rows
    my $Urow = chr(128) x (($width-$barsize*3)/2) .
               chr( 91) x ($barsize/2) .
               chr( 54) x ($barsize/2) .
               chr(240) x ($barsize/2);
    my $Vrow = chr(128) x (($width-$barsize*3)/2) .
               chr(239) x ($barsize/2) .
               chr( 35) x ($barsize/2) .
               chr(111) x ($barsize/2);

    # Y plane
    my $Y = $Yrow0 x $height1 . $Yrow1 x $height1 . $Yrow2 x $height1 .
        $Yrow3 x $height2;
    # U and V planes
    my $U = $Urow x ($height/2);
    my $V = $Vrow x ($height/2);

    # Final frame
    return $Y.$U.$V;
}


sub gen_rgb_frame
{
    my ($width,$height) = @_;

    # Size of a vertical color bar (1/4 of the frame, multiple of 16 pixels)
    my $barsize = int($width/64) * 16;
    # Size of the black/gray/white bar (everything remaining)
    my $whitesize = $width - 3*$barsize;
    # Height of top 3 sections (1/4 of the frame, multiple of 16 pixels)
    my $height1 = int($height/64) * 16;
    # Height of the bottom section (everything remaining)
    my $height2 = $height - 3*$height1;

    # Color bar part of one row
    my $color = (chr(253).chr(0).chr(1)) x $barsize .
                (chr(2).chr(255).chr(1)) x $barsize .
                (chr(2).chr(0).chr(255)) x $barsize;
    # Full rows (shades of gray from black to white on the right)
    my $row0 = chr(0)x($whitesize*3) . $color;
    my $row1 = chr(85)x($whitesize*3) . $color;
    my $row2 = chr(170)x($whitesize*3) . $color;
    my $row3 = chr(255)x($whitesize*3) . $color;

    # Final frame
    return $row0 x $height1 .
           $row1 x $height1 .
           $row2 x $height1 .
           $row3 x $height2;
}

###########################################################################
