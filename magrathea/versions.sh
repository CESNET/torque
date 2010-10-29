#! /bin/bash
# $Id: versions.sh,v 1.7 2008/08/08 10:59:14 xdenemar Exp $

OUTPUT="$1"; shift

echo "Generating versions:"

tag='HEAD'
if [[ -r CVS/Tag ]]; then
    read <CVS/Tag
    tag="${REPLY#[TDN]}"
fi

echo "#define VERSION_TAG \"$tag\"" >"$OUTPUT"
echo "#define VERSIONS \\" >>"$OUTPUT"

fqdn=$(hostname --fqdn)
[[ "$fqdn" == "localhost" ]] && fqdn=$(hostname)

echo "\"Build by: $(whoami)@$fqdn\n\"\\" >>"$OUTPUT"
echo "\"Build at: $(date)\n\"\\" >>"$OUTPUT"
echo "\"CVS Tag: \" VERSION_TAG \"\\n\"\\" >>"$OUTPUT"
echo "\"Source files:\\n\"\\" >>"$OUTPUT"

for src in "$@"; do
    echo "$src"
done |
sort |
while read; do
    src="$REPLY"
    echo -n "  $src: "
    revision=$(
        cat "$src" |
        grep -m 1 '\$Id[:] .*\$' |
        sed -e 's/.*\$Id: .\+,v \(\([0-9]\+\.\)\+[0-9]\+\) .*/\1/')
    mod=$(find "$src" -printf '%T@' | perl -e 'print gmtime(<>)." UTC";')

    if [[ -n "$revision" ]]; then
        echo "\"  $src\\t$revision\\t$mod\\n\"\\" >>"$OUTPUT"
        echo -e "\t$revision\t$mod"
    else
        echo "\"  $src\\t$mod\\n\"\\" >>"$OUTPUT"
        echo -e "\tmissing\t$mod"
    fi
done

echo '""' >>"$OUTPUT"

