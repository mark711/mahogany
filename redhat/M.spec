# Purpose:  The .spec file for building Mahogany RPM
# Version:  $Id$

# version and release
%define VERSION 0.67
%define RELEASE 0

# default installation directory
%define prefix /usr/local

# we can build 2 different packages from this spec file: a quartstatically
# linked version (wxGTK linked statically, the rest dynamically) or a
# dynamically linked one (we might also build a semistatic version with both
# wxGTK abd GTK libs linked statically)
%define static 1
%if %{static}
%define MAKETARGET quartstatic
%else
%define MAKETARGET
%endif

Summary: Mahogany email and news client
Name: mahogany
Version: %{VERSION}
Release: %{RELEASE}
License: GPL
Group: X11/Applications/Networking
Source: ftp://ftp.sourceforge.net/pub/sourceforge/mahogany/mahogany-%{VERSION}.tar.bz2
URL: http://mahogany.sourceforge.net/
Packager: Vadim Zeitlin <vadim@wxwindows.org>
Provides: mua

# doesn't seem to work for the programs?
# BuildPreReq: wx-config
BuildPrereq: bison byacc tetex-latex

%if !%{static}
Requires: wxwin
%endif

Prefix: %prefix
BuildRoot: /var/tmp/%{name}-root
Icon: mahogany.gif

%description
This package contains Mahogany, a powerful, flexible GUI mail and news client
using GTK+ toolkit. Mahogany supports remote POP3, IMAP4, NNTP servers as well
as local MBX, MBOX, MH and news spool folders and sending mail using SMTP or
local MTA. SSL and TLS are supported for all of the protocols above.

%prep
# the name is the same whether the package name is mahogany or mahogany-dynamic
%setup -n mahogany-%{VERSION}

%build
if [ ! -f configure ]; then
  autoconf
fi

CFLAGS="$RPM_OPT_FLAGS" \
   ./configure --prefix=$RPM_BUILD_ROOT/%{prefix} $CONFIG_FLAGS

#if [ "x%{MAKETARGET}" = "xquartstatic" ]; then
    # be nice and check for existence of static library before starting to build
    # (see bug http://mahogany.tt-solutions.com/cgi-bin/mbugs/show_bug.cgi?id=873)
#    libwx=$($(echo @WX_CONFIG_PATH@ | sed `grep WX_CONFIG_PATH config.status`) \
#                --static --libs | \
#                    sed 's@^.* \(/.*/libwx_based-[0-9.]\+.a\) .*$@\1@')
#    if [ ! -f $libwx ]; then
#        echo "You need to build static wxWidgets library $libwx first!"
#        exit 1
#    fi
#fi

# also check if we have everything we need to build the docs (bug 874)
if [ $(echo @MAKE_HAVE_DOCTOOLS@ | \
        sed `grep MAKE_HAVE_DOCTOOLS config.status`) = "no" ]; then
    if [ ! -f doc/Mdocs-%{VERSION}.tar.gz ]; then
        echo "Please download Mdocs-%{VERSION}.tar.gz file or install "\
             "the missing tools required for the docs generation!"
        exit 1
    fi
fi

# we have to fix M_PREFIX in config.h because the package will be later
# installed in just %prefix, so fallback paths hardcoded into the binary
# shouldn't contain RPM_BUILD_ROOT
sed "s@$RPM_BUILD_ROOT/@@g" include/config.h > include/config.h.new &&
mv include/config.h.new include/config.h

# if MAKE is not set, find the best value ourselves
if [ "x$MAKE" = "x" ]; then
  if [ "x$SMP" != "x" ]; then
    export MAKE="make -j $SMP"
  else
    export MAKE=make
  fi
fi

$MAKE %{MAKETARGET}

%install
export PATH=/sbin:$PATH
make -k install
make install_rpm

%clean
rm -rf $RPM_BUILD_ROOT

%post

echo -e "# added by rpm installation\nGlobalDir=%prefix" >> %prefix/share/mahogany/M.conf

%postun

%files -f filelist
