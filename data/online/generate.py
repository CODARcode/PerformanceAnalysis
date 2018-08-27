"""
Generate anomaly detection data
Authors: Shinjae Yoo (sjyoo@bnl.gov), Gyorgy Matyasfalvi (gmatyasfalvi@bnl.gov)
Create: August, 2018
"""

import sys
import time
import os
import pickle
import parser
import event

# Initialize parser and event classes
prs = parser.Parser("gen.cfg")
evn = event.Event(prs.getFunMap(), "gen.cfg")

# Stream events
dataOK = True
ctrl = 1
outct = 0
cuminct = 0
while ctrl >= 0:
    funStream = prs.getFunData() # Stream function call data
    inct = 0
    for i in funStream:  
        if evn.addFun(i): # Store function call data in event object
            inct = inct + 1
        else:
            dataOK = False
            break
    cuminct = cuminct + inct    
    outct = outct + 1
    if dataOK:
        pass
    else:
        print("\n\n\nCall stack violation at ",outct," ",cuminct,"... \n\n\n")
        break
    prs.getStream() # Advance stream
    ctrl = prs.getStatus() # Check stream status

print("Total number of advance operations: ", outct)
print("Total number of events: ", cuminct, "\n\n")

# Get dictionary of lists [program id,  mpi rank, thread id, function id, entry timestamp, execution time] from event object
funData = evn.getFunExecTime()
#Debug
#print("Function execution time: \n", funData)

# Store data (serialize)
with open('funtime.pickle', 'wb') as handle:
    pickle.dump(funData, handle, protocol=pickle.HIGHEST_PROTOCOL)

# Load data (deserialize)
with open('funtime.pickle', 'rb') as handle:
    usFunData = pickle.load(handle)

# Validate data serialization
if funData == usFunData:
    print("Pickle serialization successful...\n\n")
else:
    raise Exception("Pickle serialization unsuccessful...\n\n")


