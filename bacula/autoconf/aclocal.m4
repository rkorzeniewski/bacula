# generated automatically by aclocal 1.9.6 -*- Autoconf -*-

# Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
# 2005  Free Software Foundation, Inc.
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.

# AM_CONDITIONAL                                            -*- Autoconf -*-

# Copyright (C) 1997, 2000, 2001, 2003, 2004, 2005
# Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# serial 7

# AM_CONDITIONAL(NAME, SHELL-CONDITION)
# -------------------------------------
# Define a conditional.
AC_DEFUN([AM_CONDITIONAL],
[AC_PREREQ(2.52)dnl
 ifelse([$1], [TRUE],  [AC_FATAL([$0: invalid condition: $1])],
	[$1], [FALSE], [AC_FATAL([$0: invalid condition: $1])])dnl
AC_SUBST([$1_TRUE])
AC_SUBST([$1_FALSE])
if $2; then
  $1_TRUE=
  $1_FALSE='#'
else
  $1_TRUE='#'
  $1_FALSE=
fi
AC_CONFIG_COMMANDS_PRE(
[if test -z "${$1_TRUE}" && test -z "${$1_FALSE}"; then
  AC_MSG_ERROR([[conditional "$1" was never defined.
Usually this means the macro was only invoked conditionally.]])
fi])])

m4_include([gettext-macros/codeset.m4])
m4_include([gettext-macros/gettext.m4])
m4_include([gettext-macros/glibc2.m4])
m4_include([gettext-macros/glibc21.m4])
m4_include([gettext-macros/iconv.m4])
m4_include([gettext-macros/intdiv0.m4])
m4_include([gettext-macros/intmax.m4])
m4_include([gettext-macros/inttypes-pri.m4])
m4_include([gettext-macros/inttypes.m4])
m4_include([gettext-macros/inttypes_h.m4])
m4_include([gettext-macros/isc-posix.m4])
m4_include([gettext-macros/lcmessage.m4])
m4_include([gettext-macros/lib-ld.m4])
m4_include([gettext-macros/lib-link.m4])
m4_include([gettext-macros/lib-prefix.m4])
m4_include([gettext-macros/longdouble.m4])
m4_include([gettext-macros/longlong.m4])
m4_include([gettext-macros/nls.m4])
m4_include([gettext-macros/po.m4])
m4_include([gettext-macros/printf-posix.m4])
m4_include([gettext-macros/progtest.m4])
m4_include([gettext-macros/signed.m4])
m4_include([gettext-macros/size_max.m4])
m4_include([gettext-macros/stdint_h.m4])
m4_include([gettext-macros/uintmax_t.m4])
m4_include([gettext-macros/ulonglong.m4])
m4_include([gettext-macros/wchar_t.m4])
m4_include([gettext-macros/wint_t.m4])
m4_include([gettext-macros/xsize.m4])
m4_include([bacula-macros/largefiles.m4])
m4_include([bacula-macros/os.m4])
