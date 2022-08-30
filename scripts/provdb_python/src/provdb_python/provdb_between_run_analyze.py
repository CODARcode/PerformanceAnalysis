#Between-run analysis report
import provdb_python.provdb_interact as pdb
import provdb_python.provdb_analyze as pa
import pymargo
from pymargo.core import Engine
import json
import sys
import copy

#Get a map of the function name to its func stats for those functions that had anomalies
def getFuncStatsMap(db):
    func_stats = db.getGlobalDB().all('func_stats')
    out = {}
    for f in func_stats:
        if(f['anomaly_metrics'] != None):
            fname = f['fname']
            out[fname] = f
    return out



def pairMax(t):
    if t[0] == None:
        return t[1]
    elif t[1] == None:
        return t[0]
    else:
        return max(t[0],t[1])

def secondsStr(t):
    if(t == None):
        return "-"
    else:
        secs = float(t) / 1e6 #Tau gives times in microseconds
        return "%.2f" % secs


def stringRowPadded(row, widths):
    assert(len(row) == len(widths))
    s = ""
    for i in range(len(row)-1):
        s = "%s%s | " % (s, row[i].ljust(widths[i]) )
    s = "%s%s" % (s, row[-1])
    return s

#Print a table width padding
def printTable(labels, table):
    ncol = len(labels)
    maxwidths = [len(l) for l in labels]
    for r in table:
        assert(len(r) == ncol)
        for i in range(ncol):
            maxwidths[i] = max( len(r[i]), maxwidths[i] )
    h = stringRowPadded(labels, maxwidths)
    print(h)
    h = "-" * len(h)
    print(h)
    for r in table:
        print(stringRowPadded(r, maxwidths))

        
    
def provdb_between_run_analysis(args):
    if(len(args) != 4):
        print("Arguments: <dir1> <nshards1> <dir2> <nshards2>")
        sys.exit(0)
    dir1=args[0]
    nshards1 = int(args[1])
    dir2=args[2]
    nshards2 = int(args[3])

    with Engine('na+sm', pymargo.server) as engine:
        db1 = pdb.provDBinterface(engine, "%s/provdb.%%d.unqlite" % dir1, nshards1)
        db2 = pdb.provDBinterface(engine, "%s/provdb.%%d.unqlite" % dir2, nshards2)
        print("")
        print("")
        
        #Get the map of function name to its stats for functions with anomalies
        fstats1 = getFuncStatsMap(db1)
        fstats2 = getFuncStatsMap(db2)

        #Get a combined map of function name to accumulated severity so we can sort
        sev_map_combined = {}
        for k in fstats1.keys():
            sev_map_combined[k] = (fstats1[k]['anomaly_metrics']['severity']['accumulate'], None)
        for k in fstats2.keys():
            v = fstats2[k]['anomaly_metrics']['severity']['accumulate']
            if k in sev_map_combined:
                sev_map_combined[k] = (sev_map_combined[k][0], v)
            else:                
                sev_map_combined[k] = (None,v)
        
        #Get the top10 functions. Take the maximum accumulated severity to represent the pair
        sev_map_combined_sorted = sorted(sev_map_combined.items(), key=lambda kv: pairMax(kv[1]), reverse=True)
        nkeep = min(10, len(sev_map_combined))

        #Generate the comparison table
        header = ["Acc. severity", "Anom. count", "Anom. freq (%)", "Rel. anom. time", "Function name"]
        tab = [ ]
        for i in range(nkeep):
            entry = sev_map_combined_sorted[i]  #fmt  (fname, (t1,t2))
            fname = entry[0]

            #accum severity
            t1 = secondsStr(entry[1][0])
            t2 = secondsStr(entry[1][1])
            tcomp = "%s %s" % (t1,t2)

            #anom counts
            c1 = fstats1[fname]['anomaly_metrics']['anomaly_count']['accumulate']
            c2 = fstats2[fname]['anomaly_metrics']['anomaly_count']['accumulate']            
            counts = "%d %d" % (c1,c2)

            #anomaly frequency
            f1 = float(c1) / fstats1[fname]["runtime_profile"]["exclusive_runtime"]["count"] * 100
            f2 = float(c2) / fstats2[fname]["runtime_profile"]["exclusive_runtime"]["count"] * 100
            freq = "%.1f %.1f" % (f1,f2)
            
            #relative time in anomalous executions
            r1 = float(entry[1][0]) / fstats1[fname]["runtime_profile"]["exclusive_runtime"]["accumulate"]
            r2 = float(entry[1][1]) / fstats2[fname]["runtime_profile"]["exclusive_runtime"]["accumulate"]
            reltime = "%.2f %.2f" % (r1,r2)
            
            tab.append( [tcomp, counts, freq, reltime, fname ] )

        printTable(header, tab)

        print("")
        del db1
        del db2
        engine.finalize()
