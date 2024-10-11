#!/usr/bin/perl
no warnings qw(experimental);
use feature qw(refaliasing);

$ARGC=scalar @ARGV;

if($ARGC < 1){
    print "Usage: <script> <log file> <options>\n";
    print "Options:\n";
    print "        -analyze_coridx Analyze unmatch correlation IDs\n";
    exit;
}

$logfile = $ARGV[0];

$ana_corid=0;
for($i=1;$i<$ARGC;$i++){
    if($ARGV[$i] == "-analyze_coridx"){
	$ana_corid=1;
	print "Analyzing correlation IDs\n";
    }
}


print "Using log $logfile\n";

open(IN, $logfile);
@lines = <IN>;
close(IN);

$event_active = 0;
$event_n = 0;
$event_nrows = 0;

$count_active = 0;
$count_n = 0;
$count_nrows = 0;

$corid_cidx=-1;  #index of correlation ID counter

%funcid_name_map = ();
%gputhread_info = ();

#Map  (function id)-> (thread idx)-> (data array)
#Where data array has the format  [ (state (0 inactive, 1 active)), (timestamp),  [(elapsed times)] ]
%funcs = ();

%corids = ();
$ncorid_matched=0;

for($i=0;$i<scalar @lines;$i++){
    $line = $lines[$i];

    #print $line;
    #Parse function names. Format:
    #FOUND NEW ATTRIBUTE: timer 17 {Elements:1 Type:string Value:"MPI_Comm_size()" }
    if($line=~m/timer\s(\d+)\s.*Value:\"(.*)\"/){
	$funcid_name_map{$1} = $2;
	#print "Found fid map: $1 : $2\n";

    #Parse correlation ID counter index
    #FOUND NEW ATTRIBUTE: counter 0 {Elements:1 Type:string Value:"Correlation ID" }    
    }elsif($line=~m/counter\s(\d+)\s.*Value:\"Correlation ID\"/){
	$corid_cidx=$1;
	print "Correlation id maps to counter $corid_cidx\n";
	
    #Parse GPU device/context
    #FOUND NEW ATTRIBUTE: MetaData:0:7:CUDA Context {Elements:1 Type:string Value:"1" }
    #FOUND NEW ATTRIBUTE: MetaData:0:7:CUDA Device {Elements:1 Type:string Value:"0" }
    }elsif($line=~m/(\d+):CUDA\s(Context|Device|Stream).*Value:\"(\d+)\"/){
	$thread =$1;
	$property = $2;
	$value = $3;
	if(!exists $gputhread_info{$thread}){
	    $gputhread_info{$thread} = {};
	}
	\%tprops = $gputhread_info{$thread};
	$tprops{$property} = $value;

    #Parse open of event timestamps list for this timestep
    }elsif($line=~m/event_timestamps.*Parsed\ssize\s(\d+)/){
	$event_nrows = $1;
	#print "Rows $event_nrows\n";
	$event_active = 1;
	$event_n = 0;

    #Parse an entry in the event_timestamps array
    }elsif($event_active == 1){
	#print "Active: $line\n";
	#Example line format
	#(45 0 ):0 (45 1 ):0 (45 2 ):5 (45 3 ):1 (45 4 ):21 (45 5 ):1585852643242747

	if(!($line=~m/2\s\)\:(\d+)\s\(\d+\s3\s\)\:(\d+)\s\(\d+\s4\s\)\:(\d+)\s\(\d+\s5\s\)\:(\d+)/)){
	    print "ERR\n";
	    exit;
	}
	$thread = $1;
	$event = $2;
	$func = $3;
	$ts = $4;
	
	#print "$event $func $ts\n";

	#print "\"$func\"\n";
	if(!(exists($funcs{$func}))){
	    $funcs{$func} = {};
	}

	\%tmap = $funcs{$func};
	if(!(exists $tmap{$thread})){
	    $tmap{$thread} = [0, 0, []];
	}
	\@thread_data = $tmap{$thread};
	
	#Example event type attributes
	#FOUND NEW ATTRIBUTE: event_type 0 {Elements:1 Type:string Value:"ENTRY" }
	#FOUND NEW ATTRIBUTE: event_type 1 {Elements:1 Type:string Value:"EXIT" }
	#print "Found func $func event $event\n";
	if($event == 0){
	    if($thread_data[0] == 1){
		print "WARNING previous entry to func $func had no exit thread $thread\n";
	    }		
	    $thread_data[0] = 1;
	    $thread_data[1] = $ts;
	    #print "Found entry to func $func thread $thread\n";
	}elsif($event == 1){
	    if($thread_data[0] == 0){
		print "WARNING found exit without entry for func $func thread $thread\n";
	    }		
	    $thread_data[0] = 0;
	    $elapsed = $ts - $thread_data[1];
	    push(@{$thread_data[2]}, $elapsed);
	    #print "Found exit from func $func thread $thread: elapsed $elapsed\n";
	}	
	
	$event_n++;
	if($event_n == $event_nrows){
	    $event_active = 0;
	}

    #Counter timestamps
    }elsif($line=~m/counter_values.*Parsed\ssize\s(\d+)/){
	$counter_nrows = $1;
	$counter_active = 1;
	$counter_n = 0;

    #Parse an entry in the counter_values array
    }elsif($counter_active == 1){
	#print "Active: $line\n";
	#Example line format
	#(1 0 ):0 (1 1 ):0 (1 2 ):0 (1 3 ):0 (1 4 ):2 (1 5 ):1629924204499736
	#pid rid tid cidx cval ts
	
	if(!($line=~m/2\s\)\:(\d+)\s\(\d+\s3\s\)\:(\d+)\s\(\d+\s4\s\)\:(\d+)\s\(\d+\s5\s\)\:(\d+)/)){
	    print "ERR\n";
	    exit;
	}
	$thread = $1;
	$cid = $2;
	$cval = $3;
	$ts = $4;

	if($cid == $corid_cidx){
	    if(exists($corids{$cval})){
		print "Found matching corid $cval\n";
		delete $corids{$cval};
		$ncorid_matched++;
	    }else{
		print "Found new corid $cval\n";
		$corids{$cval} = [$thread, $cid, $cval, $ts];
	    }
	}
	
	$counter_n++;
	if($counter_n == $counter_nrows){
	    $counter_active = 0;
	}

    }
	   
}

foreach $func (keys %funcs){
    print "--------------------------------------------------------\n";
    $name = "";
    if(!(exists $funcid_name_map{$func})){
	print "Could not find name of function with index $func\n";
    }else{
	$name = $funcid_name_map{$func};
    }

    \%tmap = $funcs{$func};
    foreach $thread (keys %tmap){

	$tinfo = "";
	if(exists $gputhread_info{$thread}){
	    $tinfo = " (GPU";
	    \%tprops = $gputhread_info{$thread};
	    foreach $p (keys %tprops){
		$v = $tprops{$p};
		$tinfo = "$tinfo $p:$v";
	    }
	    $tinfo = "$tinfo)";
	}

	\@thread_data = $tmap{$thread};
	
	\@elapsed = $thread_data[2];
	$n = scalar @elapsed;
	print "Func $func ($name) thread $thread$tinfo executed $n times\n";
	if($n > 0){
	    $mean=0;
	    $stddev=0;
	    for($i=0;$i<$n;$i++){
		print "$elapsed[$i] ";
		$mean = $mean + $elapsed[$i];
		$stddev = $stddev + ($elapsed[$i])*($elapsed[$i]);
	    }
	    $mean = $mean/$n;
	    $stddev = sqrt($stddev/$n - $mean*$mean);    
	    print "\n";
	    print "Mean $mean std.dev $stddev\n";
	}
    }
}

if($ana_corid == 1){
    if($corid_cidx==-1){
	print "Could not find correlation ID counter index!\n";
	exit 1;
    }
    print "Correlation ID counter index is $corid_cidx\n";
    $nunmatched = keys %corids;
    print "Found $ncorid_matched matched and $nunmatched unmatched correlation IDs\n";
    
    foreach $c (keys %corids){
	\@data = $corids{$c};
	$thread = $data[0];
	$ts = $data[3];
	print "$c with timestamp $ts on thread $thread\n";
    }

    
}
