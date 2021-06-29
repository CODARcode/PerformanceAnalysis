#!/usr/bin/perl

if(scalar @ARGV != 5){
    print "Expect: <total number of nodes> <ranks per node> <cores per rank main> <gpus per rank main> <cores per host for AD>\n";
    exit 1;	
}


$nhosts=$ARGV[0]; #should be the number of hosts (nodes) that the application will run on, plus one for the provDB and pserver. The job script should allocate this many nodes
$nranks_per_host=${ARGV[1]}; #number of MPI ranks per host (node) for the application
$ncores_per_rank_main=${ARGV[2]}; #the number of cores to assign per MPI rank to the main app
$ngpus_per_rank_main=${ARGV[3]}; #number of GPUs per MPI rank for the application
$ncores_per_host_ad = ${ARGV[4]}; #number of cores to reserve for Chimbuko AD. Should be divisible by 2 to divide over the 2 sockets
$nthreads_per_rank_ad = 4; #The number of hardware threads per rank of the AD. The are are 4 hardware threads per core, hence if this is set to 4 the AD will have an entire core per rank.

if($ngpus_per_rank_main * $nranks_per_host > 6){
    print "Error: too many GPUs per rank\n";
    exit;
}

#Summit characteristics
$ncores_host=42; #cores per node
$ncores_socket=21; #cores per socket
$nthreads_core=4; #hardware threads per core
$socket_offset_core = 88; #core offset of second socket: 22nd physical core on each socket reserved for OS
$socket_offset_gpu = 3; #3 GPUs per socket

#Generate ERF for Chimbuko services
open(OUT, ">services.erf");
print OUT "cpu_index_using: physical\n";
$corestart_socket0 = 0;
$coreend_socket0 = 4*$ncores_socket - 1;
$corestart_socket1 = $socket_offset_core;
$coreend_socket1 = $socket_offset_core + 4*$ncores_socket - 1;
print OUT "rank: 0: { host: 1; cpu: {${corestart_socket0}-${coreend_socket0}},{${corestart_socket1}-${coreend_socket1}}  } : app 0\n";  #  ; mem: *
close(OUT);

#Generate ERF for AD and main application
if($nranks_per_host % 2 != 0){
    print "Expect number of ranks to be a multiple of 2!";
    exit;
}
if($ncores_per_host_ad % 2 != 0){
    print "Expect number of host cores for the AD to be a multiple of 2!";
    exit;
}
   
$nranks_per_socket = ${nranks_per_host}/2;
$ncores_per_socket_ad = ${ncores_per_host_ad}/2;
$ncores_per_socket_main = ${ncores_per_rank_main} * ${nranks_per_socket};

if($ncores_per_socket_main + $ncores_per_socket_ad > $ncores_socket){
    print "Too many cores per socket!";
    exit;
}

print "Assigning ${ncores_per_rank_main} cores per rank to main program, ${ncores_per_socket_main} cores per socket.\n";
print "Assigning ${ncores_per_socket_ad} cores per socket to the AD\n";

$nhosts_job = $nhosts-1;
$hoststart_job = 2; #0 is launch node, 1 is first compute node

open(OUT, ">ad.erf");
print OUT "cpu_index_using: physical\noverlapping-rs : allow\noversubscribe-cpu : allow\noversubscribe-mem : allow\n";

open(OUT2, ">main.erf");
print OUT2 "cpu_index_using: physical\n";

for($h=0;$h<$nhosts_job;$h++){
    $host = $hoststart_job +$h;
    
    for($s=0;$s<2;$s++){
	$rank_off = $h*$nranks_per_host +  $s * $nranks_per_socket;

	$corestart=$s*$socket_offset_core;

	#AD shares the same resource set for all ranks
	#However we need to manually assign the ranks to cores to prevent all ranks being piled on the first core
	$coreend = $corestart + 4*${ncores_per_socket_ad} - 1;

	for($r=0;$r<$nranks_per_socket;$r++){
	    $rank = $r + $rank_off;
	    $rank_corestart = ( ($nthreads_per_rank_ad*$r) % (4*${ncores_per_socket_ad}) ) + ${corestart}; #round-robin 2 threads per rank
	    $rank_coreend = $rank_corestart + $nthreads_per_rank_ad - 1;
	    if($nthreads_per_rank_ad == 1){
		print OUT "rank: ${rank}: { host: ${host}; cpu: {${rank_corestart}} } : app 0\n";
	    }else{
		print OUT "rank: ${rank}: { host: ${host}; cpu: {${rank_corestart}-${rank_coreend}} } : app 0\n";
	    }
	}



	$gpustart = $s*$socket_offset_gpu;
	$corestart = $coreend+1;

	#Then to main
	for($r=0;$r<$nranks_per_socket;$r++){
	    $rank = $r + $rank_off;
	    $coreend = $corestart + 4*$ncores_per_rank_main -1;
	    
	    $gpuend = $gpustart + $ngpus_per_rank_main - 1;
	    $gpu_str = "; gpu: {$gpustart - $gpu_end}";
	    if($ngpus_per_rank_main == 1){
		$gpu_str = "; gpu: {$gpustart}";
	    }elsif($ngpus_per_rank_main == 0){
		$gpu_str = "";
	    }

	    print OUT2 "rank: ${rank}: { host: ${host}; cpu: {${corestart}-${coreend}}${gpu_str} } : app 0\n";

	    $gpustart = $gpuend + 1;
	    $corestart = $coreend+1;
	}
    }
}
close(OUT);
close(OUT2);

