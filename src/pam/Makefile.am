include $(top_srcdir)/buildutils/config.mk

libdir=/lib/security

lib_LTLIBRARIES = pam_pbssimpleauth.la

AM_CFLAGS = -DPBS_SERVER_HOME=\"$(PBS_SERVER_HOME)\"

EXTRA_DIST = README.pam

pam_pbssimpleauth_la_SOURCES = pam_pbssimpleauth.c
pam_pbssimpleauth_la_LDFLAGS = -module


