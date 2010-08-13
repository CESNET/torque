# $Id: parse-config.sh,v 1.2 2008/03/06 12:44:06 xdenemar Exp $

eval $(cat $CONFIG_FILE |
       perl -ne '/^\s*[[:alnum:]][-_[:alnum:]]*\s*=/
                 and s/^\s*([^=]*?)\s*=\s?(.*)/\U$1\E="$2"/
                 or next;
                 s/-(?=.*?=)/_/g;
                 print;')

