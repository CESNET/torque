#!/bin/bash

function dead_quota_size {
	du -sk "$1" 2>/dev/null | sed 's/\t/ /' | awk '{ print $1 }';
}

function dead_quota_sum {
	if [ -d "$1" ]; then
		find "$1/" -maxdepth 2 -type f -name "*.size" -exec cat {} \; | awk '
		BEGIN { sum=0; } { sum+=$1; } END { print sum; }'
	else
		echo 0
	fi
}

function dead_quota_user_sum {
	if [ -d "$1" ]; then
		find "$1/" -maxdepth 1 -type f -name "*.size" -exec cat {} \; | awk '
		BEGIN { sum=0; } { sum+=$1; } END { print sum; }'
	else
		echo 0
	fi
}
