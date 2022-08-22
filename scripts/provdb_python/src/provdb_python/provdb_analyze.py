#A Python module for offline analysis of the provenance database
#Executed as a script it performs some rudimentary analysis
import provdb_python.provdb_interact as pdb
import pymargo
from pymargo.core import Engine
import json
import sys
import copy

#Terminology:
#The database comprises multiple *shards* each of which contains a number of records (*events*)
#Each *event* contains a number of *keys* (record fields) that can be used to index the records
#Every event has the same record schema

#For speed purposes we first generate an *index* which maps a set of keys to a list of corresponding *record ids*


#Get the list of valid keys (record fields) in a given collection
#Valid keys are string or numeric
def getKeys(interface, collection):
    if(len(interface.getShards()) == 0):
        return None
    pvars = {'keys'}
    pcode = """$keys = [];
               $member = db_fetch('%s');
               if($member != NULL){
                  foreach ( $member as $key , $value){
                    if(is_numeric($value) || is_string($value)){
                      array_push($keys, $key);
                    }
                  }
               }""" % collection
    ret = json.loads(interface.getShard(0).execute(pcode,pvars)['keys'])
    return ret

#Generate mappings between keys and a time-ordered list of record ids
#Record ids are tuples: (db_idx, r_idx, timestamp)   where bd_idx is the shard index, r_idx the record within the shard, and timestamp the entry timestamp
#Keys should be a list
def generateIndex(interface, keys, collection):
    if len(keys) == 0:
        return None

    results = {}
    results["__collection"] = collection
    for k in keys:
        results[k] = {}

    keys_str = ','.join(keys)
    pvars = {'results'}
    pcode = """$results = {};
              $keys = [%s];
              foreach($keys as $key){              
                $results[$key] = {};
              }  

              while(($member = db_fetch('%s')) != NULL){
                    $id = $member.__id;
                    $entry = $member.entry;
                    foreach($keys as $key){
                        $val = $member[$key];
                        if( !array_key_exists($val, $results[$key]) ){
                           $results[$key][$val] = [];
                        }
                        array_push($results[$key][$val], [$id,$entry]);
                    }
               }""" % (keys_str,collection)

    shard=0
    for database in interface.getShards():
        ret = json.loads(database.execute(pcode,pvars)['results'])
        for k in keys:
            ret_k = ret[k]   #returned vals with given key
            into_k = results[k]    #output for this key
            for v in ret_k.keys():   #value
                if v not in into_k:
                    into_k[v] = []
                for idx_entry in ret_k[v]:
                    idx = idx_entry[0]
                    entry = idx_entry[1]
                    into_k[v].append( (shard,idx,entry) )    
                    
        shard+=1

    #Sort by entry timestamp
    for k in keys:
        for v in results[k].keys():
            results[k][v].sort(key=lambda x: x[2])


    return results

#Write the index to disk
def writeIndex(filename, index):
    f = open(filename,"w")
    json.dump(index, f)
    f.close()

#Read the index from disk
def readIndex(filename):
    f = open(filename,"r")
    index = json.load(f)
    f.close()
    return index

#Get an event using the index tuple obtained from the provided index
def getEventByID(interface, index, idx_tuple):
    collection = index['__collection']
    shard = idx_tuple[0]
    rid = idx_tuple[1]

    pvars = {'entry'}
    pcode = """$entry = db_fetch_by_id('%s', %d);""" % (collection, rid)
    ret = interface.getShard(shard).execute(pcode,pvars)
    return json.loads(ret['entry'])

#Return a list of values for a given key
def getValuesUsingIndex(index, key):
    if key not in index:
        return None
    else:
        return [ v for v in index[key].keys() ]

#Return a list of record ids for specific key and value
def getRecordIDsByKeyValue(index, key, value):
    return index[key][value]

#Get a list of records for specific key and value
#By default these are sorted by their entry timestamp (in ascending order) but a different key can be provided
def getRecordsByKeyValue(interface, index, key, value, sort_key = 'entry'):
    lst = [ getEventByID(interface, index, x) for x in getRecordIDsByKeyValue(index,key,value) ]
    if sort_key != 'entry':
        lst.sort(key=lambda x: x[sort_key])
    return lst

#Generate a string containing a summary of an event:  (process, rank, thread, function name, step, exclusive runtime, inclusive runtime, outlier score, outlier severity)
#If the event was a GPU event, the thread index will be replaced by GPU${DEVICE}/${CONTEXT}/${STREAM}
def summarizeEvent(event):
    thr_str = "{}".format(event['tid'])
    if event['is_gpu_event']:
        thr_str = "GPU{}/{}/{}".format(event['gpu_location']['device'],event['gpu_location']['context'],event['gpu_location']['stream'])    
    return "pid={} rid={} tid={} func=\"{}\" step={} excl={}s tot={}s score={} severity={}".format(event['pid'],event['rid'],  thr_str, event['func'], event['io_step'], float(event['runtime_exclusive'])/1e6, float(event['runtime_total'])/1e6, event['outlier_score'], event['outlier_severity'])
                                             

#Get the function profile information for application index 'app'
#Returns a dictionary indexed by the function name with entries
#    'excl_time_tot' : the total exclusive time (in seconds) spent in the function over all ranks and threads
#    'count' : the number of times the function was encountered over all ranks and threads
#    'excl_time_mean' : the average exclusive time (in seconds) spent in the function over all occurences
#    'frac_time' : the amount of time spent in the function over all occurences relative to the total runtime spent in all threads and ranks
def getFunctionProfile(interface, app):
        profile = interface.getGlobalDB().filter('func_stats', "function($f){return $f[\"app\"] == %d;}" % app)
        total_time = 0 #sum of exclusive times is total time
        fprofile = {}
        for f in profile:
            #Don't currently sum times but can reconstruct from count and mean
            j = json.loads(f)
            count = int(j['exclusive']['count'])
            excl_mean = float(j['exclusive']['mean']) / 1e6   #seconds
            excl_tot = count * excl_mean
            fname = j['name']
            fprofile[fname] = {}
            fprofile[fname]['excl_time_tot'] = excl_tot
            fprofile[fname]['count'] = count
            fprofile[fname]['excl_time_mean'] = excl_mean
            total_time += excl_tot


        for f in fprofile.keys():
            fprofile[f]['frac_time'] = fprofile[f]['excl_time_tot']/total_time

        return fprofile

#Print summary information for each function sorted by the total exclusive runtime spent in the function
def summarizeProfile(fprofile):
    fordered = sorted(fprofile.keys(), key=lambda x: fprofile[x]['excl_time_tot'], reverse=True)
    for f in fordered:
        finfo = fprofile[f]
        print("(name='%s') (total excl. time=%es) (count=%d) (avg. excl. time=%es) (runtime fraction=%e)" % (f,finfo['excl_time_tot'],finfo['count'],finfo['excl_time_mean'],finfo['frac_time']) )


def getKeys(interface, collection):
    if(len(interface.getShards()) == 0):
        return None
    pvars = {'keys'}
    pcode = """$keys = [];
               $member = db_fetch('%s');
               if($member != NULL){
                  foreach ( $member as $key , $value){
                    if(is_numeric($value) || is_string($value)){
                      array_push($keys, $key);
                    }
                  }
               }""" % collection
    ret = json.loads(interface.getShard(0).execute(pcode,pvars)['keys'])
    return ret

#Internal: Generate a jx9 filter function using the provided formatted list
#Filters have a tuple form:  (key, comparison operator, value)
#  comparison operator can be ==, !=, <, >
#  special operator: "contains" for substring filtering
#Logical operations between filters are input as strings, support "(", ")", "&&", "||"
#Examples:
#    anom = anl.filterDatabase(pdb, 'anomalies', [ ('__id','==',0) ]) #__id == 0
#
#    anom = anl.filterDatabase(pdb, 'anomalies', [ ('exit','==',1648661837676928), "&&", ("fid","==",490) ])
#
#    anom = anl.filterDatabase(pdb, 'anomalies', [ '(' , ('fid','==', 490), "||", ("fid","==",491), ')' , '&&' , ("rid","==",0) ])
#    
#    anom = anl.filterDatabase(pdb, 'anomalies', [ ("func","contains","Kokkos") ])
def genFilterFunc(filter_list):
    if(len(filter_list) == 0):
        return None

    filter_func = "function($f){return "
    for f in filter_list:
        if(isinstance(f,str)):
            filter_func += " " + f + " "
        elif(isinstance(f,tuple)):
            if(len(f) != 3):
                print("Expect filter to be a tuple containing a key, comparison operator and value")
                sys.exit(1)
            key = f[0]

            v = f[2]        
            if(isinstance(v,str)):
                v = "\"%s\"" % v

            op = f[1]
            if(op == "contains"):
                #Special function: string contains
                filter_func += "substr_count($f[\"%s\"], %s) > 0" % (key,v)
            else:
                filter_func += "$f[\"%s\"] %s %s" % (key,op,v)

    filter_func += "; }"
    return filter_func

#Internal: For a specific collection in the given database, return the list of keys available in any given entry
def _listKeys(db,collection_name):
    j = json.loads(db.getCollection(collection_name).fetch(0))
    return j.keys()

#For a specific collection in the main database, return the list of keys available in any given entry
def listKeys(interface, collection_name):
    if(interface.getNshards() == 0):
        return []
    return _listKeys(interface.getShard(0),collection_name)

#For a specific collection in the global database, return the list of keys available in any given entry
def listGlobalKeys(interface, collection_name):
    return _listKeys(interface.getGlobalDB(),collection_name)
    

#Filter the database against a list of filters
#filter_list :  a list of filters and logical operations    
#Filters have a tuple form:  (key, comparison operator, value)
#  comparison operator can be ==, !=, <, >
#  special operator: "contains" for substring filtering
#Logical operations between filters are input as strings, support "(", ")", "&&", "||"
#Examples:
#    anom = anl.filterDatabase(pdb, 'anomalies', [ ('__id','==',0) ]) #__id == 0
#
#    anom = anl.filterDatabase(pdb, 'anomalies', [ ('exit','==',1648661837676928), "&&", ("fid","==",490) ])
#
#    anom = anl.filterDatabase(pdb, 'anomalies', [ '(' , ('fid','==', 490), "||", ("fid","==",491), ')' , '&&' , ("rid","==",0) ])
#    
#    anom = anl.filterDatabase(pdb, 'anomalies', [ ("func","contains","Kokkos") ])
def filterDatabase(interface, collection_name, filter_list):
    if(interface.getNshards() == 0):
        return None

    filter_func = genFilterFunc(filter_list)
    
    print("Filtering with function: %s" % filter_func)

    results = []
    for s in range(interface.getNshards()):
        results += [ json.loads(x) for x in interface.getShard(s).filter(collection_name, filter_func) ]
    return results

def filterGlobalDatabase(interface, collection_name, filter_list):
    filter_func = genFilterFunc(filter_list)
    
    print("Filtering with function: %s" % filter_func)

    return [ json.loads(x) for x in interface.getGlobalDB().filter(collection_name, filter_func) ]


    
    
def provdb_basic_analysis(args):
    if(len(args) != 1):
        print("Arguments: <nshards>")
        sys.exit(0)
    nshards = int(args[0])

    with Engine('na+sm', pymargo.server) as engine:
        db = pdb.provDBinterface(engine, r'provdb.%d.unqlite', nshards)

        print("Generating index")
        anom_index = generateIndex(db, ['func','rid'], 'anomalies')
        print("Getting function names")
        func_names = getValuesUsingIndex(anom_index, 'func')

        #Sort functions by total anomalous event time (sum of anomaly times)
        print("Computing total anomalous event time")
        anom_times = {}
        for func in func_names:
            events =  getRecordsByKeyValue(db, anom_index, 'func', func)
            time = 0
            for e in events:
                time += e['runtime_exclusive']                
            anom_times[func] = time
        
        func_names.sort(key=lambda x: anom_times[x], reverse=True)
        print("Functions ordered by total anomalous event time (name #anomalies time):")
        for func in func_names:
            print("{} {} {}s".format(func, len(getRecordIDsByKeyValue(anom_index, 'func', func)), float(anom_times[func])/1e6 ))
                

        print("Summary of top 10 events for each anomalous function with <100 anomalies")
        for func in func_names:
            nanom = len(getRecordIDsByKeyValue(anom_index, 'func', func))
            if nanom < 100:
                print("%s (%d anomalies):" % (func, nanom  )    )
                events = getRecordsByKeyValue(db, anom_index, 'func', func)
                events.sort(key=lambda x: x['runtime_exclusive'], reverse=True)
                n = 0
                for e in events:
                    print(summarizeEvent(e))
                    n+=1
                    if n > 10:
                        break


        del db
        engine.finalize()
