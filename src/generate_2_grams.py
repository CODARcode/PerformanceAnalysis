# coding: utf-8
# Date: Monday, September 18, 2017
# Author: Alok Singh
# Description: This function generates 2_grams by reason a stack trace from JSON file
# Entries in JSON must conform to in-order traversal of call-sequence graph

from pprint import pprint

def generate_2_grams(file_name = 'trace_entry_exit_0.json'):
    #
    import json
    from collections import deque
    print('Reading file %s' % file_name)
   
    two_grams = []
    d = deque()
    
    with open('trace_entry_exit_0.json') as data_file:
        data = json.load(data_file)

    for i in range(0,len(data)):
        if(data[i]['event-type'] == 'entry'):
            d.append(data[i])
        else: #exit
            d.pop() #pop the entry of this node
            if(len(d)>0):
                # add a 2-gram: (parent, this_child)
                two_grams.append([ d[-1], data[i]])
                
    print('Total 2-grams (one less than # of  nodes) = %d' % len(two_grams))
    print('Total nodes = %d' % (len(data)/2))
    return two_grams

pairs = generate_2_grams('trace_entry_exit_0.json')
pprint(len(pairs))