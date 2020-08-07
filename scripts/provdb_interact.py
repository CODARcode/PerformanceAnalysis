import sys
import json

import pymargo
from pymargo.core import Engine

from pysonata.provider import SonataProvider
from pysonata.client import SonataClient
from pysonata.admin import SonataAdmin

class provDBinterface:
    def __init__(self,engine,filename):
        #Setup provider and admin to manage database
        self.provider = SonataProvider(engine, 0)
        self.address = str(engine.addr())
        self.admin = SonataAdmin(engine)
        self.admin.attach_database(self.address, 0, 'provdb', 'unqlite', "{ \"path\" : \"%s\" }" % filename)

        #Open database as a client
        self.client = SonataClient(engine)
        self.database = self.client.open(self.address, 0, 'provdb')

        #Initialize collections
        self.anomalies = self.database.open('anomalies')
        self.normalexecs = self.database.open('normalexecs')
        self.metadata = self.database.open('metadata')

    #Apply a jx9 filter to a collection
    def filter(self, which_coll, query):
        col = None
        if which_coll == "anomalies":
            col = self.anomalies
        elif which_col == "normalexecs":
            col = self.normalexecs
        elif which_col == "metadata":
            col = self.metadata
        else:
            print("Invalid collection")
            sys.exit(1)
        return col.filter(query)


    def __del__(self):
        self.admin.detach_database(self.address, 0, 'provdb')
        del self.provider
        

if __name__ == '__main__':
    argc = len(sys.argv)
    if(argc != 3):
        print("""Basic script usage is to apply a jx9 filter to the database
--------------------------------------------------------
Usage python3.7 provdb_interact.py $COLLECTION $QUERY
--------------------------------------------------------
Where COLLECTION is 'anomalies', 'normalexecs' or 'metadata'
and QUERY is a jx9 query
e.g. "function(\$entry) { return true; }"   will show all entries
        """)
        sys.exit(0)
    
    col = sys.argv[1]
    query = sys.argv[2]

    with Engine('na+sm', pymargo.server) as engine:
        db = provDBinterface(engine, 'provdb.unqlite')

        filtered_records = [ json.loads(x) for x in db.filter(col,query) ]
        print(filtered_records)

        #Ensure db is deleted before engine is finalized
        del db
        engine.finalize()
