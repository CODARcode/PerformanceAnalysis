#!/usr/bin/perl

#Get the nodelist for the head node and body nodes on a PBS system
#Usage:
#PATH/PerformanceAnalysis/scripts/polaris/get_nodes.pl HEAD > head_nodes.nodefile
#PATH/PerformanceAnalysis/scripts/polaris/get_nodes.pl BODY > body_nodes.nodefile
#For services:  mpiexec ....  --hostfile head_nodes.nodefile
#For client/app  mpiexec .....  --hostfile body_nodes.nodefile 

$ARGC=scalar @ARGV;
if($ARGC != 1 || ($ARGV[0] ne "HEAD" && $ARGV[0] ne "BODY") ){
    print "Usage <script.pl> <HEAD or BODY>\n";
    print "HEAD requests the script return the head node\n";
    print "BODY requests the script return the remaining nodes\n";
    exit 1;
}

$nodelist=$ENV{PBS_NODEFILE};
open(IN,$nodelist);
@nodes=<IN>;
close(IN);

if(scalar @nodes < 2){
    print "Need at least 2 nodes\n";
    exit(0);
}

if($ARGV[0] eq "HEAD"){
    print $nodes[0];
}else{
    for($i=1;$i<scalar @nodes;$i++){
	print "$nodes[$i]";    
    }
}
