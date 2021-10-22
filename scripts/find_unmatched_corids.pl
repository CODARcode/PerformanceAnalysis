#!/usr/bin/perl

$ARGC=scalar @ARGV;
if($ARGC != 1){
    print "Need dump of ADIOS2 binary";
    exit(-1);
}



open(IN,$ARGV[0]);
$idx=-1;
$active=0;
$ndata=-1;
$i=0;

%corid;

while(<IN>){
    $line=$_;
    if($line=~m/counter\s(\d+)\s.*Correlation ID/){
	$idx=$1;
	print "Found counter index $idx\n";
    }elsif($line=~m/counter_values.*Parsed size (\d+)/){
	$active=1;
	$ndata=$1;
	$i=0;
	print "Found counter data block of size $ndata\n";
    }elsif($active == 1){
	if($line=~m/3 \):${idx} \(\d+ 4 \):(\d+)/){
	    if(exists $corid{$1}){
		delete $corid{$1};
		print "Matched $1\n";
	    }else{
		$corid{$1} = 1;
		print "Added $1\n";
	    }
		
	    
	    #print "$i $idx $1\n";
	    
	}
	    #counter_values {AvailableStepsCount:15 Max:1627915315912147 Min:0 Shape:188, 6 SingleValue:false Type:uint64_t } Parsed size 188 6
	

	$i++;
	if($i == $ndata){
	    print "Ending data block on line $i\n";
	    $active=0;
	}
    }
    
    #counter 0 {Elements:1 Type:string Value:"Correlation ID" }

    #/bld/benchmark_suite/cupti_gpu_kernel_outlier/chimbuko/adios2/data
}
close(IN);

$nunmatched=scalar keys %corid;
print "--------$nunmatched unmatched corids\n";

foreach $id (keys %corid){
    print "--------Unmatched $id\n";
}
