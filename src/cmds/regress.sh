#!/bin/bash -x

./qstop batch
./qstart batch

./qmgr -c 'p s' | tail -2

./pbsnodes -a | grep node
job=`echo sleep 100|./qsub`
./qstat $job

./qalter -A dog $job
./qhold $job
./qstat $job
./qrls  $job
./qrun  $job
#./qsig  -s SIGUSR1 $job
./qrerun $job
./qdel $job
./qstat

./qdisable batch
./qenable batch


#./qorder
#./nqs2pbs
#./pbsdsh
#./qmove
#./qmsg
#./momctl
#./qselect
#./qterm
