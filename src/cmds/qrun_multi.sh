#!/bin/bash

if [ $# -lt 4 ]; then
	echo "syntax: $0 <total-jobs> <no-of-threads> <no-of-servers> <no-of-moms>"
	exit 1
fi

function create_cluster {
	for i in {1..3}; do
		/etc/init.d/pbs stop; pkill pbs_mom; pkill pbs_server; ps -elf|grep pbs
	done
	echo "$(dirname "$0")/multisvr $1 18000"
	if [ $1 -gt 1 ]; then
		$(dirname "$0")/multisvr $1 18000
	else
		cp /etc/pbs.conf /etc/pbs.conf_multi
		cp /etc/pbs.conf_single_svr /etc/pbs.conf
	fi
	/etc/init.d/pbs start
	$(dirname "$0")/multimom $2 19000
}

function srv_log {
	. /etc/pbs.conf
	ls -lrt $PBS_HOME/server_logs/*|tail -1|awk '{print $NF}'
}

function submit_jobs {
	njobs=$1
	echo "New thread submitting, jobs=$njobs"

	for i in $(seq 1 $njobs)
	do
		qsub -koed -o /dev/null -e /dev/null -- /bin/true > /dev/null
	done
}

function run_jobs {
	tno=$1
	nmoms=$2
	ncpus=$3

	ct=$(($RANDOM%$nmoms+1))
	cpu=1

	echo "Thread$tno: Running jobs... "

	while read job; do
		if [ $cpu -lt $ncpus ]; then
			jobs+="$job "
			cpu=`expr $cpu + 1`
		else
			ct=$(($RANDOM%$nmoms+1))
			jobs+="$job "
			#echo "qrun -H mom$ct $jobs"
			qrun -H mom$ct $jobs
			cpu=1
			jobs=""
		fi
	done < thread$tno
	if [ ! -z "$jobs" ]; then
		#echo "qrun -H mom$ct $jobs"
		qrun -H mom$ct $jobs
	fi
}

if [ "$1" = "submit" ]; then
	submit_jobs $2
	exit 0
fi

if [ "$1" = "run" ]; then
	run_jobs $2 $3 $4
	exit 0
fi

njobs=$1
nthreads=$2
nservers=$3
nmoms=$4
echo "njobs: $1, nthreads: $2, nservers: $3, nmoms: $4"
create_cluster $nservers $nmoms

qmgr -c 's s scheduling=0'; qmgr -c "s s acl_roots+=`whoami`" > /dev/null 2>&1
qmgr -c 's s job_history_enable=1'
qdel -Wforce `qselect` > /dev/null 2>&1

echo "parameters supplied: nthreads=$nthreads, njobs=$njobs"

start_time=`date +%s%3N`
#nthreads=`cat /proc/cpuinfo | grep "^processor"|wc -l`
jobs_per_thread=$(($njobs/$nthreads))

for i in $(seq 1 $nthreads)
do
	setsid $0 submit $jobs_per_thread none none &
done

wait

end_time=`date +%s%3N`
diff=`bc -l <<< "scale=3; ($end_time - $start_time) / 1000"`
perf=`bc -l <<< "scale=3; $njobs / $diff"`
echo "Time(ms) started=$start_time, ended=$end_time"
echo "Total jobs submitted=$total_jobs, time taken(secs.ms)=$diff, jobs/sec=$perf"

nmoms=`pbsnodes -av|grep free|wc -l`
ncpus=`cat /proc/cpuinfo | grep "^processor"|wc -l`
start_time=`date +%s%3N`
echo "nmoms: $nmoms, ncpus: $ncpus"

#Preprocessing
rm -f thread*
n=1
for job in `qselect -sQ`; do
	echo "$job" >> thread$n
	n=$(( ( n + 1 ) % nthreads ))
done
cat /dev/null > `srv_log`
for i in $(seq 0 "$(( nthreads - 1 ))")
do
	setsid $0 run $i $nmoms $ncpus &
done

wait


end_time=`date +%s%3N`
diff=`bc -l <<< "scale=3; ($end_time - $start_time) / 1000"`
perf=`bc -l <<< "scale=3; $njobs / $diff"`
echo "Time(ms) started=$start_time, ended=$end_time"
echo "Total jobs Running=$total_jobs, time taken(secs.ms)=$diff, jobs/sec=$perf"

echo "Collecting Results..."
sleep 5
. /etc/pbs.conf
pbs_loganalyzer -s `srv_log`

if [ $nservers -eq 1 ]; then
	cp /etc/pbs.conf /etc/pbs.conf_single_svr
	cp /etc/pbs.conf_multi /etc/pbs.conf
fi

