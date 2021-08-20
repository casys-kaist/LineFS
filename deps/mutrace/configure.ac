# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# This file is part of mutrace.
#
# Copyright 2009 Lennart Poettering
#
# mutrace is free software: you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# mutrace is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with mutrace. If not, see <http://www.gnu.org/licenses/>.

AC_PREREQ([2.68])

AC_INIT([mutrace], [0.2], [mzzhgenpr (at) 0pointer (dot) net])
AC_CONFIG_SRCDIR([mutrace.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR(m4)

AM_INIT_AUTOMAKE([foreign 1.10 -Wall -Wno-portability tar-pax])

AC_SUBST(PACKAGE_URL, [http://git.0pointer.de/?p=mutrace.git])

AC_CANONICAL_HOST

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

if type -p stow > /dev/null && test -d /usr/local/stow ; then
   AC_MSG_NOTICE([*** Found /usr/local/stow: default install prefix set to /usr/local/stow/${PACKAGE_NAME}-${PACKAGE_VERSION} ***])
   ac_default_prefix="/usr/local/stow/${PACKAGE_NAME}-${PACKAGE_VERSION}"
fi

#### Checks for programs. ####

# CC

AC_PROG_CC
AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_PROG_GCC_TRADITIONAL
AC_USE_SYSTEM_EXTENSIONS

# C++

AC_PROG_CXX

# GCC flags

AX_APPEND_COMPILE_FLAGS([-Wall -W -Wextra -pipe -Wno-long-long -Winline -Wvla -Wno-overlength-strings -Wunsafe-loop-optimizations -Wundef -Wformat=2 -Wlogical-op -Wsign-compare -Wformat-security -Wmissing-include-dirs -Wformat-nonliteral -Wold-style-definition -Wpointer-arith -Winit-self -Wdeclaration-after-statement -Wfloat-equal -Wmissing-prototypes -Wstrict-prototypes -Wredundant-decls -Wmissing-declarations -Wmissing-noreturn -Wshadow -Wendif-labels -Wcast-align -Wwrite-strings -Wno-unused-parameter -Wno-strict-aliasing -ffast-math -Wp,-D_FORTIFY_SOURCE=2 -fno-common -fdiagnostics-show-option -fno-omit-frame-pointer -fPIC])

#### libtool stuff ####

LT_PREREQ(2.2)
LT_INIT([disable-static])

#### Checks for header files. ####

# ISO
AC_HEADER_STDC

# XPG4-UNIX
AC_CHECK_HEADERS([sys/poll.h])

# Other
AC_CHECK_HEADERS([sys/ioctl.h])
AC_CHECK_HEADERS([byteswap.h])

AC_SEARCH_LIBS([bfd_init], [bfd], [], [AC_MSG_ERROR([*** libbfd not found])])
AC_CHECK_HEADERS([bfd.h], [], [AC_MSG_ERROR([*** libbfd headers not found])])

AC_CHECK_HEADERS([libiberty/libiberty.h], [], [AC_MSG_ERROR([*** libiberty headers not found])])

AC_HEADER_STDBOOL

#### Typedefs, structures, etc. ####

AC_C_CONST
AC_C_BIGENDIAN
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_CHECK_TYPES(ssize_t, , [AC_DEFINE([ssize_t], [signed long],
    [Define ssize_t if it is not done by the standard libs.])])
AC_TYPE_OFF_T
AC_TYPE_UID_T
AC_TYPE_UINT64_T

AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([clock_gettime])

#### Large File-Support (LFS) ####

AC_SYS_LARGEFILE

AX_PTHREAD

###################################
#            Output               #
###################################

AC_CONFIG_FILES([
Makefile
])
AC_OUTPUT

echo "
 ---{ $PACKAGE_NAME $VERSION }---

    prefix:                 ${prefix}
    sysconfdir:             ${sysconfdir}
    localstatedir:          ${localstatedir}
    Compiler:               ${CC}
    CFLAGS:                 ${CFLAGS}
"
