"""
Run anomaly detection and output data for visualization
Authors: Gyorgy Matyasfalvi (gmatyasfalvi@bnl.gov)
Create: September, 2018
"""

import sys
import time
import os
import configparser
import json
import pickle
import numpy as np
np.set_printoptions(threshold=np.nan)
from collections import defaultdict
from sklearn.preprocessing import MinMaxScaler
import matplotlib as mpl
if os.environ.get('DISPLAY','') == '':
    print('No display found; using non-interactive Agg backend...\n')
    mpl.use('Agg')
import matplotlib.pyplot as plt
# import seaborn as sns
import parser
import event
import outlier
import visualizer


# Proces config file
config = configparser.ConfigParser()
config.read(sys.argv[1])
dataType = config['Parser']['DataType']
funId = int(config['Parser']['FunId'])
numNeighbors = config['Lof']['n_neighbors']
contamination = float(config['Lof']['contamination'])
fixcontamination = int(config['Lof']['fixcontamination'])

# This sets conp appropriately 
if fixcontamination > 0:
    conp = fixcontamination

# Initialize parser object, get function id function name map 
prs = parser.Parser(sys.argv[1])
funMap = prs.getFunMap()

# Initialize event object
evn = event.Event(funMap, sys.argv[1])

# Initialize outlier object
otl = outlier.Outlier(sys.argv[1])

# Initialize visualizer object
viz = visualizer.Visualizer(sys.argv[1])

# Dump function data
viz.sendFunMap(list(funMap.values()))
eventType = ['ENTRY', 'EXIT', 'SEND', 'RECV']
viz.sendEventType(eventType)

# Stream events
ctFun = defaultdict(int)
dataOK = True
ctrl = 1
outct = 0
eventId = np.uint64(0)
while ctrl >= 0:
    print("\noutct = ", outct, "\n\n")
    
    # Stream function call data
    funStream = prs.getFunData()
    evn.initFunData(funStream.shape[0])
             
    for i in funStream:
        # Append eventId so we can backtrack which event in the trace was anomalous
        i = np.append(i,np.uint64(eventId))  
        if evn.addFun(i): # Store function call data in event object
            pass
        else:
            dataOK = False
            break        
        eventId += 1
        ctFun[i[4]] += 1
    if dataOK:
        pass
    else:
        print("\n\n\nCall stack violation at ", outct, " ", eventId, "... \n\n\n")
        break
    
    
    # Detect anomalies in function call data
    data = evn.getFunExecTime()
    outlId = []
    for funId in data: 
        X = np.array(data[funId])
        numPoints = X.shape[0]
        otl.compOutlier(X)
        funOutl = np.array(otl.getOutlier())        
        funOutlId = X[funOutl == -1][:,6]
        for i in range(0,len(funOutlId)):
            outlId.append(str(funOutlId[i]))
        
    
    # Determine function of interest
    
    #r = req.post(self.vizUrl, json={'type':'foi', 'value':['adios_close', 'adios_open']})
    

    
    
    # Stream counter data
    countStream = prs.getCountData() 
    countDataOut = np.full((countStream.shape[0], 13), np.nan)
    
    idx = 0
    for i in countStream:
        i = np.append(i,np.uint64(eventId))
        countDataOut[idx,0:3] = i[0:3]
        countDataOut[idx,5:7] = i[3:5]
        countDataOut[idx,11:13] = i[5:7]
        idx += 1
        eventId += 1 
    
     
    # Stream communication data
    commStream = prs.getCommData()
    commDataOut = np.full((commStream.shape[0], 13), np.nan)
    
    idx = 0
    for i in commStream:
        i = np.append(i,np.uint64(eventId))
        commDataOut[idx,0:4] = i[0:4]
        commDataOut[idx,8:11] = i[4:7]
        commDataOut[idx,11:13] = i[7:9]
        idx += 1
        eventId += 1


    # Dump trace data
    viz.sendTraceData(evn.getFunData(), countDataOut, commDataOut, outct)
    # Dump anomaly data
    viz.sendOutlIds(outlId, outct)
    
    # Free memory
    evn.clearFunDict()
    outlId.clear()
    evn.clearFunData()
    del countDataOut
    del commDataOut

    # Update outer loop counter   
    outct += 1 
        
    # Advance stream and check status
    prs.getStream()
    ctrl = prs.getStatus()
    
    
print("Total number of advance operations: ", outct)
print("Total number of events: ", eventId, "\n\n")

#ctFun = sorted(ctFun.items(), key=operator.itemgetter(1), reverse=True)
    
        #=======================================================================
        # # Plot graphs
        # if len(funOutlId) > 0:
        #     X = X[:,4:6]
        #     scaler = MinMaxScaler()
        #     X = scaler.fit_transform(X)
        #     
        #     f = plt.figure()
        #     pltitle = "STDB Method on NWChem \n" + "(Function ID: " + str(funId) + ", Number of Data Points: " + str(numPoints) + ")" 
        #      
        #     main = plt.subplot(211)
        #     main.set_title(pltitle)
        #     main.set_xlabel('Function entry time (scaled)')
        #     main.set_ylabel('Function exec. time (scaled)')
        #     a = main.scatter(X[funOutl == 1][:,0], X[funOutl == 1][:,1], c='white', edgecolor='k', s=20) #sns.kdeplot(X[outl == 1, 0], X[outl == 1, 1], cmap="Blues", shade=True)
        #     b = main.scatter(X[funOutl == -1][:,0], X[funOutl == -1][:,1], c='red', edgecolor='k', s=20)
        #     main.axis('tight')
        #     main.set_xlim((-0.1, 1.1))
        #     main.set_ylim((-0.1, 1.1))
        #      
        #     # Create a legend
        #     main.legend([a, b], ["inlier", "outlier"], loc="upper center", bbox_to_anchor=(0.1, -0.4))
        #       
        #     if fixcontamination > 0:
        #         params = main.annotate("nearest neighb. k: "+numNeighbors+"\n"+"contamination: "+" top "+str(fixcontamination), xy=(1, 0),  xycoords='data', 
        #             xytext=(0.99, -0.5), textcoords='axes fraction',
        #             bbox=dict(boxstyle="round", fc="w", alpha=0.25),
        #             horizontalalignment='right', verticalalignment='top', multialignment='left')
        #     else:
        #         params = main.annotate("nearest neighb. k: "+numNeighbors+"\n"+"contamination: "+" "+str(contamination*100)+"%", xy=(1, 0),  xycoords='data', 
        #             xytext=(0.99, -0.5), textcoords='axes fraction',
        #             bbox=dict(boxstyle="round", fc="w", alpha=0.25),
        #             horizontalalignment='right', verticalalignment='top', multialignment='left')
        #       
        #     #params.set_alpha(.75)
        #       
        #     zoom = plt.subplot(212)
        #     zoom.set_title("Zoom")
        #     zoom.set_xlabel('Function entry time (scaled)')
        #     zoom.set_ylabel('Function exec. time (scaled)')
        #     a = zoom.scatter(X[conp:, 0], X[conp:, 1], c='white', edgecolor='k', s=20) #sns.kdeplot(X[outl == 1, 0], X[outl == 1, 1], cmap="Blues", shade=True)
        #     b = zoom.scatter(X[:conp, 0], X[:conp, 1], c='red', edgecolor='k', s=20)
        #     #a = zoom.scatter(X[outl == 1, 0], X[outl == 1, 1], c='white', edgecolor='k', s=20)
        #     #b = zoom.scatter(X[outl == -1, 0], X[outl == -1, 1], c='red', edgecolor='k', s=20)
        #     zoom.axis('tight')
        #     zoom.set_xlim((-0.1, 1.1))
        #     zoom.set_ylim(((ymean-(0.15*ystd)), (ymean+ystd)))
        #      
        #      
        #     plt.subplots_adjust(hspace=1.1)
        #     if fixcontamination > 0:
        #         figfile = "NWChem_F" + str(funId) + "_" + "Nn" + numNeighbors + "_" + "ConTop" + str(fixcontamination) + ".png"
        #     else:
        #     figfile = "NWChem_F" + str(funId) + "_" + "Sigma" + "_" + "%.png"
        #          
        #     f.savefig(figfile)
        #     plt.close(f)
        #=======================================================================
    
    
    
    
    
    #===========================================================================
    # 
    # 
    # if dataType == 'funtime':
    #     funIds = list(data.keys())
    #     #===========================================================================
    #     # if funId in funIds:
    #     #     pass
    #     # else:
    #     #     raise Exception("Invalid function id ...")
    #     #===========================================================================
    #     for funId in funIds:
    #         if funId == 23: #23 or funId == 21:
    #             print("\n\n\nAnomaly detection for function: ", funId, " ...")
    #             
    #             # Save function data and Extract timestamp and exectuion time
    #             funData = X = np.array(data[funId])
    #             X = X[:,4:6]
    #             
    #             # Number of data points
    #             numPoints = X.shape[0]
    #             
    #             # This sets conp appropriately
    #             if (fixcontamination > 0) == False :
    #                 conp = contamination * numPoints
    #             
    #             # Scale data
    #             scaler = MinMaxScaler()
    #             X = scaler.fit_transform(X)
    #             ymean = np.mean(X[:,1])
    #             ystd = np.std(X[:,1])
    #             
    #             # Compute outliers
    #             otl = outlier.Outlier(sys.argv[1])
    #             otl.lofComp(X)
    #             scores = otl.getScore()
    #             #outl = otl.getOutlier()
    #             X = X[(-scores).argsort(kind='mergesort')]
    #             funData = funData[(-scores).argsort(kind='mergesort')]
    #             scores = scores[(-scores).argsort(kind='mergesort')]
    #             
    #             # Store scores 
    #             outfile = config['Parser']['OutFile'] + "NWChem_F" + str(funId) + "_Nn" + numNeighbors + "_scores.pickle"
    #             with open(outfile, 'wb') as handle:
    #                 pickle.dump(scores, handle, protocol=pickle.HIGHEST_PROTOCOL)
    #                 
    #             
    #             # Print funData
    #             print("Anomaly info\n")
    #             if fixcontamination > 0:
    #                 for i in range(0,conp):
    #                     print(i, ", ", funData[i,:])
    #              
    #             # Plot with function ID 
    #             f = plt.figure()
    #             pltitle = "LOF Method on NWChem \n" + "(Function ID: " + str(funId) + ", Number of Data Points: " + str(numPoints) + ")" 
    #             
    #             main = plt.subplot(211)
    #             main.set_title(pltitle)
    #             main.set_xlabel('Function entry time (scaled)')
    #             main.set_ylabel('Function exec. time (scaled)')
    #             a = main.scatter(X[conp:, 0], X[conp:, 1], c='white', edgecolor='k', s=20) #sns.kdeplot(X[outl == 1, 0], X[outl == 1, 1], cmap="Blues", shade=True)
    #             b = main.scatter(X[:conp, 0], X[:conp, 1], c='red', edgecolor='k', s=20)
    #             #a = zoom.scatter(X[outl == 1, 0], X[outl == 1, 1], c='white', edgecolor='k', s=20)
    #             #b = zoom.scatter(X[outl == -1, 0], X[outl == -1, 1], c='red', edgecolor='k', s=20)
    #             main.axis('tight')
    #             main.set_xlim((-0.1, 1.1))
    #             main.set_ylim((-0.1, 1.1))
    #             
    #             # Create a legend
    #             main.legend([a, b], ["inlier", "outlier"], loc="upper center", bbox_to_anchor=(0.1, -0.4))
    #             
    #             if fixcontamination > 0:
    #                 params = main.annotate("nearest neighb. k: "+numNeighbors+"\n"+"contamination: "+" top "+str(fixcontamination), xy=(1, 0),  xycoords='data', 
    #                     xytext=(0.99, -0.5), textcoords='axes fraction',
    #                     bbox=dict(boxstyle="round", fc="w", alpha=0.25),
    #                     horizontalalignment='right', verticalalignment='top', multialignment='left')
    #             else:
    #                 params = main.annotate("nearest neighb. k: "+numNeighbors+"\n"+"contamination: "+" "+str(contamination*100)+"%", xy=(1, 0),  xycoords='data', 
    #                     xytext=(0.99, -0.5), textcoords='axes fraction',
    #                     bbox=dict(boxstyle="round", fc="w", alpha=0.25),
    #                     horizontalalignment='right', verticalalignment='top', multialignment='left')
    #             
    #             #params.set_alpha(.75)
    #             
    #             zoom = plt.subplot(212)
    #             zoom.set_title("Zoom")
    #             zoom.set_xlabel('Function entry time (scaled)')
    #             zoom.set_ylabel('Function exec. time (scaled)')
    #             a = zoom.scatter(X[conp:, 0], X[conp:, 1], c='white', edgecolor='k', s=20) #sns.kdeplot(X[outl == 1, 0], X[outl == 1, 1], cmap="Blues", shade=True)
    #             b = zoom.scatter(X[:conp, 0], X[:conp, 1], c='red', edgecolor='k', s=20)
    #             #a = zoom.scatter(X[outl == 1, 0], X[outl == 1, 1], c='white', edgecolor='k', s=20)
    #             #b = zoom.scatter(X[outl == -1, 0], X[outl == -1, 1], c='red', edgecolor='k', s=20)
    #             zoom.axis('tight')
    #             zoom.set_xlim((-0.1, 1.1))
    #             zoom.set_ylim(((ymean-(0.15*ystd)), (ymean+ystd)))
    # 
    #            
    #             plt.subplots_adjust(hspace=1.1)
    #             if fixcontamination > 0:
    #                 figfile = "NWChem_F" + str(funId) + "_" + "Nn" + numNeighbors + "_" + "ConTop" + str(fixcontamination) + ".png"
    #             else:
    #                 figfile = "NWChem_F" + str(funId) + "_" + "Nn" + numNeighbors + "_" + "Con" + str(contamination*100) + "%.png"
    #                 
    #             f.savefig(figfile)
    #         
    #     
    #===========================================================================
