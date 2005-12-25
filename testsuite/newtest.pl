#!/usr/bin/perl -w
#
# Simple test suite for transcode
# Written by Andrew Church <achurch@achurch.org>
#
# This file is part of transcode, a video stream processing tool.
# transcode is free software, distributable under the terms of the GNU
# General Public License (version 2 or later).  See the file COPYING
# for details.

use POSIX;  # for floor() and ceil()

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
my $ListTests = 0;    # list available tests instead of running them?
my $AccelMode = "";   # acceleration flags for transcode
my %TestsToRun = ();  # user-specified list of tests to run
my %VideoData;        # for saving raw output data to compare against

########
# Initialization
&init();

########
# First make sure that -x raw -y raw works, and get an AVI in transcode's
# output format for comparison purposes
&add_test("raw/raw (YUV)", [],
          "Test input/output using raw YUV420P data",
          \&test_raw_raw, CSP_YUV);
&add_test("raw/raw (RGB)", [],
          "Test input/output using raw RGB data",
          \&test_raw_raw, CSP_RGB);
# Alias for both of the above
&add_test("raw", ["raw/raw (YUV)", "raw/raw (RGB)"],
          "Test input/output using raw data (all colorspaces)");

# Test colorspace conversion
&add_test("raw/raw (YUV->RGB)", ["raw"],
          "Test colorspace conversion from YUV420P to RGB",
          \&test_raw_raw_csp, CSP_YUV, CSP_RGB);
&add_test("raw/raw (RGB->YUV)", ["raw"],
          "Test colorspace conversion from RGB to YUV420P",
          \&test_raw_raw_csp, CSP_RGB, CSP_YUV);
&add_test("raw-csp", ["raw", "raw/raw (YUV->RGB)", "raw/raw (RGB->YUV)"],
          "Test colorspace conversion");

# Test import_ffmpeg on test video
&add_test("-x ffmpeg (raw)", ["raw"],
          "Test input of raw data using import_ffmpeg",
          \&test_import, "ffmpeg", \$VideoData{&CSP_YUV}, CSP_YUV);

# Test various export modules by running their output through ffmpeg
#&add_test("-y af6", ["-x ffmpeg (raw)"],
#          "Test output using export_af6",
#          \&test_export_x_ffmpeg, "af6", CSP_YUV, "Uncompressed I420");
&add_test("-y ffmpeg", ["-x ffmpeg (raw)"],
          "Test output using export_ffmpeg",
          \&test_export_x_ffmpeg, "ffmpeg", CSP_YUV, "mpeg4");
&add_test("-y mjpeg", ["-x ffmpeg (raw)"],
          "Test output using export_mjpeg",
          \&test_export_x_ffmpeg, "mjpeg", CSP_YUV);
&add_test("-y xvid4", ["-x ffmpeg (raw)"],
          "Test output using export_xvid4",
          \&test_export_x_ffmpeg, "xvid4", CSP_YUV);

#X# # Test the PPM export module
#X# &add_test("-y ppm", ["raw"],
#X#           "Test output using export_ppm",
#X#           \&test_export_ppm);

# Test the core video operations

&add_test("-j N", ["raw"],
          "Test -j with one parameter",
          \&test_vidcore, ["-j", "10", \&vidcore_crop, 10, 0, 10, 0]);
&add_test("-j N,N", ["raw"],
          "Test -j with two parameters",
          \&test_vidcore, ["-j", "10,20", \&vidcore_crop, 10, 20, 10, 20]);
&add_test("-j N,N,N", ["raw"],
          "Test -j with three parameters",
          \&test_vidcore, ["-j", "10,20,30", \&vidcore_crop, 10, 20, 30, 20]);
&add_test("-j N,N,N,N", ["raw"],
          "Test -j with four parameters",
          \&test_vidcore, ["-j", "10,20,30,40", \&vidcore_crop, 10,20,30,40]);
&add_test("-j", ["-j N", "-j N,N", "-j N,N,N", "-j N,N,N,N"],
          "Test -j");

&add_test("-I 1", ["raw"],
          "Test -I in interpolation mode",
          \&test_vidcore, ["-I", "1", \&vidcore_deint_interpolate]);
&add_test("-I 3", ["raw"],
          "Test -I in drop-field-and-zoom mode",
          \&test_vidcore, ["-I", "3", \&vidcore_deint_dropzoom]);
&add_test("-I 4", ["raw"],
          "Test -I in drop-field mode",
          \&test_vidcore, ["-I", "4", \&vidcore_deint_dropfield]);
&add_test("-I 5", ["raw"],
          "Test -I in linear-blend mode",
          \&test_vidcore, ["-I", "5", \&vidcore_deint_linear_blend]);
&add_test("-I", ["-I 1", "-I 3", "-I 4", "-I 5"],
          "Test -I");

# Be careful with values here!  Truncation by accelerated rescale() during
# vertical resize can cause false errors (cases where the byte value is off
# by 1 because rounding went the other way).  -X 6 seems to be safe.
&add_test("-X y", ["raw"],
          "Test -X with height only",
          \&test_vidcore, ["-X", "6", \&vidcore_resize, 6*32, 0]);
&add_test("-X 0,x", ["raw"],
          "Test -X with width only",
          \&test_vidcore, ["-X", "0,11", \&vidcore_resize, 0, 11*32]);
&add_test("-X y,x", ["raw"],
          "Test -X with width and height",
          \&test_vidcore, ["-X", "6,11", \&vidcore_resize, 6*32, 11*32]);
&add_test("-X y,x,M", ["raw"],
          "Test -X with width, height, and multiplier",
          \&test_vidcore, ["-X", "24,44,8", \&vidcore_resize, 24*8, 44*8]);
&add_test("-X", ["-X y", "-X 0,x", "-X y,x", "-X y,x,M"],
          "Test -X");

&add_test("-B y", ["raw"],
          "Test -B with height only",
          \&test_vidcore, ["-B", "6", \&vidcore_resize, -6*32, 0]);
&add_test("-B 0,x", ["raw"],
          "Test -B with width only",
          \&test_vidcore, ["-B", "0,11", \&vidcore_resize, 0, -11*32]);
&add_test("-B y,x", ["raw"],
          "Test -B with width and height",
          \&test_vidcore, ["-B", "6,11", \&vidcore_resize, -6*32, -11*32]);
&add_test("-B y,x,M", ["raw"],
          "Test -B with width, height, and multiplier",
          \&test_vidcore, ["-B", "24,44,8", \&vidcore_resize, -24*8, -44*8]);
&add_test("-B", ["-B y", "-B 0,x", "-B y,x", "-B y,x,M"],
          "Test -B");

&add_test("-Z WxH,fast", ["raw"],
          "Test -Z (fast mode)",
          \&test_vidcore, ["-Z", ((WIDTH-11*32)."x".(HEIGHT+6*32).",fast"),
                           \&vidcore_resize, 6*32, -11*32]);
&add_test("-Z WxH", ["raw"],
          "Test -Z (slow mode)",
          \&test_vidcore, ["-Z", ((WIDTH-76)."x".(HEIGHT+76)),
                           \&vidcore_zoom, WIDTH-76, HEIGHT+76]);
&add_test("-Z", ["-Z WxH,fast", "-Z WxH"],
          "Test -Z");

&add_test("-Y N", ["raw"],
          "Test -Y with one parameter",
          \&test_vidcore, ["-Y", "10", \&vidcore_crop, 10, 0, 10, 0]);
&add_test("-Y N,N", ["raw"],
          "Test -Y with two parameters",
          \&test_vidcore, ["-Y", "10,20", \&vidcore_crop, 10, 20, 10, 20]);
&add_test("-Y N,N,N", ["raw"],
          "Test -Y with three parameters",
          \&test_vidcore, ["-Y", "10,20,30", \&vidcore_crop, 10, 20, 30, 20]);
&add_test("-Y N,N,N,N", ["raw"],
          "Test -Y with four parameters",
          \&test_vidcore, ["-Y", "10,20,30,40", \&vidcore_crop, 10,20,30,40]);
&add_test("-Y", ["-Y N", "-Y N,N", "-Y N,N,N", "-Y N,N,N,N"],
          "Test -Y");

&add_test("-r n", ["raw"],
          "Test -r with one parameter",
          \&test_vidcore, ["-r", "2", \&vidcore_reduce, 2, 2]);
&add_test("-r y,x", ["raw"],
          "Test -r with two parameters",
          \&test_vidcore, ["-r", "2,5", \&vidcore_reduce, 2, 5]);
&add_test("-r y,1", ["raw"],
          "Test -r with width reduction == 1",
          \&test_vidcore, ["-r", "2,1", \&vidcore_reduce, 2, 1]);
&add_test("-r 1,x", ["raw"],
          "Test -r with height reduction == 1",
          \&test_vidcore, ["-r", "1,5", \&vidcore_reduce, 1, 5]);
&add_test("-r 1,1", ["raw"],
          "Test -r with width/height reduction == 1 (no-op)",
          \&test_vidcore, ["-r", "1,1"]);
&add_test("-r", ["-r n", "-r y,x", "-r y,1", "-r 1,x", "-r 1,1"],
          "Test -r");

&add_test("-z", ["raw"],
          "Test -z",
          \&test_vidcore, ["-z", undef, \&vidcore_flip_v]),
&add_test("-l", ["raw"],
          "Test -l",
          \&test_vidcore, ["-l", undef, \&vidcore_flip_h]),
&add_test("-k", ["raw"],
          "Test -k",
          \&test_vidcore, ["-k", undef, \&vidcore_rgbswap]),
&add_test("-K", ["raw"],
          "Test -K",
          \&test_vidcore, ["-K", undef, \&vidcore_grayscale]),
&add_test("-G", ["raw"],
          "Test -G",
          \&test_vidcore, ["-G", "1.2", \&vidcore_gamma_adjust, 1.2]),
&add_test("-C", ["raw"],
          "Test -C",
          \&test_vidcore, ["-C", "3", \&vidcore_antialias, 1/3, 1/2],
                          ["--antialias_para", ((1/3).",".(1/2))]),

&add_test("vidcore", ["-j", "-I", "-X", "-B", "-Z", "-Y", "-r", "-z",
                      "-l", "-k", "-K", "-G", "-C"],
          "Test all video core operations");

########
# Run all (or specified) tests
my $exitcode = 0;
if (!$ListTests) {
    foreach $test (@Tests) {
        $exitcode ||= !&run_test($test) if !%TestsToRun || $TestsToRun{$test};
    }
}

########
# Finished, clean up
&cleanup();
exit $exitcode;

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
                print STDERR "Usage: $0 [-kvl] [-a accel] [-t test [-t test...]]\n";
                print STDERR "     -k: don't delete temporary directory\n";
		print STDERR "     -v: verbose (transcode output to stdout/stderr\n";
                print STDERR "     -l: list tests available\n";
                print STDERR "     -a: specify acceleration types (transcode --accel)\n";
                print STDERR "     -t: specify test(s) to run\n";
                exit 0;
            } elsif ($option eq "-k") {
                $KeepTemp = 1;
	    } elsif ($option eq "-v") {
		$Verbose = 1;
            } elsif ($option eq "-l") {
                $ListTests = 1;
            } elsif ($option eq "-a") {
                $optval = $ARGV[++$i] if $optval eq "";
                $AccelMode = $optval;
            } elsif ($option eq "-t") {
                $optval = $ARGV[++$i] if $optval eq "";
                $TestsToRun{$optval} = 1;
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

    if ($AccelMode) {
        push @args, "--accel", $AccelMode;
    }
    push @args, "--zoom_filter", "triangle";
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
    if (open(F, "<$TMPDIR/$OUT_AVI")) {
	$outdata = <F>;
	close F;
	return $outdata;
    }
    my @files = glob("$TMPDIR/$OUT_AVI*");
    if (@files) {
	my $outdata = "";
	foreach $file (@files) {
	    if (!open(F, "<$file")) {
		print STDERR "$file: $!\n";
		return undef;
	    }
	    $outdata .= <F>;
	    close F;
	    unlink $file if !$KeepTemp;
	}
    }
    return undef;
}

###########################################################################

# Add a test to the global list of tests.  Pass the test name in $name, the
# the list of dependent tests (array reference) in $deps, a description of
# the test in $desc, the function to call in $func, and any parameters to
# the function in @args.  The function should return undef for success or
# an error message (string) for failure.
#
# If $func and @args are omitted, the test becomes an "alias" for all tests
# it depends on; when run, its dependencies are executed but nothing is
# done for the test itself.

sub add_test
{
    my ($name, $deps, $desc, $func, @args) = @_;
    $Tests{$name} = {deps => $deps, func => $func, args => [@args], run => 0};
    push @Tests, $name;
    printf "%20s | %s\n", $name, $desc if $ListTests;
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

# Test one or more video core operations.  Each parameter is a reference to
# an array containing, in the following order:
#     * The transcode command-line option, e.g. "-j"
#     * The parameter to the option, or undef if there is no parameter
#     * The function (vidcore_* below) that implements the transformation
#          (can be omitted if no transformation should be executed)
#     * Parameters to the function, if any
# The operations are assumed to be performed by transcode in the order they
# are passed to this function.

sub test_vidcore
{
    my @cmdline = ("-x", "raw,null", "-y", "raw,null", "--use_rgb");

    # Diagonalize the test frame, to try and catch more bugs
    if (!$vidcore_in_frame) {
        $vidcore_in_frame = &gen_rgb_frame(WIDTH,HEIGHT);
        for (my $y = 1; $y < HEIGHT; $y++) {
            $vidcore_in_frame = substr($vidcore_in_frame, 0, $y*WIDTH*3)
                . substr($vidcore_in_frame, $y*WIDTH*3-3, 3)
                . substr($vidcore_in_frame, $y*WIDTH*3, (HEIGHT-$y)*WIDTH*3-3);
        }
    }

    # Generate command line and expected output frame
    my $out_frame = $vidcore_in_frame;
    my $out_w = WIDTH;
    my $out_h = HEIGHT;
    foreach $op (@_) {
        push @cmdline, $$op[0];
        push @cmdline, $$op[1] if defined($$op[1]);
        if ($$op[2]) {
            if (!&{$$op[2]}(\$out_frame, \$out_w, \$out_h, @$op[3..$#$op])) {
                return "Video operation for $$op[0] not implemented";
            }
        }
    }

    # Run transcode
    my $in_avi = &gen_raw_avi(WIDTH, HEIGHT, NFRAMES, CSP_RGB,
                              $vidcore_in_frame);
    my $out_avi = &transcode($in_avi, @cmdline);

    # Check output data
    my $pos = index($out_avi, "movi00db");
    $pos += 4 if $pos >= 0;
    for (my $i = 0; $i < NFRAMES; $i++) {
        if ($pos < 0) {
            return "Can't find video data in transcode output";
        }
        my $len = unpack("V", substr($out_avi, $pos+4, 4));
        if ($len != $out_w*$out_h*3) {
            return "Video frame has bad size ($len, expected "
                . ($out_w*$out_h*3) . ")";
        }
        if (substr($out_avi, $pos+8, $len) ne $out_frame) {
#open F,">/tmp/t";print F $out_frame;close F;open F,">/tmp/u";print F substr($out_avi,$pos+8,$len);close F;
            return "Incorrect data in video frame";
        }
        $pos = index($out_avi, "00db", $pos+8+$len);
    }
    return undef;
}

################

# -j/-Y
sub vidcore_crop
{
    my ($frameref, $widthref, $heightref, $top, $left, $bottom, $right) = @_;
    if ($top > 0) {
        $$frameref = substr($$frameref, $top*$$widthref*3);
    } elsif ($top < 0) {
        $$frameref = ("\0" x (-$top*$$widthref*3)) . $$frameref;
    }
    $$heightref -= $top;
    if ($bottom > 0) {
        $$frameref = substr($$frameref, 0, ($$heightref-$bottom)*$$widthref*3);
    } elsif ($bottom < 0) {
        $$frameref .= "\0" x (-$bottom*$$widthref*3);
    }
    $$heightref -= $bottom;
    my $newframe = "";
    for (my $y = 0; $y < $$heightref; $y++) {
        my $row = substr($$frameref, $y*$$widthref*3, $$widthref*3);
        if ($left > 0) {
            $row = substr($row, $left*3);
        } elsif ($left < 0) {
            $row = ("\0" x (-$left*3)) . $row;
        }
        if ($right > 0) {
            $row = substr($row, 0, length($row) - $right*3);
        } elsif ($right < 0) {
            $row .= "\0" x (-$right*3);
        }
        $newframe .= $row;
    }
    $$widthref -= $left + $right;
    $$frameref = $newframe;
    return 1;
}

################

# -I 1
sub vidcore_deint_interpolate
{
    my ($frameref, $widthref, $heightref) = @_;
    my $Bpl = $$widthref*3;

    my $newframe = "";
    for (my $y = 0; $y < $$heightref; $y++) {
        if ($y%2 == 0) {
            $newframe .= substr($$frameref, $y*$Bpl, $Bpl);
        } elsif ($y == $$heightref-1) {
            $newframe .= substr($$frameref, ($y-1)*$Bpl, $Bpl);
        } else {
            for (my $x = 0; $x < $Bpl; $x++) {
                my $c1 = ord(substr($$frameref, ($y-1)*$Bpl+$x, 1));
                my $c2 = ord(substr($$frameref, ($y+1)*$Bpl+$x, 1));
                $newframe .= chr(int(($c1+$c2+1)/2));
            }
        }
    }
    $$frameref = $newframe;
    return 1;
}


# -I 3
sub vidcore_deint_dropzoom
{
    my ($frameref, $widthref, $heightref) = @_;

    my $oldheight = $$heightref;
    &vidcore_deint_dropfield($frameref, $widthref, $heightref);
    &vidcore_zoom($frameref, $widthref, $heightref, $$widthref, $oldheight);
}


# -I 4
sub vidcore_deint_dropfield
{
    my ($frameref, $widthref, $heightref) = @_;
    my $Bpl = $$widthref*3;

    my $newframe = "";
    for (my $y = 0; $y < int($$heightref/2); $y++) {
        $newframe .= substr($$frameref, ($y*2)*$Bpl, $Bpl);
    }
    $$frameref = $newframe;
    $$heightref = int($$heightref/2);
    return 1;
}


# -I 5
sub vidcore_deint_linear_blend
{
    my ($frameref, $widthref, $heightref) = @_;
    my $Bpl = $$widthref*3;

    my $evenframe = "";
    for (my $y = 0; $y < $$heightref; $y++) {
        if ($y%2 == 0) {
            $evenframe .= substr($$frameref, $y*$Bpl, $Bpl);
        } elsif ($y == $$heightref-1) {
            $evenframe .= substr($$frameref, ($y-1)*$Bpl, $Bpl);
        } else {
            for (my $x = 0; $x < $Bpl; $x++) {
                my $c1 = ord(substr($$frameref, ($y-1)*$Bpl+$x, 1));
                my $c2 = ord(substr($$frameref, ($y+1)*$Bpl+$x, 1));
                $evenframe .= chr(int(($c1+$c2+1)/2));
            }
        }
    }

    my $oddframe = "";
    for (my $y = 0; $y < $$heightref; $y++) {
        if ($y%2 == 1) {
            $oddframe .= substr($$frameref, $y*$Bpl, $Bpl);
        } elsif ($y == 0) {
            $oddframe .= substr($$frameref, ($y+1)*$Bpl, $Bpl);
        } elsif ($y == $$heightref-1) {
            $oddframe .= substr($$frameref, ($y-1)*$Bpl, $Bpl);
        } else {
            for (my $x = 0; $x < $Bpl; $x++) {
                my $c1 = ord(substr($$frameref, ($y-1)*$Bpl+$x, 1));
                my $c2 = ord(substr($$frameref, ($y+1)*$Bpl+$x, 1));
                $oddframe .= chr(int(($c1+$c2+1)/2));
            }
        }
    }

    my $newframe = "";
    for (my $i = 0; $i < $$heightref*$Bpl; $i++) {
        my $c1 = ord(substr($evenframe, $i, 1));
        my $c2 = ord(substr($oddframe, $i, 1));
        $newframe .= chr(int(($c1+$c2+1)/2));
    }

    $$frameref = $newframe;
    return 1;
}

################

# -B/-X
sub vidcore_resize
{
    my ($frameref, $widthref, $heightref, $resize_h, $resize_w) = @_;

    my $newframe = $$frameref;

    if ($resize_h) {
        my $Bpl = $$widthref*3;
        my $new_h = $$heightref + $resize_h;
        my $ratio = $$heightref / $new_h;
        my $oldy_block = int($$heightref/8);
        my $y_block = int($new_h/8);
        my (@source, @weight1, @weight2);
        for (my $i = 0; $i < $y_block; $i++) {
            my $oldi = $i * $$heightref / $new_h;
            $source[$i] = int($oldi);
            if ($oldi+1 >= $oldy_block || $oldi + $ratio < $source[$i]+1) {
                $weight1[$i] = 65536;
                $weight2[$i] = 0;
            } else {
                my $temp = ((int($oldi)+1) - $oldi) / $ratio * (3.14159265358979323846264338327950/2);
                $weight1[$i] = int(65536 * sin($temp)*sin($temp) + 0.5);
                $weight2[$i] = 65536 - $weight1[$i];
            }
        }
        $newframe = "";
        for (my $y = 0; $y < $new_h; $y++) {
            my $block = int($y / $y_block);
            my $i = $y % $y_block;
            my $oldy = $block * $oldy_block + $source[$i];
            if ($weight1[$i] == 0x10000) {
                $newframe .= substr($$frameref, $oldy*$Bpl, $Bpl);
            } else {
                for (my $x = 0; $x < $Bpl; $x++) {
                    my $c1 = ord(substr($$frameref, $oldy*$Bpl+$x, 1));
                    my $c2 = ord(substr($$frameref, ($oldy+1)*$Bpl+$x, 1));
                    my $c = $c1*$weight1[$i] + $c2*$weight2[$i] + 32768;
                    $newframe .= chr($c>>16);
                }
            }
        }
        $$frameref = $newframe;
        $$heightref = $new_h;
    }

    if ($resize_w) {
        my $new_w = $$widthref + $resize_w;
        my $ratio = $$widthref / $new_w;
        my $oldx_block = int($$widthref/8);
        my $x_block = int($new_w/8);
        my (@source, @weight1, @weight2);
        for (my $i = 0; $i < $x_block; $i++) {
            my $oldi = $i * $$widthref / $new_w;
            $source[$i] = int($oldi);
            if ($oldi+1 >= $oldx_block || $oldi + $ratio < $source[$i]+1) {
                $weight1[$i] = 65536;
                $weight2[$i] = 0;
            } else {
                my $temp = ((int($oldi)+1) - $oldi) / $ratio * (3.14159265358979323846264338327950/2);
                $weight1[$i] = int(65536 * sin($temp)*sin($temp) + 0.5);
                $weight2[$i] = 65536 - $weight1[$i];
            }
        }
        $newframe = "";
        for (my $block = 0; $block < $$heightref * 8; $block++) {
            my $y = int($block/8);
            for (my $i = 0; $i < $x_block; $i++) {
                my $oldx = ($block%8) * $oldx_block + $source[$i];
                if ($weight1[$i] == 0x10000) {
                    $newframe .= substr($$frameref, ($y*$$widthref+$oldx)*3,
                                        3);
                } else {
                    for (my $j = 0; $j < 3; $j++) {
                        my $c1 = ord(substr($$frameref,
                                            ($y*$$widthref+$oldx)*3+$j, 1));
                        my $c2 = ord(substr($$frameref,
                                            ($y*$$widthref+$oldx+1)*3+$j, 1));
                        my $c = $c1*$weight1[$i] + $c2*$weight2[$i] + 32768;
                        $newframe .= chr($c>>16);
                    }
                }
            }
        }
        $$frameref = $newframe;
        $$widthref = $new_w;
    }

    return 1;
}

################

# -Z (slow)
# Implemented using triangle filter
sub vidcore_zoom
{
    my ($frameref, $widthref, $heightref, $newwidth, $newheight) = @_;
    my $Bpp = 3;
    my $Bpl = $$widthref*$Bpp;

    my @x_contrib = ();
    my $xscale = $newwidth / $$widthref;
    my $fscale = ($xscale<1 ? 1/$xscale : 1);
    my $fwidth = 1.0 * $fscale;
    for (my $x = 0; $x < $newwidth*$Bpp; $x++) {
        my $center = int($x/$Bpp) / $xscale;
        my $left = ceil($center - $fwidth);
        my $right = floor($center + $fwidth);
        my @contrib = ();
        for (my $i = $left; $i <= $right; $i++) {
            my $weight = ($i-$center) / $fscale;
            $weight = (1 - abs($weight)) / $fscale;
            $weight = 0 if $weight < 0;
            my $n = $i;
            $n = -$n if $n < 0;
            $n = ($$widthref-$n) + $$widthref - 1 if $n >= $$widthref;
            push @contrib, $n*$Bpp + ($x%$Bpp), int($weight*65536);
        }
        push @x_contrib, [@contrib];
    }

    my @y_contrib = ();
    my $yscale = $newheight / $$heightref;
    $fscale = ($yscale<0 ? 1/$yscale : 1);
    $fwidth = 1.0 * $fscale;
    for (my $y = 0; $y < $newheight; $y++) {
        my $center = $y / $yscale;
        my $left = ceil($center - $fwidth);
        my $right = floor($center + $fwidth);
        my @contrib = ();
        for (my $i = $left; $i <= $right; $i++) {
            my $weight = ($i-$center) / $fscale;
            $weight = (1 - abs($weight)) / $fscale;
            $weight = 0 if $weight < 0;
            my $n = $i;
            $n = -$n if $n < 0;
            $n = ($$heightref-$n) + $$heightref - 1 if $n >= $$heightref;
            push @contrib, $n, int($weight*65536);
        }
        push @y_contrib, [@contrib];
    }

    my $tmpframe = "";
    for (my $y = 0; $y < $$heightref; $y++) {
        use integer;
        my @pixels = unpack("C*", substr($$frameref, $y*$Bpl, $Bpl));
        for (my $x = 0; $x < $newwidth*$Bpp; $x++) {
            my $weight = 0x8000;
            for (my $i = 0; $i < @{$x_contrib[$x]}; $i += 2) {
                $weight += $pixels[$x_contrib[$x][$i]] * $x_contrib[$x][$i+1];
            }
            $weight >>= 16;
            $weight = 0 if $weight < 0;
            $weight = 255 if $weight > 255;
            $tmpframe .= chr($weight);
        }
    }

    my @tmpframe = ();
    $Bpl = $newwidth*$Bpp;
    for (my $y = 0; $y < $$heightref; $y++) {
        push @tmpframe, [unpack("C*", substr($tmpframe, $y*$Bpl, $Bpl))];
    }

    my $newframe = "";
    for (my $y = 0; $y < $newheight; $y++) {
        use integer;
        for (my $x = 0; $x < $newwidth*$Bpp; $x++) {
            my $weight = 0x8000;
            for (my $i = 0; $i < @{$y_contrib[$y]}; $i += 2) {
                $weight += $tmpframe[$y_contrib[$y][$i]][$x] * $y_contrib[$y][$i+1];
            }
            $weight >>= 16;
            $weight = 0 if $weight < 0;
            $weight = 255 if $weight > 255;
            $newframe .= chr($weight);
        }
    }

    $$frameref = $newframe;
    $$widthref = $newwidth;
    $$heightref = $newheight;
    return 1;
}

################

# -r
sub vidcore_reduce
{
    my ($frameref, $widthref, $heightref, $reduce_h, $reduce_w) = @_;

    my $newframe = "";
    for (my $y = 0; $y < int($$heightref/$reduce_h); $y++) {
        for (my $x = 0; $x < int($$widthref/$reduce_w); $x++) {
            $newframe .= substr($$frameref,
                                (($y*$reduce_h)*$$widthref+($x*$reduce_w))*3,
                                3);
        }
    }
    $$frameref = $newframe;
    $$widthref = int($$widthref / $reduce_w);
    $$heightref = int($$heightref / $reduce_h);
    return 1;
}

################

# -z
sub vidcore_flip_v
{
    my ($frameref, $widthref, $heightref) = @_;
    my $Bpl = $$widthref*3;

    my $newframe = "";
    for (my $y = $$heightref-1; $y >= 0; $y--) {
        $newframe .= substr($$frameref, $y*$Bpl, $Bpl);
    }
    $$frameref = $newframe;
    return 1;
}

################

# -l
sub vidcore_flip_h
{
    my ($frameref, $widthref, $heightref) = @_;

    my $newframe = "";
    for (my $y = 0; $y < $$heightref; $y++) {
        for (my $x = $$widthref-1; $x >= 0; $x--) {
            $newframe .= substr($$frameref, ($y*$$widthref+$x)*3, 3);
        }
    }
    $$frameref = $newframe;
    return 1;
}

################

# -k
sub vidcore_rgbswap
{
    my ($frameref, $widthref, $heightref) = @_;

    my $newframe = "";
    for (my $i = 0; $i < $$widthref*$$heightref; $i++) {
        $newframe .= substr($$frameref, $i*3+2, 1);
        $newframe .= substr($$frameref, $i*3+1, 1);
        $newframe .= substr($$frameref, $i*3  , 1);
    }
    $$frameref = $newframe;
    return 1;
}

################

# -K
sub vidcore_grayscale
{
    my ($frameref, $widthref, $heightref) = @_;

    my $newframe = "";
    for (my $i = 0; $i < $$widthref * $$heightref; $i++) {
        my $r = ord(substr($$frameref, $i*3  , 1));
        my $g = ord(substr($$frameref, $i*3+1, 1));
        my $b = ord(substr($$frameref, $i*3+2, 1));
        my $c = (19595*$r + 38470*$g + 7471*$b + 32768) >> 16;
        $newframe .= chr($c) x 3;
    }
    $$frameref = $newframe;
    return 1;
}

################

# -G
sub vidcore_gamma_adjust
{
    my ($frameref, $widthref, $heightref, $gamma) = @_;

    my @table = ();
    for (my $i = 0; $i < 256; $i++) {
        $table[$i] = int((($i/255)**$gamma) * 255);
    }

    my $newframe = "";
    for (my $i = 0; $i < $$widthref*$$heightref*3; $i++) {
        $newframe .= chr($table[ord(substr($$frameref, $i, 1))]);
    }
    $$frameref = $newframe;
    return 1;
}

################

# -C
sub vidcore_antialias
{
    my ($frameref, $widthref, $heightref, $aa_weight, $aa_bias) = @_;
    my $Bpl = $$widthref*3;
    my (@table_c, @table_x, @table_y, @table_d);

    for (my $i = 0; $i < 256; $i++) {
        $table_c[$i] = int($i * $aa_weight * 65536);
        $table_x[$i] = int($i * $aa_bias * (1-$aa_weight)/4 * 65536);
        $table_y[$i] = int($i * (1-$aa_bias) * (1-$aa_weight)/4 * 65536);
        $table_d[$i] = int(($table_x[$i] + $table_y[$i] + 1) / 2);
    }

    my $newframe = substr($$frameref, 0, $Bpl);
    for (my $y = 1; $y < $$heightref-1; $y++) {
        $newframe .= substr($$frameref, $y*$Bpl, 3);
        for (my $x = 1; $x < $$widthref-1; $x++) {
            my $UL = substr($$frameref, (($y-1)*$$widthref+($x-1))*3, 3);
            my $U  = substr($$frameref, (($y-1)*$$widthref+($x  ))*3, 3);
            my $UR = substr($$frameref, (($y-1)*$$widthref+($x+1))*3, 3);
            my $L  = substr($$frameref, (($y  )*$$widthref+($x-1))*3, 3);
            my $C  = substr($$frameref, (($y  )*$$widthref+($x  ))*3, 3);
            my $R  = substr($$frameref, (($y  )*$$widthref+($x+1))*3, 3);
            my $DL = substr($$frameref, (($y+1)*$$widthref+($x-1))*3, 3);
            my $D  = substr($$frameref, (($y+1)*$$widthref+($x  ))*3, 3);
            my $DR = substr($$frameref, (($y+1)*$$widthref+($x+1))*3, 3);
            if ((&SAME($L,$U) && &DIFF($L,$D) && &DIFF($L,$R))
             || (&SAME($L,$D) && &DIFF($L,$U) && &DIFF($L,$R))
             || (&SAME($R,$U) && &DIFF($R,$D) && &DIFF($R,$L))
             || (&SAME($R,$D) && &DIFF($R,$U) && &DIFF($R,$L))
            ) {
                for (my $i = 0; $i < 3; $i++) {
                    my $c = $table_d[ord(substr($UL,$i,1))]
                          + $table_y[ord(substr($U ,$i,1))]
                          + $table_d[ord(substr($UR,$i,1))]
                          + $table_x[ord(substr($L ,$i,1))]
                          + $table_c[ord(substr($C ,$i,1))]
                          + $table_x[ord(substr($R ,$i,1))]
                          + $table_d[ord(substr($DL,$i,1))]
                          + $table_y[ord(substr($D ,$i,1))]
                          + $table_d[ord(substr($DR,$i,1))]
                          + 32768;
                    $newframe .= chr($c>>16);
                }
            } else {
                $newframe .= $C;
            }
        }
        $newframe .= substr($$frameref, $y*$Bpl+($Bpl-3), 3);
    }
    $$frameref = $newframe . substr($$frameref, ($$heightref-1)*$Bpl, $Bpl);
    return 1;
}

sub SAME {
    my @pixel1 = unpack("CCC", $_[0]);
    my @pixel2 = unpack("CCC", $_[1]);
    my $diff0 = abs($pixel2[0] - $pixel1[0]);
    my $diff1 = abs($pixel2[1] - $pixel1[1]);
    my $diff2 = abs($pixel2[2] - $pixel1[2]);
    $diff1 = $diff2 if $diff2 > $diff1;
    $diff0 = $diff1 if $diff1 > $diff0;
    return $diff0 < 25;
}

sub DIFF { return !&SAME(@_); }

###########################################################################
###########################################################################

# Video data generation.

###########################################################################

sub gen_raw_avi
{
    my ($width,$height,$nframes,$csp,$frame) = @_;

    if (!defined($frame)) {
        $frame = $csp==CSP_RGB ? &gen_rgb_frame($width,$height)
                               : &gen_yuv_frame($width,$height);
    }
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

# Local variables:
#   indent-tabs-mode: nil
# End:
#
# vim: expandtab shiftwidth=4:
