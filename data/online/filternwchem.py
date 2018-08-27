"""
Filter NWChem data and generate anomaly detection data 
Authors: Shinjae Yoo (sjyoo@bnl.gov), Gyorgy Matyasfalvi (gmatyasfalvi@bnl.gov)
Create: August, 2018
"""

import sys
import time
import os
import pickle
from collections import defaultdict
import operator
import configparser
import parser
import event


# Initialize parser and event classes
prs = parser.Parser("gen.cfg")
evn = event.Event(prs.getFunMap(), "gen.cfg")

# Initialize filter method
config = configparser.ConfigParser()
config.read("gen.cfg")
thr = int(config['Filter']['Threshold'])
mcFun = []
numMcFun = []

# Stream events
ctFun = defaultdict(int)
dataOK = True
ctrl = 1
outct = 0
cuminct = 0
while ctrl >= 0:
    print("\noutct = ", outct, "\n\n")
    funStream = prs.getFunData() # Stream function call data
    inct = 0
    for i in funStream:  
        if evn.addFun(i): # Store function call data in event object
            inct = inct + 1
            ctFun[i[4]] += 1
        else:
            dataOK = False
            break
    cuminct = cuminct + inct    
    outct = outct + 1
    if dataOK:
        pass
    else:
        print("\n\n\nCall stack violation at ", outct, " ", cuminct, "... \n\n\n")
        break
    prs.getStream() # Advance stream
    ctrl = prs.getStatus() # Check stream status

print("Total number of advance operations: ", outct)
print("Total number of events: ", cuminct, "\n\n")
ctFun = sorted(ctFun.items(), key=operator.itemgetter(1), reverse=True)
# Debug
#print("ctFun = ", ctFun)

count = 0
for i in ctFun:
    if count >= thr:
        break
    mcFun.append(i[0])
    numMcFun.append(i[1])
    count = count + 1

# Debug
print("\n\nmcFun = ", mcFun)
print("\n\nnumMcFun = ", numMcFun)
# Get dictionary of lists [program id,  mpi rank, thread id, function id, entry timestamp, execution time] from event object
funData = evn.getFunExecTime()
#Debug
#print("Function execution time: \n", funData)

mcFunData = {k: funData[k] for k in mcFun}
#Debug
#print("\n\nMost Common Function execution time: \n", mcFunData)

# Store data (serialize)
with open('funtime.pickle', 'wb') as handle:
    pickle.dump(mcFunData, handle, protocol=pickle.HIGHEST_PROTOCOL)

# Load data (deserialize)
with open('funtime.pickle', 'rb') as handle:
    usMcFunData = pickle.load(handle)

# Validate data serialization
if mcFunData == usMcFunData:
    print("\nPickle serialization successful...\n\n")
else:
    raise Exception("\nPickle serialization unsuccessful...\n\n")

