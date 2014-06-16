#!/bin/bash

function dead_quota_size {
	du -sk $1 2>/dev/null | sed 's/\t/ /' | awk '{ print $1 }';
}
