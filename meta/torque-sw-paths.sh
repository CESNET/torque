#!/bin/bash

# Path to list_cache
CACHE_LIST=

if [ -x "/usr/bin/list_cache" ]; then
	CACHE_LIST="/usr/bin/list_cache";
elif [ -x "/usr/sbin/list_cache" ]; then
	CACHE_LIST="/usr/sbin/list_cache";
elif [ -x "/usr/local/sbin/list_cache" ]; then
	CACHE_LIST="/usr/local/sbin/list_cache";
elif [ -x "/software/pbs-7.0.0/cache/list_cache" ]; then
	CACHE_LIST="/software/pbs-7.0.0/cache/list_cache";
fi

if [ -z "$CACHE_LIST" ]; then
	echo "Fatal error, couldn't find list_cache";
	logger -p daemon.crit -t mom_shell_api "Fatal error, couldn't find list_cache";
	exit 1
fi

# Path to update_cache
CACHE_UPDATE=

if [ -x "/usr/bin/update_cache" ]; then
	CACHE_UPDATE="/usr/bin/update_cache";
elif [ -x "/usr/sbin/update_cache" ]; then
	CACHE_UPDATE="/usr/sbin/update_cache";
elif [ -x "/usr/local/sbin/update_cache" ]; then
	CACHE_UPDATE="/usr/local/sbin/update_cache";
elif [ -x "/software/pbs-7.0.0/cache/update_cache" ]; then
	CACHE_UPDATE="/software/pbs-7.0.0/cache/update_cache";
fi

if [ -z "$CACHE_UPDATE" ]; then
	echo "Fatal error, couldn't find update_cache";
	logger -p daemon.crit -t mom_shell_api "Fatal error, couldn't find update_cache";
	exit 1
fi

# Path to delete_cache
CACHE_DELETE=

if [ -x "/usr/bin/delete_cache" ]; then
	CACHE_DELETE="/usr/bin/delete_cache";
elif [ -x "/usr/sbin/delete_cache" ]; then
	CACHE_DELETE="/usr/sbin/delete_cache";
elif [ -x "/usr/local/sbin/delete_cache" ]; then
	CACHE_DELETE="/usr/local/sbin/delete_cache";
elif [ -x "/software/pbs-7.0.0/cache/delete_cache" ]; then
	CACHE_DELETE="/software/pbs-7.0.0/cache/delete_cache";
fi

if [ -z "$CACHE_DELETE" ]; then
	echo "Fatal error, couldn't find delete_cache";
	logger -p daemon.crit -t mom_shell_api "Fatal error, couldn't find delete_cache";
	exit 1
fi

# Server name
SERVER=

if [ -f "/var/spool/torque/server_name" ]; then
	SERVER=$(cat /var/spool/torque/server_name);
fi

if [ -z "$SERVER" ]; then
	echo "Fatal error, couldn't set server name";
	logger -p daemon.crit -t mom_shell_api "Fatal error, couldn't set server name";
	exit 1
fi
