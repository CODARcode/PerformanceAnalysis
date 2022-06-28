
#A python module for interacting with the provenance database
import sys
import json
import re
import os

import pymargo
from pymargo.core import Engine

from pysonata.provider import SonataProvider
from pysonata.client import SonataClient
from pysonata.admin import SonataAdmin

#Base class for a Sonata client connection to a database
class provDBclientBase:
    def _openCollection(self, coll_name, create=False):
        if(self.database.exists(coll_name) == False):
            if(create == True):
                print("Shard %s creating '%s' collection" % (self.db_name, coll_name) )
                self.database.create(coll_name)
            else:
                print("Error: Shard %s, collection '%s' does not exist!" % (self.db_name, coll_name))
                sys.exit(1)
        return self.database.open(coll_name)

    #Apply a jx9 filter to a collection
    def filter(self, which_coll, query):
        return self.getCollection(which_coll).filter(query)

    def execute(self, code, variables):
        return self.database.execute(code,variables)

    def store(self, which_coll, record):
        return self.getCollection(which_coll).store(record, commit=True)

    def size(self, which_coll):
        col = self.getCollection(which_coll)
        try:
            return col.size
        except:
            return 0


    #Fetch a record with a specific index
    #idx can be integer or an array of integers
    def fetch(self, which_coll, idx):
        if isinstance(idx, int):
            entry = self.getCollection(which_coll).fetch(idx)
            return json.loads(entry)
        elif isinstance(idx, list):
            return [ json.loads(x) for x in self.getCollection(which_coll).fetch_multi(idx) ]
        else:
            print("Error: Index must be integer or list")
            sys.exit(1)


#Sonata client connection to a database shard
class provDBshard(provDBclientBase):
    def __init__(self,client,address,db_name,create=False):
        #Open database as a client
        self.db_name = db_name
        self.database = client.open(address, 0, db_name)

        #Initialize collections
        self.anomalies = self._openCollection('anomalies',create=create)
        self.normalexecs = self._openCollection('normalexecs',create=create)
        self.metadata = self._openCollection('metadata',create=create)

    def __del__(self):
        del self.anomalies
        del self.normalexecs
        del self.metadata        
        del self.database

    def getCollection(self, which_coll):
        col = None
        if which_coll == "anomalies":
            col = self.anomalies
        elif which_coll == "normalexecs":
            col = self.normalexecs
        elif which_coll == "metadata":
            col = self.metadata
        else:
            print("Invalid collection")
            sys.exit(1)
        return col
        
    def listCollections(self):
        return ['anomalies','normalexecs','metadata']


class provDBglobal(provDBclientBase):
    def __init__(self,client,address,db_name,create=False):
        #Open database as a client
        self.db_name = db_name
        self.database = client.open(address, 0, db_name)

        #Initialize collections
        self.func_stats = self._openCollection('func_stats',create=create)
        self.counter_stats = self._openCollection('counter_stats',create=create)
        self.ad_model = self._openCollection('ad_model',create=create)

    def __del__(self):
        del self.func_stats
        del self.counter_stats

    def getCollection(self, which_coll):
        col = None
        if which_coll == "func_stats":
            col = self.func_stats
        elif which_coll == "counter_stats":
            col = self.counter_stats
        elif which_coll == "ad_model":
            col = self.ad_model
        else:
            print("Invalid collection")
            sys.exit(1)
        return col
        
    def listCollections(self):
        return ['func_stats','counter_stats','ad_model']

        

class provDBinterface:
    #Filename should contain a '%d' which will be replaced by the shard index
    #If create=True it will create the database if it does not exist
    def __init__(self,engine,filename,nshards,create=False):
        if not re.search(r'\%d',filename):
            print("Error, filename does not contain \%d")
            sys.exit(1)

        #Setup provider admin and client to manage database
        self.provider = SonataProvider(engine, 0)
        self.address = str(engine.addr())
        self.admin = SonataAdmin(engine)
        self.client = SonataClient(engine)

        self.db_names = []
        self.db_shards = []
        for s in range(nshards):
            db_name = "provdb.%d" % s
            self.db_names.append(db_name)
           
            db_file = re.sub(r'\%d',str(s),filename)
            if os.path.exists(db_file) == False:
                if create == True:
                    print("Creating shard %d as %s from file %s" % (s,db_name,db_file))
                    self.admin.create_database(self.address, 0, db_name, 'unqlite', "{ \"path\" : \"%s\" }" % db_file)
                else:
                    print("Error: File '%s' does not exist!" % db_file)
                    sys.exit(1)                    
            else:
                print("Attaching shard %d as %s from file %s" % (s,db_name,db_file))
                self.admin.attach_database(self.address, 0, db_name, 'unqlite', "{ \"path\" : \"%s\" }" % db_file)

            self.db_shards.append( provDBshard(self.client, self.address, db_name, create=create) )

        #Connect to global database if available
        db_name = "provdb.global"
        self.db_global_name = db_name
        db_file = re.sub(r'\%d',"global",filename)
        if os.path.exists(db_file) == False:
            if create == True:
                print("Creating global database as %s from file %s" % (db_name,db_file))
                self.admin.create_database(self.address, 0, db_name, 'unqlite', "{ \"path\" : \"%s\" }" % db_file)
                self.use_global_db = True                                
            else:
                self.use_global_db = False
        else:
            print("Attaching global database as %s from file %s" % (db_name,db_file))
            self.admin.attach_database(self.address, 0, db_name, 'unqlite', "{ \"path\" : \"%s\" }" % db_file)
            self.use_global_db = True

        if self.use_global_db:
            self.db_global = provDBglobal(self.client, self.address, db_name, create=create)
        else:
            self.db_global = None
            
    def getNshards(self):
        return len(self.db_shards)

    def getShard(self, i):
        return self.db_shards[i]

    def getShards(self):
        return self.db_shards

    #Get the global database client. Returns None if the global database doesn't exit
    def getGlobalDB(self):
        return self.db_global
    
    def __del__(self):
        del self.db_shards        
        for n in self.db_names:
            self.admin.detach_database(self.address, 0, n)
            
        if self.use_global_db:
            del self.db_global
            self.admin.detach_database(self.address, 0, self.db_global_name)

        del self.client
        del self.admin
        del self.provider


        

if __name__ == '__main__':
    argc = len(sys.argv)
    if(argc != 4):
        print("""Basic script usage is to apply a jx9 filter to the database
--------------------------------------------------------
Usage python3.7 provdb_interact.py $COLLECTION $QUERY $NSHARDS
--------------------------------------------------------
Where COLLECTION is 'anomalies', 'normalexecs' or 'metadata'
and QUERY is a jx9 query
e.g. "function(\$entry) { return true; }"   will show all entries
        """)
        sys.exit(0)
    
    col = sys.argv[1]
    query = sys.argv[2]
    nshards = int(sys.argv[3])

    with Engine('na+sm', pymargo.server) as engine:
        db = provDBinterface(engine, 'provdb.%d.unqlite', nshards)

        filtered_records = []
        for i in range(nshards):
            filtered_records += [ json.loads(x) for x in db.getShard(i).filter(col,query) ]

        print(json.dumps(filtered_records,indent=4))

        del db
        engine.finalize()
