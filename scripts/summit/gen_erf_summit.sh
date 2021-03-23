#!/usr/bin/perl

#A script to generate ERF files for Summit job placement

###################################### USER SHOULD EDIT ###################################################################################
$nhosts=17; #should be the number of hosts (nodes) that the application will run on, plus one for the provDB and pserver. The job script should allocate this many nodes
$nranks_per_host=6; #number of MPI ranks per host (node) for the application

#NB total ranks is ($nhosts -1) * $nranks_per_host

$ncores_per_rank_ad=1; #the number of cores to assign per MPI rank to the AD instances. Remaining cores on socket are divided evenly over the ranks of the application
$ngpus_per_rank_main=1; #number of GPUs per MPI rank for the application
$use_provdb=1; #disable provdb if 0
###################################### END OF USER EDITED SECTION ###################################################################################

if($ngpus_per_rank_main * $nranks_per_host > 6){
    print "Error: too many GPUs per rank\n";
    exit;
}

$nranks_total = ($nhosts -1) * $nranks_per_host;
print "Generating ERF files for ${nranks_total} ranks\n";

#Summit characteristics
$ncores_host=42; #cores per node
$ncores_socket=21; #cores per socket
$socket_offset_core = 88; #core offset of second socket: 22nd physical core on each socket reserved for OS
$socket_offset_gpu = 3; #4 GPUs per socket

#Do the assignment per socket rather than per node to prevent splitting of ranks across sockets

#Pserver and provdb
if($use_provdb){
    #1 socket each
    open(OUT, ">provdb.erf");
    print OUT "cpu_index_using: physical\n";
    $corestart_provdb = 0;
    $coreend_provdb = 4*$ncores_socket - 1;
    print OUT "rank: 0: { host: 1; cpu: {${corestart_provdb}-${coreend_provdb}} } : app 0\n";  #  ; mem: *
    close(OUT);

    open(OUT, ">pserver.erf");
    print OUT "cpu_index_using: physical\n";
    $corestart_pserver = $socket_offset_core;
    $coreend_pserver = $corestart_pserver + 4*$ncores_socket-1;
    print OUT "rank: 0: { host: 1; cpu: {${corestart_pserver}-${coreend_pserver}} } : app 0\n";
    close(OUT);
}else{
    #Both sockets to pserver
    $ncores_provdb=$ncores_socket;
    $ncores_pserver=$ncores_socket;

    open(OUT, ">pserver.erf");
    print OUT "cpu_index_using: physical\n";

    $corestart_pserver = 0;
    $coreend_pserver = 4*$ncores_socket - 1;

    $corestart2_pserver = $socket_offset_core;
    $coreend2_pserver = $corestart2_pserver + 4*$ncores_socket-1;
    print OUT "rank: 0: { host: 1; cpu: {${corestart_pserver}-${coreend_pserver},${corestart2_pserver}-${coreend2_pserver}} } : app 0\n"; #  ; mem: *
    close(OUT);
}

if($nranks_per_socket % 2 != 0){
    print "Expect number of ranks to be a multiple of 2!";
    exit;
}
   
$nranks_per_socket = ${nranks_per_host}/2;
$ncores_per_socket_main = $ncores_socket - $nranks_per_socket * $ncores_per_rank_ad;
$ncores_per_rank_main = 0;
{
    use integer;
    $ncores_per_rank_main = $ncores_per_socket_main / $nranks_per_socket;
}
print "Assigning ${ncores_per_rank_main} cores per rank to main program\n";

$nhosts_job = $nhosts-1;
$hoststart_job = 2; #0 is launch node, 1 is first compute node

open(OUT, ">ad.erf");
print OUT "cpu_index_using: physical\n";

open(OUT2, ">main.erf");
print OUT2 "cpu_index_using: physical\n";

for($h=0;$h<$nhosts_job;$h++){
    $host = $hoststart_job +$h;
    
    for($s=0;$s<2;$s++){
	$rank_off = $h*$nranks_per_host +  $s * $nranks_per_socket;

	$corestart=$s*$socket_offset_core;
	$gpustart = $s*$socket_offset_gpu;

	#Assign first cores to AD
	for($r=0;$r<$nranks_per_socket;$r++){
	    $rank = $r + $rank_off;
	    $coreend = $corestart + 4*$ncores_per_rank_ad -1;
	    print OUT "rank: ${rank}: { host: ${host}; cpu: {${corestart}-${coreend}} } : app 0\n";

	    $corestart = $coreend+1;
	}
	#Then to main
	for($r=0;$r<$nranks_per_socket;$r++){
	    $rank = $r + $rank_off;
	    $coreend = $corestart + 4*$ncores_per_rank_main -1;
	    
	    $gpuend = $gpustart + $ngpus_per_rank_main - 1;
	    $gpu_str = "$gpustart - $gpu_end";
	    if($ngpus_per_rank_main == 1){
		$gpu_str = "$gpustart";
	    }
	    print OUT2 "rank: ${rank}: { host: ${host}; cpu: {${corestart}-${coreend}}  ; gpu: {${gpu_str}} } : app 0\n";

	    $gpustart = $gpuend + 1;
	    $corestart = $coreend+1;
	}
    }
}
close(OUT);
close(OUT2);

