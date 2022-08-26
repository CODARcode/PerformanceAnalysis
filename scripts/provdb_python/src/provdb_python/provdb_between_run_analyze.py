#Between-run analysis report
import provdb_python.provdb_interact as pdb
import provdb_python.provdb_analyze as pa
import pymargo
from pymargo.core import Engine
import json
import sys
import copy

def provdb_between_run_analysis(args):
    if(len(args) != 4):
        print("Arguments: <dir1> <nshards1> <dir2> <nshards2>")
        sys.exit(0)
    dir1=args[0]
    nshards1 = int(args[1])
    dir2=args[2]
    nshards2 = int(args[3])

    print("Initial value of provider_idx_next",pdb.provider_idx_next)
    
    with Engine('na+sm', pymargo.server) as engine:
        db1 = pdb.provDBinterface(engine, "%s/provdb.%%d.unqlite" % dir1, nshards1)
        print("Created db1 with provider index",db1.provider_idx)
        print("New value of provider_idx_next",pdb.provider_idx_next)
            
        db2 = pdb.provDBinterface(engine, "%s/provdb.%%d.unqlite" % dir2, nshards2)
        print("Created db2 with provider index",db2.provider_idx)
        print("New value of provider_idx_next",pdb.provider_idx_next)
        
        print("Check provider indices",db1.provider_idx,db2.provider_idx)
        
        #Using the global database, map each function to its accumulated severity
        func_stats_1 = db1.getGlobalDB().all('func_stats')       
        func_stats_2 = db2.getGlobalDB().all('func_stats')


        del db1
        del db2
        engine.finalize()
