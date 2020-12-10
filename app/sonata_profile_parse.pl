#!/usr/bin/perl

$ARGC=scalar @ARGV;
if($ARGC != 1){
    print "Usage: <script.pl> <filename.csv>\n";
    exit -1;
}

$file = $ARGV[0];
open(IN, $file);

#cf https://xgitlab.cels.anl.gov/sds/margo/blob/master/doc/instrumentation.md#generating-a-profile-and-topology-graph
#Generate profiles by setting env variable MARGO_ENABLE_PROFILING=1

$i=0;
$nrpc;
%rpcnames = ();

while(<IN>){
    $line=$_;
    if($i==0){
	if(!($line=~m/(\d+)/)){
	    print "Could not parse number of RPC names from line $line\n";
	    exit -1;
	}else{
	    $nrpc = $1;
	    print "$nrpc RPCs\n";
	}
    }elsif($i==1){
	if(!($line=~m/\d+\,(.*)/)){
	    print "Could not parse server IP from line $line\n";
	    exit -1;
	}else{
	    $ip = $1;
	    print "IP $ip\n";
	}
    }elsif($i>=2 && $i<2+$nrpc){
	if(!($line=~m/(\w+)\,(.*)/)){
	    print "Could not parse RPC hash/name from line $line\n";
	    exit -1;
	}else{
	    $hash = $1;
	    $name = $2;
	    print "$name -> $hash\n";
	    $rpcnames{"$hash"} = $name;
	}
    }elsif( ($i -2 - $nrpc) % 2 == 0){
	#Even lines format is hash, avg, rpc_breadcrumb, addr_hash, origin_or_target, cumulative, _min, _max, count, abt_pool_size_hwm, abt_pool_size_lwm, abt_pool_size_cumulative, abt_pool_total_size_hwm, abt_pool_total_size_lwm, abt_pool_total

	@vals = split('\s*,\s*', $line);
	if(scalar @vals != 15){
	    print "Could not parse data line $line\n";
	    exit -1;
	}
	$hash = $vals[0];
	if(!(exists $rpcnames{$hash})){
	    print "Could not find hash '$hash' in map\n";
	    exit -1;
	}
	$name = $rpcnames{$hash};
	$avg = $vals[1];
	$count = $vals[8];
	$total = $vals[5];
	$loc = $vals[4];
	print "Func $name Origin/Target $loc Avg $avg Count $count Total $total\n";
    }
    $i++;
}
