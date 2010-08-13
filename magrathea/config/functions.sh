function log()
{
    echo "$@" >>config.log
}

function print()
{
    log "$@"
    echo "$@" >&2
}

function error()
{
    echo "" >&2
    echo "" >&2
    echo "" >&2
    print "ERROR: $@"
    echo "" >&2
    echo "" >&2
    echo "" >&2
}


function generate_config()
{
    cat >config.h <<EOF
#ifndef CONFIG_H
#define CONFIG_H
$(sed -e 's/^\([^ ]*\) \(.*\)/#define \1 \2/' config.tmp)
#endif
EOF

    cat >config.mak <<EOF
$(sed -e 's/^\([^ ]*\) \(.*\)/override \1 := \2/' config.tmp)
override VMM := \$(shell echo "\$(VMM)" | tr , " ")
EOF

    rm config.tmp
}


function get_params()
{
    src="$1"; shift

    grep 'configure:[A-Z0-9_]\+=' "$src" |
    sed -e 's/^.*configure:\([A-Z0-9_]\+\)=/local compiler_\1=/'
}


function compiler()
{
    mode=$1; shift
    src="./config/$1.c"; shift
    define=$1; shift

    unset INFO
    eval $(get_params $src) || return 1
    [[ $compiler_INFO ]] || return 1

    args=
    case "$mode" in
    compile)
        args="$CFLAGS $compiler_CFLAGS -c"
        ;;

    link)
        if [[ $STATIC -eq 1 ]]; then
            args="$CFLAGS $compiler_CFLAGS $LDFLAGS $compiler_LDFLAGS_STATIC"
        else
            args="$CFLAGS $compiler_CFLAGS $LDFLAGS $compiler_LDFLAGS"
        fi
        ;;

    *)
        print "unknown compiler mode"
        return 1
        ;;
    esac

    log "running $CC $src -o /dev/null $args"

    print -n "$compiler_INFO... "

    output=$($CC $src -o /dev/null $args 2>&1)

    if [[ $? -eq 0 ]]; then
        print "yes"
        echo "$define 1"
        return 0
    else
        print "no"
        log "$output"
        echo "$define 0"
        return 1
    fi
}


