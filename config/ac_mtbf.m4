##*****************************************************************************
## $Id: ac_mtbf.m4,v 1.1.2.5 2006/09/08 01:09:30 achu Exp $
##*****************************************************************************

AC_DEFUN([AC_MTBF],
[
  AC_MSG_CHECKING([for whether to build mtbf modules])
  AC_ARG_WITH([mtbf],
    AC_HELP_STRING([--with-mtbf], [Build mtbf modules]),
    [ case "$withval" in
        no)  ac_mtbf_test=no ;;
        yes) ac_mtbf_test=yes ;;
        *)   AC_MSG_ERROR([bad value "$withval" for --with-mtbf]) ;;
      esac
    ]
  )
  AC_MSG_RESULT([${ac_mtbf_test=yes}])
  
  if test "$ac_mtbf_test" = "yes"; then
      LDFLAGS_SAVE="${LDFLAGS}"
      TMP_MTBF=`mysql_config --libs`
      TMP_MTBF_LDFLAGS=""
      TMP_MTBF_LIBS=""

      for TMPARG in ${TMP_MTBF}; do
          TMP_MTBF_LDFLAGS_COUNT=`echo ${TMPARG} | grep -c "\-L"`
          TMP_MTBF_LIBS_COUNT=`echo ${TMPARG} | grep -c "\-l"`

          if [[ ${TMP_MTBF_LDFLAGS_COUNT} -gt 0 ]]; then
              TMP_MTBF_LDFLAGS="${TMP_MTBF_LDFLAGS} ${TMPARG}"
          elif [[ ${TMP_MTBF_LIBS_COUNT} -gt 0 ]]; then
              TMP_MTBF_LIBS="${TMP_MTBF_LIBS} ${TMPARG}"
          fi
      done

      LDFLAGS="${TMP_MTBF_LDFLAGS}"
      AC_CHECK_LIB([mysqlclient], [mysql_init], [ac_mtbf_have_mysqlclient=yes], [])
      LDFLAGS="${LDFLAGS_SAVE}"
  fi

  if test "$ac_mtbf_have_mysqlclient" = "yes"; then
     AC_DEFINE([WITH_MTBF], [1], [Define if you want the mtbf module.])
     MTBF_LDFLAGS="${TMP_MTBF_LDFLAGS}"   
     MTBF_LIBS="${TMP_MTBF_LIBS}"
     MANPAGE_MTBF=1
     ac_with_mtbf=yes
  else 
     MANPAGE_MTBF=0
     ac_with_mtbf=no
  fi

  AC_SUBST(MTBF_LDFLAGS)
  AC_SUBST(MTBF_LIBS)
  AC_SUBST(MANPAGE_MTBF)
])
