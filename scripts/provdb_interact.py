
#A python module for interacting with the provenance database
import sys
import json
import re

import pymargo
from pymargo.core import Engine

from pysonata.provider import SonataProvider
from pysonata.client import SonataClient
from pysonata.admin import SonataAdmin

class provDBshard:
    def __init__(self,client,address,db_name):
        #Open database as a client
        self.database = client.open(address, 0, db_name)

        #Initialize collections
        self.anomalies = self.database.open('anomalies')
        self.normalexecs = self.database.open('normalexecs')
        self.metadata = self.database.open('metadata')

    def __del__(self):
        del self.anomalies
        del self.normalexecs
        del self.metadata        
        del self.database

        
    #Apply a jx9 filter to a collection
    def filter(self, which_coll, query):
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
        return col.filter(query)

    def execute(self, code, variables):
        return self.database.execute(code,variables)


class provDBinterface:
    #Filename should contain a '%d' which will be replaced by the shard index
    def __init__(self,engine,filename,nshards):
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
            print("Attaching shard %d as %s from file %s" % (s,db_name,db_file))
            self.admin.attach_database(self.address, 0, db_name, 'unqlite', "{ \"path\" : \"%s\" }" % db_file)

            self.db_shards.append( provDBshard(self.client, self.address, db_name) )

    def getNshards(self):
        return len(self.db_shards)

    def getShard(self, i):
        return self.db_shards[i]

    def getShards(self):
        return self.db_shards

    def __del__(self):
        del self.db_shards
        del self.client
        for n in self.db_names:
            self.admin.detach_database(self.address, 0, n)
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
