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

#Interactive component for detailed analysis
class InteractiveAnalysis(Cmd):
    prompt = '> '
    intro = "Type ? to list commands"

    def __init__(self, db):
        Cmd.__init__(self)
        self.cstats = db.getGlobalDB().all('counter_stats')
        self.db = db

        #Generate the table
        self.table = []
        for f in self.cstats:
            entry = {}
            entry['name'] = f['counter']
            entry['pid'] = f['app']
            entry['count'] = f['stats']['count']
            entry['min'] = f['stats']['minimum']
            entry['max'] = f['stats']['maximum']
            entry['avg'] = f['stats']['mean']
            self.table.append(entry)

        #The columns we will show (can be user-manipulated)
        self.col_show = {'pid','count','avg','name'}
        #The list of all columns in the order that they will be displayed (assuming they are enabled by the show list)
        self.all_cols = ['pid','count','min','max','avg','name']
        #Headers for the columns above
        self.all_cols_headers = ['PID','Count','Min.Val', 'Max.Val', 'Avg.Val', 'Name']

        #Default sort order, can be changed by 'order'
        self.sort_order = 'Descending'
        #Default sorted-by, will be overridden when the user calls 'sort'
        self.sort_by = None

        #Number of entries to display (-1 for unbounded)
        self.top = 10

    def do_lscol(self,ignored):
        '''Display the valid column tags and a description'''
        print("""pid: The program index
count: The total number of times the counter appeared
min: The minimum counter value
max: The maximum counter value        
avg: The average counter value
name: The counter name        
""")        
        
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
                    row.append( sv )
                    
            tab.append(row)
        pa.printTable(hdr,tab)
    
    def do_sort(self, sort_by):
        '''Sort the global database func_stats by the column tag provided and output the result.
        If no tag is provided, the tag used for the last sort will be reused. If no sort has been performed previously, the default will be used.
        For allowed tags, call \'lscol\''''

        if(sort_by == ''):
            sort_by = self.sort_by

        if sort_by not in self.all_cols:
            print("Invalid tag")
            return
        
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

            

def provdb_counter_analysis(args):
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
