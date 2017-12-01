# coding: utf-8
# Date: Monday, September 18, 2017
# Authors: Alok Singh, Shinjae Yoo
# Description: This function generates 2_grams by reason a stack trace from JSON file
# Entries in JSON must conform to in-order traversal of call-sequence graph

import re 
from pprint import pprint
import json
from collections import deque
import pandas as pd 
import pickle
import sys

def refine_fn (func_name):
    return re.sub(r'\s?C?\s*\[.*\]\s*$', '', func_name).replace(" ", "")

# call stack
def generate_n_grams_ct(file_name = 'trace_entry_exit_0.json', k = 2):
    #
    #print('Reading file %s' % file_name)
   
    n_grams = []
    d = deque()
    
    list_list = []
    
    with open('trace_entry_exit_0.json') as data_file:
        data = json.load(data_file)

    last_time = float(data[-1]['time'])
    node_id   = data[1]['node-id']
    thread_id = data[1]['thread-id']

    for i in range(0,len(data)):
        if(i%1000 == 0):
            print(i)
        if(data[i]['event-type'] == 'entry'):
            d.append(data[i])
        else: #exit
            pdata = d.pop() #pop the entry of this node
            if(len(d)>0):
                kl = [ refine_fn(d[-nk]['name']) for nk in range(min(len(d), k), 1, -1)]
                kl.append(refine_fn(data[i]['name']))
                '''print("KG={0}#{3}#{4}#{1}#{2}".format(":".join(kl), 
                    float(pdata["time"])/last_time, 
                    float(data[i]["time"]) - float(pdata["time"]), 
                    node_id, 
                    thread_id))
                df.loc[len(df)] = [":".join(kl), 
                    float(pdata["time"])/last_time, 
                    float(data[i]["time"]) - float(pdata["time"]), 
                    node_id, 
                    thread_id]'''
                list_list.append([":".join(kl), 
                    float(pdata["time"])/last_time, 
                    float(data[i]["time"]) - float(pdata["time"]), 
                    node_id, 
                    thread_id])
                #n_grams.append(":".join(kl))

    print('len of data = %d', i)
    df = pd.DataFrame(list_list,columns = ['kl','time_by_lasttime','time_diff','node_id','thread_id'])
    with open(filename+'.df','wb') as handle:
        pickle.dump(df, handle, protocol=pickle.HIGHEST_PROTOCOL)

    return n_grams
# call seq.
def generate_n_grams_cs(file_name = 'trace_entry_exit_0.json', k = 2):
    #
    import json
    from collections import deque
    #print('Reading file %s' % file_name)
   
    n_grams = []
    d = deque()
    g = deque()
    
    with open('trace_entry_exit_0.json') as data_file:
        data = json.load(data_file)

    for i in range(0,len(data)):
        if(data[i]['event-type'] == 'entry'):
            d.append(refine_fn(data[i]['name']))
            g.append(data[i])
        else: # exit
            pdata = g.pop()
            if( len(d) >= k ) :
                print("KS={}#{}".format(":".join(list(d)),  float(data[i]["time"]) - float(pdata["time"])))
                d.popleft()
        
    return n_grams


if __name__ == "__main__":
    #print("__main__: calling default function")
    generate_n_grams_ct(sys.argv[1], sys.argv[2])
