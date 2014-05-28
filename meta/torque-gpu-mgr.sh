#!/bin/bash

CACHE_LIST=/software/pbs-7.0.0/cache/list_cache
CACHE_UPDATE=/software/pbs-7.0.0/cache/update_cache
MOMCTL=/usr/sbin/momctl
SERVER=$(cat /var/spool/torque/server_name);
JOBID=$1;
OPR=$2;
COUNT=$3;

if [ $# -lt 2 ]; then
	exit 1;
fi

if [ "$OPR" = "lock" ]; then
	# Verify - consistency of aux files
	for i in $(ls /var/spool/torque/aux/gpu-* 2>/dev/null); do
		jobid=$(echo $i | cut -d\- -f2);
		if [ "$($MOMCTL -q jobs 2>/dev/null | grep $jobid)" = "" ]; then
			# Inconsistency detected
			if [ -r /var/spool/torque/aux/$jobid ]; then
				logger -p daemon.crit -t pbs_mom "Node doesn't report job whose files are still present in aux.";
			else
				logger -p daemon.crit -t pbs_mom "Cleaned up stale gpu aux file.";
				rm $i;
			fi
		fi
	done

	# Verify - consistency of cache
	for i in $($CACHE_LIST $SERVER gpu_allocation | grep -v unallocated | grep `hostname` | cut -f3 | sort -u); do
		if [ "$($MOMCTL -q jobs 2>/dev/null | grep $i)" = "" ]; then
			# Inconsistency detected
			if [ -r /var/spool/torque/aux/$i ]; then
				logger -p daemon.crit -t pbs_mom "Node doesn't report job whose files are still present in aux.";
			else
				logger -p daemon.crit -t pbs_mom "Cleaned up stale cache record.";
				for j in $($CACHE_LIST $SERVER gpu_allocation | grep -v unallocated | grep `hostname` | grep $i | cut -f1); do
					$CACHE_UPDATE $SERVER $j gpu_allocation unallocated;
				done
			fi
		fi
	done
fi

case $OPR in
	lock)
	if [ "$COUNT" != "" ]; then
		devices=(`ls /dev/nvidia[0-9]* 2>/dev/null`);

		if [ ${#devices[@]} -gt 0 ]; then
			rm -f /var/spool/torque/aux/gpu-$JOBID

			id=0;
			FREE_GPU=0;
			while [ $id -lt ${#devices[@]} ]; do
				if [ "$(grep ${devices[$id]} /var/spool/torque/aux/gpu-* 2>/dev/null)" != "" ]; then
					devices[$id]=;
				else
					let FREE_GPU+=1;
				fi
				let id+=1;
			done

			if [ $COUNT -gt $FREE_GPU ]; then # if requested more then present - bail out
				exit 3;
			fi

			id=0;
			while [ $COUNT -gt 0 ]; do

				while [ "${devices[$id]}" == "" ]; do
					let id+=1;
				done

				CARD=${devices[$id]};
				echo $CARD >>/var/spool/torque/aux/gpu-$JOBID;
				chown root:root $CARD;
				chmod 666 $CARD;
				$CACHE_UPDATE $SERVER `hostname`:$CARD gpu_allocation $JOBID;

				let id+=1;
				let COUNT-=1;
			done

			chmod 644 /var/spool/torque/aux/gpu-$JOBID &>/dev/null
		fi
	fi
	;;

	rls)
	HOST=`hostname`
	for i in `$CACHE_LIST $SERVER gpu_allocation | grep $JOBID | sed -n 's/'$HOST'://p' | cut -f1`; do
		chmod 666 $i;
		chown root:root $i;
		$CACHE_UPDATE $SERVER `hostname`:$i gpu_allocation unallocated;
	done

	rm -f /var/spool/torque/aux/gpu-$JOBID
	;;
esac

