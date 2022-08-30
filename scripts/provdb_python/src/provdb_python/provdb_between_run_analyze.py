#Between-run analysis report
import provdb_python.provdb_interact as pdb
import provdb_python.provdb_analyze as pa
import provdb_python.histogram as hist
import pymargo
from pymargo.core import Engine
import json
import sys
import copy
from cmd import Cmd

#Get a map of (pid, function name) to its func stats for those functions that had anomalies
#Note we don't use function index as it may differ between runs. We assume the program index is the same as it is assigned manually
def getFuncStatsMap(db):
    func_stats = db.getGlobalDB().all('func_stats')
    out = {}
    for f in func_stats:
        if(f['anomaly_metrics'] != None):
            fname = f['fname']
            pid = f['app']
            out[(pid,fname)] = f
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

#Interactive component for detailed analysis
class InteractiveAnalysis(Cmd):
    prompt = '> '
    intro = "Type ? to list commands"

    def __init__(self, sev_map_combined_sorted, fstats1, fstats2, db1, db2, nrows):
        Cmd.__init__(self)
        self.sev_map_combined_sorted = sev_map_combined_sorted
        self.fstats1 = fstats1
        self.fstats2 = fstats2
        self.db1 = db1
        self.db2 = db2
        self.nrows = nrows

    def do_exit(self, inp):
        '''Exit the application.'''
        print("Exiting")
        return True

    def do_histcomp(self, inp):
        '''Compare the function execution time histograms for the given table row (1st column)'''
        if(self.nrows == 0):
            print("Table is empty")
            return
        
        if(inp.isdigit() == False or int(inp) < 1 or int(inp) > self.nrows):
            print("Must provide a valid integer between 1 and %d" % self.nrows)
            return
            
        idx = int(inp)
        key = self.sev_map_combined_sorted[idx-1][0]
        pid = key[0]
        fname = key[1]
        print("Comparing histograms for table row %d: Pid %d, function name \"%s\"" % (idx,pid,fname))

        #fids are consistent within a run so we can use them to lookup the histogram
        fid1 = self.fstats1[key]['fid']
        fid2 = self.fstats2[key]['fid']
        m1 = pa.filterGlobalDatabase(self.db1, "ad_model", [ ('pid','==', pid), "&&", ("fid","==",fid1) ])
        if(len(m1) != 1):
            raise Exception("Could not retrieve model from global database 1")
        m2 = pa.filterGlobalDatabase(self.db2, "ad_model", [ ('pid','==', pid), "&&", ("fid","==",fid2) ])
        if(len(m2) != 1):
            raise Exception("Could not retrieve model from global database 2")

        m1 = m1[0]
        m2 = m2[0]
        if("histogram" not in m1["model"].keys()):
            print("Model in database 1 does not appear to be a histogram; perhaps a non-histogram-based algorithm was used?")
            return
        if("histogram" not in m2["model"].keys()):
            print("Model in database 2 does not appear to be a histogram; perhaps a non-histogram-based algorithm was used?")
            return

        h1 = hist.HistogramVBW()
        h1.setup(m1["model"]["histogram"]["Histogram Bin Edges"], m1["model"]["histogram"]["Histogram Bin Counts"])

        h2 = hist.HistogramVBW()
        h2.setup(m2["model"]["histogram"]["Histogram Bin Edges"], m2["model"]["histogram"]["Histogram Bin Counts"])

        h1rb, h2rb, diff = hist.HistogramVBW.diff(h1,h2,return_rebinned=True)
        h1rbe, h1rbc = h1rb.getEdgesAndCounts()
        h2rbe, h2rbc = h2rb.getEdgesAndCounts()
        de, dc = diff.getEdgesAndCounts()

        header = ["Bin start (s)","Bin end (s)","Count 1","Count 2","Diff","Rel. diff"]
        tab = []
        for i in range(0, len(dc)):
            l = "%.3f" % (float(de[i])/1e6)
            u = "%.3f" % (float(de[i+1])/1e6)
            avgc = float(h1rbc[i] + h2rbc[i])/2.0            
            reldiffstr = "-"
            if(avgc != 0.0):
                reldiff = float(dc[i])/avgc
                reldiffstr = "%.2f" % reldiff
            
            tab.append( [l,u,str(h1rbc[i]),str(h2rbc[i]),str(dc[i]),reldiffstr] )
            
        printTable(header, tab)
        
    
        
        
    
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
        header = ["","Acc. severity", "Anom. count", "Anom. freq (%)", "Rel. anom. time", "Pid", "Function name"]
        tab = [ ]
        for i in range(nkeep):
            entry = sev_map_combined_sorted[i]  #fmt  ( (pid,fname), (t1,t2))
            key = entry[0]
            pid = key[0]
            fname = key[1]
            
            #accum severity
            t1 = secondsStr(entry[1][0])
            t2 = secondsStr(entry[1][1])
            tcomp = "%s %s" % (t1,t2)

            #anom counts
            c1 = fstats1[key]['anomaly_metrics']['anomaly_count']['accumulate']
            c2 = fstats2[key]['anomaly_metrics']['anomaly_count']['accumulate']            
            counts = "%d %d" % (c1,c2)

            #anomaly frequency
            f1 = float(c1) / fstats1[key]["runtime_profile"]["exclusive_runtime"]["count"] * 100
            f2 = float(c2) / fstats2[key]["runtime_profile"]["exclusive_runtime"]["count"] * 100
            freq = "%.1f %.1f" % (f1,f2)
            
            #relative time in anomalous executions
            r1 = float(entry[1][0]) / fstats1[key]["runtime_profile"]["exclusive_runtime"]["accumulate"]
            r2 = float(entry[1][1]) / fstats2[key]["runtime_profile"]["exclusive_runtime"]["accumulate"]
            reltime = "%.2f %.2f" % (r1,r2)
            
            tab.append( [str(i+1),tcomp, counts, freq, reltime, str(pid), fname ] )

        printTable(header, tab)

        print("")

        ian = InteractiveAnalysis(sev_map_combined_sorted, fstats1, fstats2, db1, db2, nkeep)
        ian.cmdloop()

        del ian
        del db1
        del db2
        engine.finalize()
