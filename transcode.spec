# Note that this is NOT a relocatable package
Name: transcode
Version: 0.6.1
Release: 1
Summary:  a linux video stream processing utility
Copyright: GPL
Group: Applications/Multimedia
Packager: Michel Alexandre Salim <salimma1@yahoo.co.uk>

Source: %{name}-%{version}.tar.gz

BuildRoot: %{_tmppath}/%{name}-%{version}-buildroot
Prefix: %{_prefix}

%description
transcode is a text-console video stream processing
tool. Decoding and encoding is done by loading shared library modules
that are responsible for feeding transcode with raw RGB/PCM streams 
(import module) and encoding the frames (export module). It supports
elementary video and audio frame transformations.
Some example modules are included to enable import
of MPEG program streams (VOB), Digital Video (DV), or YUV video 
and export modules for writing DivX;-), OpenDivX, or uncompressed AVI files. 
A set of tools is available to extract and decode the sources into 
raw video/audio streams for import and to enable post-processing of AVI files.

Written by Thomas Östreich (ostreich@theorie.physik.uni-goettingen.de)

%prep
%setup -n %{name}-%{version}


%build
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%{prefix} --mandir=%{prefix}/share/man

make


%install
#------------- ab hier bald unnuetz -------------
perl -pi -e "s|MOD_PATH = /|MOD_PATH = $RPM_BUILD_ROOT|" */Makefile
#------------- bis hier bald unnuetz ------------
make install prefix=$RPM_BUILD_ROOT%{prefix} \
    MOD_PATH=$RPM_BUILD_ROOT%{prefix}/lib/transcode \
    pkgdir=$RPM_BUILD_ROOT%{prefix}/lib/transcode \
    mandir=$RPM_BUILD_ROOT%{prefix}/share/man

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT


# hier kommen Scripte hin, die vor/nach der Installation/Deinstallation
# aufgerufen werden.
%pre
#echo "Vor der Installation :-)"
%post
#echo "Nach der Installation :-)"
%preun
#echo "Vor der De-Installation :-)"
%postun
#echo "Nach der De-Installation :-)"


%files
%defattr(-,root,root)
%{prefix}
%{_mandir}/man1/*
%doc AUTHORS COPYING ChangeLog README TODO docs/*-API.txt

%changelog
* Thu Apr 18 2002 Michel Alexandre Salim
- man pages go to /usr/share/man
- modified for snapshot releases

* Wed Jul 11 2001 Thomas Östreich
- update to transcode v0.3.3
- small changes suggested by VM

* Tue Jul 10 2001 Thomas Östreich
- update to transcode v0.3.2
- added pkgdir in install section

* Tue Jul 10 2001 Volker Moell <moell@gmx.de>
- Wrote this specfile; first build
