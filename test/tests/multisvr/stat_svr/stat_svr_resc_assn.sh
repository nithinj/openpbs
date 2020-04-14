./multisvr;
qmgr -c 's s scheduling=0';
./qsub_multi.sh 1 $1 16001-16001;
time PBS_SERVER_INSTANCES=:16002 qstat -f > /dev/null;
PBS_SERVER_INSTANCES=:16002 pbsnodes -av > /dev/null
PBS_SERVER_INSTANCES=:16002 qstat -Qf > /dev/null
PBS_SERVER_INSTANCES=:16002 qstat -Bf > /dev/null
pkill pbs_sched; pbs_sched
PBS_SERVER_INSTANCES=:16001 qmgr -c 's s scheduling=1';
job_ct=0
while [ $job_ct -lt $1 ]
do
echo $job_ct
sleep 10;
job_ct=`PBS_SERVER_INSTANCES=:16001 qstat -f |grep "job_state = R"|wc -l`
done
time PBS_SERVER_INSTANCES=:16002 qstat -f > /dev/null
