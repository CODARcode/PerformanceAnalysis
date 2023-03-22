#A Python module for offline analysis of the provenance database
#Executed as a script it performs some rudimentary analysis
import provdb_python.provdb_interact as pdb
import pymargo
from pymargo.core import Engine
import json
import sys
import copy
from cmd import Cmd

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
    
    #print("Filtering with function: %s" % filter_func)
    
    results = []
    for s in range(interface.getNshards()):
        results += [ json.loads(x) for x in interface.getShard(s).filter(collection_name, filter_func) ]
    return results

def filterGlobalDatabase(interface, collection_name, filter_list):
    filter_func = genFilterFunc(filter_list)
    
    #print("Filtering with function: %s" % filter_func)

    return [ json.loads(x) for x in interface.getGlobalDB().filter(collection_name, filter_func) ]


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


#Interactive component for detailed analysis
class InteractiveAnalysis(Cmd):
    prompt = '> '
    intro = "Type ? to list commands"

    def __init__(self, db):
        Cmd.__init__(self)
        self.fstats = getFuncStatsMap(db)
        self.db = db

        #Generate the table
        self.table = []
        for f in self.fstats.values():
            entry = {}
            entry['fname'] = f['fname']
            entry['fid'] = f['fid']
            entry['pid'] = f['app']
            entry['anom_count'] = f['anomaly_metrics']['anomaly_count']['accumulate']
            entry['accum_sev'] = f['anomaly_metrics']['severity']['accumulate']
            entry['exec_count'] = f['runtime_profile']['exclusive_runtime']['count']
            entry['avg_sev'] = f['anomaly_metrics']['severity']['mean']
            entry['anom_freq'] = float(entry['anom_count'])/entry['exec_count']
            entry['tot_runtime_excl'] = float(f['runtime_profile']['exclusive_runtime']['accumulate'])/1e6 #s
            entry['tot_runtime_incl'] = float(f['runtime_profile']['inclusive_runtime']['accumulate'])/1e6
            entry['avg_runtime_excl'] = float(f['runtime_profile']['exclusive_runtime']['mean'])/1e3 #ms
            entry['avg_runtime_incl'] = float(f['runtime_profile']['inclusive_runtime']['mean'])/1e3
            self.table.append(entry)

        #The columns we will show (can be user-manipulated)
        self.col_show = {'pid','fid','fname','accum_sev','anom_count'}
        #The list of all columns in the order that they will be displayed (assuming they are enabled by the show list)
        self.all_cols = ['pid','fid','accum_sev','avg_sev','anom_count','exec_count','anom_freq','avg_runtime_excl','tot_runtime_excl','avg_runtime_incl','tot_runtime_incl','fname']
        #Headers for the columns above
        self.all_cols_headers = ['PID','FID','Acc.Sev','Avg.Sev','Anom.Count','Exec.Count','Anom.Freq','Avg.Excl.Runtime (ms)','Total.Excl.Runtime (s)','Avg.Incl.Runtime (ms)','Total.Incl.Runtime (s)', 'Name']

        #Default sort order, can be changed by 'order'
        self.sort_order = 'Descending'
        #Default sorted-by, will be overridden when the user calls 'sort'
        self.sort_by = None

        #Number of entries to display (-1 for unbounded)
        self.top = 10

    def do_lscol(self,ignored):
        '''Display the valid column tags and a description'''
        print("""pid: The program index
fid: The function index (not guaranteed to be the same for different runs)
accum_sev: The accumulated severity of anomalies
avg_sev: The average severity of anomalies
anom_count: The number of anomalies
exec_count: The number of times the function was executed
anom_freq: The frequency of anomalies
avg_runtime_excl : The average running time of the function in milliseconds, excluding child function calls
tot_runtime_excl : The total running time of the function in seconds over all executions, excluding child function calls
avg_runtime_incl : The average running time of the function in milliseconds, including child function calls
tot_runtime_incl : The total running time of the function in seconds over all executions, including child function calls
fname: The function name""")        
        
    def do_addcol(self,coltag):
        '''Add a column to the output table by column tag. For allowed tags, call \'lscol\'. If called without an argument it will print the current set.'''
        if coltag == '':
            pass
        elif coltag not in self.all_cols:
            print("Invalid tag")
        else:            
            self.col_show.add(coltag)
        print(self.col_show)

    def do_rmcol(self,coltag):
        '''Remove a column to the output table by column tag. For allowed tags, call \'lscol\''''
        if coltag not in self.all_cols:
            print("Invalid tag")
        else:
            self.col_show.remove(coltag)
        print(self.col_show)

    def do_top(self,val):
        '''Set the number of entries to display. -1 is unbounded. Default 10. If called without an argument it will print the current value.'''
        if val == '':
            pass
        else:
            top = None
            try:
                top = int(val)
            except:
                print("Expect an integer")

            if top < -1:
                print("Value must be >= -1")
            else:
                self.top = top

        print(self.top)

    def do_order(self, order):
        '''Set the sort order. Allowed values are 'Ascending', 'Descending' (default). If called without an argument it will print the current order.'''
        if order == '':
            pass
        elif order != "Ascending" and order != "Descending":
            print("Invalid sort order")
        else:
            self.sort_order = order
        print(self.sort_order)
        
    def emptyline(self):
        return
        
    def do_exit(self, inp):
        '''Exit the application.'''
        print("Exiting")
        return True


    def do_show(self, ignored):
        '''Show the current table'''
        hdr = []
        tab = []

        #Generate headers, highlight sorted-by in bold
        for c in range(len(self.all_cols)):
            tag = self.all_cols[c]
            if tag in self.col_show:
                h = self.all_cols_headers[c]
                if tag == self.sort_by:
                    h = '((' + h + '))'
                hdr.append(h)
        #Generate the table
        top = self.top
        if top == -1:
            top = len(self.table)
        top = min(top,len(self.table))
            
        for i in range(top):
            v = self.table[i]
            row = []
            for tag in self.all_cols:
                if tag in self.col_show:
                    sv = str(v[tag])                            
                    if tag == 'avg_sev':
                        sv = "%.1f" % v[tag]
                    elif tag == 'anom_freq':
                        sv = "%.3f" % v[tag]
                        
                    row.append( sv )
                    
            tab.append(row)
        printTable(hdr,tab)
    
    def do_sort(self, sort_by):
        '''Sort the global database func_stats by the column tag provided and output the result.
        If no tag is provided, the tag used for the last sort will be reused. If no sort has been performed previously, the default will be used.
        For allowed tags, call \'lscol\''''

        if(sort_by == ''):
            sort_by = self.sort_by

        if sort_by not in self.all_cols:
            print("Invalid tag")
            return

        #Add the column to the show list if not already
        if sort_by not in self.col_show:
            self.do_addcol(sort_by)
        
        reverse=None
        if self.sort_order == 'Ascending':
            reverse = False
        elif self.sort_order == 'Descending':
            reverse = True
        else:
            raise Exception("Invalid 'order' argument: '%s'" % self.sort_order)

        self.table.sort(key=lambda v: v[sort_by], reverse=reverse)
        self.sort_by = sort_by
        self.do_show('')

            

def provdb_basic_analysis(args):
    # if(len(args) != 1):
    #     print("Arguments: <nshards>")
    #     sys.exit(0)
    # nshards = int(args[0])
    nshards = 0 #currently need only to connect to the global database
    
    with Engine('na+sm', pymargo.server) as engine:
        db = pdb.provDBinterface(engine, r'provdb.%d.unqlite', nshards)
        if db.getGlobalDB() != None:
            ian = InteractiveAnalysis(db)
            ian.cmdloop()
            del ian
        else:
            print("Could not connect to global database")
            
        del db
        engine.finalize()
