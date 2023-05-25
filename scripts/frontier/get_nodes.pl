#!/usr/bin/perl

$ARGC=scalar @ARGV;
if($ARGC != 1 || ($ARGV[0] ne "HEAD" && $ARGV[0] ne "BODY") ){
    print "Usage <script.pl> <HEAD or BODY>\n";
    print "HEAD requests the script return the head node\n";
    print "BODY requests the script return the remaining nodes\n";
    exit 1;
}

$nodestr=$ENV{SLURM_JOB_NODELIST};
#$nodestr="test[08897-08898]";

#$nodestr="test[1-33]";
#$nodestr="test[01,2-3,4,5]";
#print "Node string is $nodestr";

@nodes = ();

if(!($nodestr=~m/(.*)\[(.*)\]/)){
    print "Stage 1: Could not parse node list '$nodestr'\n";
    exit 1;
}
$stub = $1;
$list=$2;
@list_split = split(',', $list);
foreach $l (@list_split){
    if($l=~m/(\d+)\-(\d+)/){
	$start=$1;
	$end=$2;
	for($i=$start;$i<=$end;$i++){
	    $i=sprintf("%05d", $i);
	    push(@nodes, "${stub}${i}");
	}
    }elsif($l=~m/(\d+)/){
	$i=$1;
	$i=sprintf("%05d", $i);
	push(@nodes, "${stub}${i}");
    }else{
	print "Stage 2: Could not parse list element '$l'\n";
	exit 1;
    }
}
    
    
if($ARGV[0] eq "HEAD"){
    print "$nodes[0]\n";    
}else{
    $bodynodes = (scalar @nodes) -1;
    $out="";
    for($b=0;$b<$bodynodes;$b++){
	$node = $nodes[$b+1];
	$out = $out . "${node},";
    }
    $out=~s/\,$//;
    print "$out\n";
}
    
