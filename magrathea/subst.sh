#! /bin/bash
# $Id: subst.sh,v 1.3 2007/12/10 23:17:00 xdenemar Exp $

SUBST="s|@BIN_DIR@|$BIN_DIR|g; \
       s|@SBIN_DIR@|$SBIN_DIR|g; \
       s|@RUN_DIR@|$RUN_DIR|g; \
       s|@CONF_DIR@|$CONF_DIR|g; \
       s|@INIT_DIR@|$INIT_DIR|g; \
       s|@PBS_MOM@|$PBS_MOM|g; \
       s|@VMM@|$VMM|g"

exec sed -e "$SUBST"

