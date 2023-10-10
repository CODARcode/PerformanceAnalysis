#A Python module for offline analysis of the provenance database
#Executed as a script it performs some rudimentary analysis
import provdb_python.provdb_interact as pdb
import provdb_python.provdb_analyze as pa
import pymargo
from pymargo.core import Engine
import json
import sys
import copy
from cmd import Cmd
import glob



def provdb_viz_validate(args):
    if(len(args) != 2):
        print("Arguments: <nshards> <viz output dir>")
        sys.exit(0)
    nshards = int(args[0])
    viz_dir=args[1]
    
    with Engine('na+sm', pymargo.server) as engine:
        db = pdb.provDBinterface(engine, r'provdb.%d.unqlite', nshards)
        
        dkeys = ['rid','pid','fid','io_step']
        index=pa.generateIndex(db, dkeys, 'anomalies')
        print(index)
        index_sets = {}
        for k in dkeys:
            index_sets[k] = {}
            for v in index[k].keys():
                index_sets[k][v] = set(index[k][v])

        print(index_sets)

        files=glob.glob("%s/pserver_output_stats_*.json" % viz_dir)

        fail=False
        for f in files:
            print(f)
            fp = open(f)
            v = json.load(fp)
            fp.close()

            if 'anomaly_metrics' in v:
                for anom_group in v['anomaly_metrics']:
                    print(anom_group)
                    fid=str(anom_group['fid'])
                    pid=str(anom_group['app'])
                    rid=str(anom_group['rank'])
                    new_data = anom_group['new_data']
                    nanom=int(new_data['count']['accumulate'])
                    iostep_start=int(new_data['first_io_step'])
                    iostep_end=int(new_data['last_io_step'])
                    print("%d anomalies in [%s,%s] on (%s,%s,%s)" % (nanom,iostep_start,iostep_end,pid,rid,fid))
                    
                    if pid not in index_sets['pid'].keys():
                        print("Could not find any anomalies for this pid!")
                        continue
                    if rid not in index_sets['rid'].keys():
                        print("Could not find any anomalies for this rid!")
                        continue
                    if fid not in index_sets['fid'].keys():
                        print("Could not find any anomalies for this fid!")
                        continue
                    
                    iosets = []
                    for i in range(iostep_start,iostep_end+1):
                        if(str(i) in index_sets['io_step'].keys()):
                            iosets.append(index_sets['io_step'][str(i)])
                    if(len(iosets)==0):
                        print("Could not find any anomalies in this time window!")
                        continue

                        
                    aset = index_sets['rid'][rid] & index_sets['fid'][fid] & index_sets['pid'][pid]
                    acount=0
                    for i in iosets:
                        bset = aset & i
                        acount += len(bset)
                    print("Found %d anomalies matching these keys" % acount)
                    if acount != nanom:
                        print("!!INVALID: Mismatch in number of anomalies: found %d, expected %d" % (acount,nanom) )
                        print("All anomalies in this time window:")
                        for i in range(iostep_start,iostep_end+1):
                            if(str(i) in index_sets['io_step'].keys()):
                                print("IO step %d" % i)
                                for anom in index_sets['io_step'][str(i)]:
                                    print(anom, pa.summarizeEvent(pa.getEventByID(db,index,anom)))



                        fail=True
        if(fail):
            print("Validation FAILED")
        else:
            print("Validation passed")

        del db
        engine.finalize()
