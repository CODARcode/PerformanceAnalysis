"""@package Perfanal
This is the main driver script for the anlysis code 
Author(s): 
    Gyorgy Matyasfalvi (gmatyasfalvi@bnl.gov)
Created: 
    August, 2018

    pydoc -w perfanal

"""

import sys
import os
import time
from datetime import datetime, timezone
import configparser
import logging
import json
import pickle
import numpy as np
np.set_printoptions(threshold=np.nan)
from collections import defaultdict
import parser
import event
import outlier
import visualizer


#MGY
#import pprint
#import matplotlib.pyplot as plt
#from sklearn.preprocessing import MinMaxScaler

start = time.time()

# Proces config file and set up logger
config = configparser.ConfigParser(interpolation=None)
config.read(sys.argv[1])
# Logger init
logFile = config['Debug']['LogFile']
logFormat = config['Debug']['Format']
logLevel = config['Debug']['LogLevel']
logging.basicConfig(level=logLevel, format=logFormat, filename=logFile)
log = logging.getLogger('PARSER')
msg = "\n\n\n\nLog run -- " + datetime.now(timezone.utc).strftime("%c%Z") + "\n"
log.info(msg)
# Dump config file contents
configContents = {}
for sectionName in config.sections():
    configContents[sectionName] = config.items(sectionName)
msg = "Run's config file: \n" + json.dumps(configContents, indent=2) + "\n"
log.info(msg)

# Set stopLoop allowing for early termination
stopLoop = int(config['Debug']['StopLoop'])

# Initialize parser object, get function id function name map 
prs = parser.Parser(sys.argv[1])
parseMethod = prs.getParseMethod()


if parseMethod == "BP":
    # Determine event types
    eventTypeDict = prs.getEventType()
    numEventTypes = len(eventTypeDict)
    eventTypeList = [None] * numEventTypes 
    assert(numEventTypes > 0), "No event types detected (Assertion)...\n"
    for i in range(0,numEventTypes):
        eventTypeList[i] = eventTypeDict[i]
    
    # Initialize event class
    evn = event.Event(sys.argv[1])
    evn.setEventType(eventTypeList)
    
    # Initialize outlier class
    otl = outlier.Outlier(sys.argv[1])
    
    # Initialize visualizer class
    maxDepth = int(config['Visualizer']['MaxFunDepth'])
    viz = visualizer.Visualizer(sys.argv[1])
    
    # Reset visualization server
    viz.sendReset()
    
    # In nonstreaming mode send function map and event type to the visualization server 
    viz.sendEventType(eventTypeList, 0)
    funMap = prs.getFunMap()
    viz.sendFunMap(list(funMap.values()), 0) 
else:
    # Initialize event object
    evn = event.Event(sys.argv[1])

    # Initialize outlier object
    otl = outlier.Outlier(sys.argv[1])

    # Initialize visualizer object
    maxDepth = int(config['Visualizer']['MaxFunDepth'])
    viz = visualizer.Visualizer(sys.argv[1])
    viz.sendReset()

#MGY 
#outfile = "funMap.pickle"
#with open(outfile, 'wb') as handle:
#    pickle.dump(funMap, handle, protocol=pickle.HIGHEST_PROTOCOL)
#    handle.close()
#MGY

#MGY
#outfile = "eventType.pickle"
#with open(outfile, 'wb') as handle:
#    pickle.dump(eventTypeList, handle, protocol=pickle.HIGHEST_PROTOCOL)
#    handle.close()
#MGY


# Stream events
ctFun = defaultdict(int)
ctCount = defaultdict(int)
ctComm = defaultdict(int)
anomFun = defaultdict(int)
stackOK = True
funDataOK = False
funTimeOK = False
countDataOK = False
commDataOK = False

ctrl = 1
outct = 0
eventId = np.uint64(0)
numOutl = 0

#MGY
#fig = plt.figure()
#ax = fig.add_subplot(111)
#scaler = MinMaxScaler()
#funTrue = 0
#MGY

while ctrl >= 0:
    
    msg = "\n\nFrame: " + str(outct)
    print(msg)
    log.info(msg)
    
    if parseMethod != "BP":        
        # For each frame determine the event type
        eventTypeDict = prs.getEventType()
        numEventTypes = len(eventTypeDict)
        eventTypeList = [None] * numEventTypes 
        assert(numEventTypes > 0), "No event types detected (Assertion)..."
        for i in range(0,numEventTypes):
            eventTypeList[i] = eventTypeDict[i]
            
        # In event class set event types for each frame 
        evn.setEventType(eventTypeList)
        
        # In streaming mode send function map and event type to the visualization server for each frame
        viz.sendEventType(eventTypeList, outct)
        funMap = prs.getFunMap()
        viz.sendFunMap(list(funMap.values()), outct)
    

    # Stream function call data
    outlId = []
    funOfInt = []
    try:
        funStream = prs.getFunData()
        funDataOK = True
    except:
        funDataOK = False
        msg = "Frame has no function data..."
        print(msg)
        log.info(msg)

    if funDataOK:        
        evn.initFunData(funStream.shape[0])
                 
        for i in funStream:
            # Append eventId so we can backtrack which event in the trace was anomalous
            i = np.append(i,np.uint64(eventId))
            #print(i, "\n")
            if evn.addFun(i): # Store function call data in event object
                pass
            else:
                stackOK = False
                break        
            eventId += 1
            ctFun[i[4]] += 1
        if stackOK:
            pass
        else:
            msg = "\n\n\nCall stack violation at ", outct, " ", eventId, "... \n\n\n"
            print(msg)
            log.error(msg)
            break
 
        
        # Detect anomalies in function call data
        try:
            data = evn.getFunTime()
            funTimeOK = True
            msg = "Anomaly detection..."
            print(msg)
            log.info(msg)
        except:
            funTimeOK = False
            msg = "Frame only contains open functions, no anomaly detection..."
            print(msg)
            log.info(msg)
        
        numOutlFrame = 0
        if funTimeOK:    
            for funId in data: 
                X = np.array(data[funId])
                numPoints = X.shape[0]
                otl.compOutlier(X, funId)
                funOutl = np.array(otl.getOutlier())        
                funOutlId = X[funOutl == -1][:,6]
                numOutlFrame += len(funOutlId)
                for i in range(0,len(funOutlId)):
                    outlId.append(str(funOutlId[i]))
                    
                #MGY
                #===============================================================
                # if funId == 26:
                #     if funTrue == 0:
                #         points = X[:,4:6]
                #         outliers = funOutl
                #         avg = np.full((len(X),1),otl.getMean(funId))
                #         mx = np.amax(X[:,5])
                #     else:
                #         points = np.append(points, X[:,4:6], axis=0)
                #         outliers = np.append(outliers, funOutl, axis=0)
                #         avg = np.append(avg, np.full((len(X),1),otl.getMean(funId)))
                #         if mx < np.amax(X[:,5]):
                #             mx = np.amax(X[:,5])
                #     funTrue += 1
                #===============================================================
                
                # Filter functions that are deep
                maxFunDepth = evn.getMaxFunDepth()
                if numPoints > np.sum(funOutl) and maxFunDepth[funId] < maxDepth:
                    funOfInt.append(str(funMap[funId]))
                if len(funOutlId) > 0:
                    anomFun[funId] += 1
                    #if funId in foiid:
                    #    print("Function id: ", funId, " has anomaly\n")          
    
    numOutl += numOutlFrame
    msg = "Number of outliers per frame: " + str(numOutlFrame)
    print(msg)
    log.info(msg)
    # Stream counter data
    try:
        countStream = prs.getCountData()
        countDataOK = True
    except:
        countDataOK = False
        msg = "Frame has no counter data..."
        print(msg)
        log.info(msg)
        
    if countDataOK:
        evn.initCountData(countStream.shape[0])
        
        for i in countStream:
            i = np.append(i,np.uint64(eventId))
            if evn.addCount(i):
                pass
            else:
                stackOK = False
                break
            eventId += 1 
            ctCount[i[3]] += 1
       
     
     
    # Stream communication data
    try:
        commStream = prs.getCommData()
        commDataOK = True
    except:
        commDataOK = False
        msg = "Frame has no comm data..."
        print(msg)
        log.info(msg)
    
    if commDataOK:
        evn.initCommData(commStream.shape[0])

        for i in commStream:
            i = np.append(i,np.uint64(eventId))
            if evn.addComm(i):
                pass
            else:
                stackOK = False
                break
            eventId += 1 
            ctComm[i[3]] += 1
     

    # Dump trace data
    viz.sendCombinedData(evn.getFunData(), evn.getCountData(), evn.getCommData(), funOfInt, outlId, outct)
    
    
    # Free memory
    evn.clearFunTime()
    evn.clearFunData()
    evn.clearCountData()
    evn.clearCommData()
    outlId.clear()
    funOfInt.clear()
    if parseMethod != "BP":
        eventTypeList.clear()
    
    # Debug
    if stopLoop > -1:
        if outct >= stopLoop:
            break
    
    # Update outer loop counter   
    outct += 1 
    
    # Advance stream and check status
    prs.getStream()
    ctrl = prs.getStatus()
    msg = "Adios stream status: " + str(ctrl)
    print(msg)
    sys.stdout.flush()
    log.info(msg)
    

prs.adiosClose()
prs.adiosFinalize()

#MGY
#===============================================================================
# points = scaler.fit_transform(points)
# avg /= mx
# 
# 
# plt.xlabel('Scaled entry timestamp')
# plt.ylabel('Scaled function exec. time')
# ax.scatter(points[outliers == -1][:,0], points[outliers == -1][:,1] , alpha=0.8, c="red", edgecolors='none', s=30, label="outlier")
# ax.scatter(points[outliers == 1][:,0], points[outliers == 1][:,1] , alpha=0.8, c="w", edgecolors='green', s=30, label="regular") 
# ax.plot(points[:,0], avg, color='blue', linestyle='-', linewidth=2, label="mean")
# 
# 
# #ax.plot(points[outliers == 1][:,0], points[outliers == 1][:,1], color='blue', linestyle='--', linewidth=2, marker='s')
# plt.title('NWChem function: ga_get_()')
# plt.legend(loc=1)
# plt.savefig('mva.png', bbox_inches='tight')
#===============================================================================
#MGY

assert(evn.getFunStackSize() == 0), "Function stack not empty... Possible call stack violation..."
msg = "\n\nOutlier detection done..."
print(msg)
log.info(msg)
msg = "Total number of frames: " + str(outct)
print(msg)
log.info(msg)
msg = "Total number of events: " + str(eventId)
print(msg)
log.info(msg)
msg = "Total number of outliers: " + str(numOutl)
print(msg)
log.info(msg)
end = time.time()
rt = end - start
msg = "Running time: " + str(rt) + "\n\n"
print(msg)
sys.stdout.flush()
log.info(msg)

# Generate Candidate functions
#maxFunDepth = evn.getMaxFunDepth()
#for i in anomFun:
#    if maxFunDepth[i] < 15 and ctFun[i] > 100:
#        print("Candidate function: ",i, " (", funMap[i], ")", " max tree depth: ", maxFunDepth[i], " # fun calls: ", ctFun[i])
