#!/usr/bin/perl

if(scalar @ARGV != 4){
    print "Expect: <total number of nodes> <ranks per node> <cores per rank main> <gpus per rank main>\n";
    exit 1;	
}

#We will assume 1 core per rank of the AD

$nhosts=$ARGV[0]; #should be the number of hosts (nodes) that the application will run on, plus one for the provDB and pserver. The job script should allocate this many nodes
$nranks_per_host=${ARGV[1]}; #number of MPI ranks per host (node) for the application
$ncores_per_rank_main=${ARGV[2]}; #the number of cores to assign per MPI rank to the main app
$ngpus_per_rank_main=${ARGV[3]}; #number of GPUs per MPI rank for the application

$ncores_per_host_ad = $nranks_per_host;

if($ngpus_per_rank_main * $nranks_per_host > 6){
    print "Error: too many GPUs per rank\n";
    exit;
}
if($nranks_per_host*$ncores_per_rank_main + $ncores_per_host_ad > 42){
    print "Error: too many CPUs per rank\n";
    exit;
}


#Summit characteristics
$ncores_host=42; #cores per node
$ncores_socket=21; #cores per socket
$socket_offset_core = 21; #core offset of second socket
$socket_offset_gpu = 3; #3 GPUs per socket
$mem_per_socket=309624; #get this number by assigning 2 ranks and using the -S option to view the output URS file

#Generate URS for Chimbuko services
open(OUT, ">services.urs");

print OUT "RS 0: { host: 1, cpu: ";
for($c=0;$c<$ncores_host;$c++){
    print OUT "$c ";
}
print OUT ", mem: 0-${mem_per_socket} 1-${mem_per_socket} }\n";

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

#Divide memory equally over all cores
$mem_per_core = $mem_per_socket / $ncores_socket;

print "Assigning ${ncores_per_rank_main} cores per rank to main program, ${ncores_per_socket_main} cores per socket.\n";
print "Assigning ${ncores_per_socket_ad} cores per socket to the AD\n";
print "Assigning ${mem_per_core} MB memory per core\n";

$mem_per_rank_main = $ncores_per_rank_main * $mem_per_core;


$nhosts_job = $nhosts-1;
$hoststart_job = 2; #0 is launch node, 1 is first compute node

open(OUT, ">ad.urs");

open(OUT2, ">main.urs");

for($h=0;$h<$nhosts_job;$h++){
    $host = $hoststart_job +$h;
    
    for($s=0;$s<2;$s++){
	$rank_off = $h*$nranks_per_host +  $s * $nranks_per_socket;

	$corestart=$s*$socket_offset_core;

	#AD shares the same resource set for all ranks
	#However we need to manually assign the ranks to cores to prevent all ranks being piled on the first core
	$coreend = $corestart + ${ncores_per_socket_ad} - 1;

	for($r=0;$r<$nranks_per_socket;$r++){
	    $rank = $r + $rank_off;
	    $rank_core = ${corestart} + $r;
	    print OUT "RS ${rank}: { host: ${host}, cpu: ${rank_core}, mem: ${s}-${mem_per_core} }\n";
	}

	$gpustart = $s*$socket_offset_gpu;
	$corestart = $coreend+1;

	#Then to main
	for($r=0;$r<$nranks_per_socket;$r++){
	    $rank = $r + $rank_off;
	    $coreend = $corestart + $ncores_per_rank_main -1;
	    
	    $gpuend = $gpustart + $ngpus_per_rank_main - 1;	    
	    $gpu_str = ", gpu:";
	    for($g=${gpustart};$g<=${gpuend};$g++){
		$gpu_str = "$gpu_str $g";
	    }	    
	    if($ngpus_per_rank_main == 0){
		$gpu_str = "";
	    }
	    
	    print OUT2 "RS ${rank}: { host: ${host}, cpu: ";
	    for($c=$corestart;$c<=$coreend;$c++){
		print OUT2 "$c ";
	    }
	    print OUT2 "${gpu_str}, mem: ${s}-${mem_per_rank_main} }\n";

	    $gpustart = $gpuend + $ngpus_per_rank_main;
	    $corestart = $coreend+1;
	}
    }
}
close(OUT);
close(OUT2);

