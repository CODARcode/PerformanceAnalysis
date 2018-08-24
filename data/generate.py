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
ctrl = 1
outct = 0
cuminct = 0
while ctrl >= 0 and outct < 1:
    print("ctrl = ", ctrl)
    print("outct = ", outct)
    print("cuminct = ", cuminct)
    funStream = prs.getFunData()
    inct = 0
    for i in funStream:  
        evn.addFun(i)
        print("inct = ", inct)
    cuminct = cuminct + inct    
    outct = outct + 1
    prs.getStream()
    ctrl = prs.getStatus()


print("Function execution time: \n", evn.getFunExecTime())

funData = evn.getFunExecTime()

# Store data (serialize)
with open('funtime.pickle', 'wb') as handle:
    pickle.dump(funData, handle, protocol=pickle.HIGHEST_PROTOCOL)

# Load data (deserialize)
with open('funtime.pickle', 'rb') as handle:
    usFunData = pickle.load(handle)

print(funData == usFunData)


