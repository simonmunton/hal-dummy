dnl Process this file with autoconf to produce a configure script.

# If porting the hal tarball to work well on a distribution search
# for the string "(distro-tweaks required)" for what to tweak.
#
# Patches for that is welcome.
#

AC_PREREQ(2.59c)
AC_INIT(hal-dummy, 0.5.14, s@m)
AM_INIT_AUTOMAKE([gnu 1.9])
AM_MAINTAINER_MODE
AC_CONFIG_MACRO_DIR([m4])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

glib_module="glib-2.0 >= 2.10.0 gobject-2.0 > 2.10.0 dbus-glib-1 >= 0.61"
dbus_module="dbus-1 >= 0.61"

# libtool versioning - this applies to libhal
#
# See http://sources.redhat.com/autobook/autobook/autobook_91.html#SEC91 for details
#
LT_CURRENT=1
LT_REVISION=0
LT_AGE=0
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

AC_ISC_POSIX
AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_C_O
AC_HEADER_STDC
AC_PROG_LIBTOOL
AC_PROG_MAKE_SET
AC_PROG_LN_S
AC_SYS_LARGEFILE

PKG_CHECK_MODULES(DBUS, [$dbus_module])
AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

#### gcc linker flags

if test "x$GCC" = "xyes"; then
  LDFLAGS="-Wl,--as-needed $LDFLAGS"
fi

#### gcc warning flags

if test "x$GCC" = "xyes"; then
  changequote(,)dnl
  case " $CFLAGS " in
  *[\ \	]-Wall[\ \	]*) ;;
  *) CFLAGS="$CFLAGS -Wall" ;;
  esac

  case " $CFLAGS " in
  *[\ \	]-Wchar-subscripts[\ \	]*) ;;
  *) CFLAGS="$CFLAGS -Wchar-subscripts" ;;
  esac

  case " $CFLAGS " in
  *[\ \	]-Wmissing-declarations[\ \	]*) ;;
  *) CFLAGS="$CFLAGS -Wmissing-declarations" ;;
  esac

  case " $CFLAGS " in
  *[\ \	]-Wnested-externs[\ \	]*) ;;
  *) CFLAGS="$CFLAGS -Wnested-externs" ;;
  esac

  case " $CFLAGS " in
  *[\ \	]-Wpointer-arith[\ \	]*) ;;
  *) CFLAGS="$CFLAGS -Wpointer-arith" ;;
  esac

  case " $CFLAGS " in
  *[\ \	]-Wcast-align[\ \	]*) ;;
  *) CFLAGS="$CFLAGS -Wcast-align" ;;
  esac

  case " $CFLAGS " in
  *[\ \	]-Wsign-compare[\ \	]*) ;;
  *) CFLAGS="$CFLAGS -Wsign-compare" ;;
  esac

  if test "x$enable_ansi" = "xyes"; then
    case " $CFLAGS " in
    *[\ \	]-ansi[\ \	]*) ;;
    *) CFLAGS="$CFLAGS -ansi" ;;
    esac

    case " $CFLAGS " in
    *[\ \	]-D_POSIX_C_SOURCE*) ;;
    *) CFLAGS="$CFLAGS -D_POSIX_C_SOURCE=199309L" ;;
    esac

    case " $CFLAGS " in
    *[\ \	]-D_BSD_SOURCE[\ \	]*) ;;
    *) CFLAGS="$CFLAGS -D_BSD_SOURCE" ;;
    esac

    case " $CFLAGS " in
    *[\ \	]-pedantic[\ \	]*) ;;
    *) CFLAGS="$CFLAGS -pedantic" ;;
    esac
  fi
  if test x$enable_gcov = xyes; then
    case " $CFLAGS " in
    *[\ \	]-fprofile-arcs[\ \	]*) ;;
    *) CFLAGS="$CFLAGS -fprofile-arcs" ;;
    esac
    case " $CFLAGS " in
    *[\ \	]-ftest-coverage[\ \	]*) ;;
    *) CFLAGS="$CFLAGS -ftest-coverage" ;;
    esac

    ## remove optimization
    CFLAGS=`echo "$CFLAGS" | sed -e 's/-O[0-9]*//g'`
  fi
  changequote([,])dnl
else
  if test x$enable_gcov = xyes; then
    AC_MSG_ERROR([--enable-gcov can only be used with gcc])
  fi
fi


AC_CONFIG_HEADERS([config.h])

AC_CONFIG_FILES([
Makefile
libhal/Makefile
])

AC_OUTPUT

dnl ==========================================================================
echo "
                    HAL $VERSION
                  ==============

        prefix:                           ${prefix}
        libdir:                           ${libdir}
        libexecdir:                       ${libexecdir}
        bindir:                           ${bindir}
        sbindir:                          ${sbindir}
        datadir:                          ${datadir}
        sysconfdir:                       ${sysconfdir}
        localstatedir:                    ${localstatedir}
        docdir:                           ${docdir}

        compiler:                         ${CC}
        cflags:                           ${CFLAGS}
        ldflags:                          ${LDFLAGS}
        cppflags:                         ${CPPFLAGS}

"
