for task in `ls /home/chimbuko/lammps/test-for-ml/ | sed 's/^test-for-ml.//' | tr '.' ' ' | awk '{print $1}' | grep '+' | sort | uniq`; do 
    echo processing tid=$task; 
    nl=`echo $task | tr '+' ' ' | awk '{print $1}'`
    echo '# of lammps' $nl 
    for jid in `ls -d /home/chimbuko/lammps/test-for-ml/test-for-ml.$task.* | sed "s/.*\.//g"`; do 
        echo Processing jid=$jid; 
        ls /home/chimbuko/lammps/test-for-ml/test-for-ml.$task.$jid/lammps_traces/
        for nid in `seq 0 1 $(($nl -1))`; do 
          echo Processing nid=$nid;
          /home/sjyoo/codar/x86_64/bin/tau_trace2json /home/chimbuko/lammps/test-for-ml/test-for-ml.$task.$jid/lammps_traces/tautrace.$nid.0.0.trc /home/chimbuko/lammps/test-for-ml/test-for-ml.$task.$jid/lammps_traces/events.$nid.edf > test-for-ml-lammps-$task.$jid.$nid.json;
        done;
    done; 
done
