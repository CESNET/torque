#!/bin/bash

mkdir -p /var/glite

ticket=/var/glite/krb5cc_lb

KTUTIL="/usr/sbin/ktutil"
HOSTNAME="/bin/hostname"
KINIT="/usr/bin/kinit"

PRINCIPAL=`${KTUTIL} list 2>/dev/null | grep $($HOSTNAME -f) | grep host | awk '//{print $3}'|uniq`

KRB5CCNAME="FILE:${ticket}0" ${KINIT} -k $PRINCIPAL

chown glite:glite ${ticket}0

mv ${ticket}0 $ticket
