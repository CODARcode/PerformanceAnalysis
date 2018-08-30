"""
Run anomaly detection algorithms on data
Authors: Shinjae Yoo (sjyoo@bnl.gov), Gyorgy Matyasfalvi (gmatyasfalvi@bnl.gov)
Create: August, 2018
"""

import sys
import time
import os
import configparser
import pickle
import numpy as np
np.set_printoptions(threshold=np.nan)
from sklearn.preprocessing import MinMaxScaler
import matplotlib.pyplot as plt
import outlier


# Proces config file
config = configparser.ConfigParser()
config.read(sys.argv[1])
pickleFile = config['Parser']['PickleFile']
dataType = config['Parser']['DataType']
#funId = int(config['Parser']['FunId'])
numNeighbors = config['Lof']['n_neighbors']
contamination = float(config['Lof']['contamination'])
conp = 100 * contamination

# Load data (deserialize)
with open(pickleFile, 'rb') as handle:
    data = pickle.load(handle)

if dataType == 'funtime':
    funIds = list(data.keys())
    #===========================================================================
    # if funId in funIds:
    #     pass
    # else:
    #     raise Exception("Invalid function id ...")
    #===========================================================================
    for funId in funIds:
        print("\n\n\nAnomaly detection for function: ", funId, " ...")
        
        # Extract timestamp and exectuion time
        X = np.array(data[funId])
        X = X[:,4:6]
        
        # Number of data points
        numPoints = X.shape[0]
        
        # Scale data
        scaler = MinMaxScaler()
        X = scaler.fit_transform(X)
        ymean = np.mean(X[:,1])
        ystd = np.std(X[:,1])
        
        # Compute outliers
        otl = outlier.Outlier(sys.argv[1])
        otl.lofComp(X)
        scores = otl.getScore()
        outl = otl.getOutlier()
        
        # Store scores 
        outfile = config['Parser']['OutFile'] + "F" + str(funId) + "_scores.pickle"
        with open(outfile, 'wb') as handle:
            pickle.dump(scores, handle, protocol=pickle.HIGHEST_PROTOCOL)
         
        # Plot
        f = plt.figure()
        pltitle = "LOF on NWChem " + "funId: " + str(funId) + " numPoints: " + str(numPoints) 
        
        main = plt.subplot(211)
        main.set_title(pltitle)
        a = main.scatter(X[outl == 1, 0], X[outl == 1, 1], c='white', edgecolor='k', s=20)
        b = main.scatter(X[outl == -1, 0], X[outl == -1, 1], c='red', edgecolor='k', s=20)
        main.axis('tight')
        main.set_xlim((-0.1, 1.1))
        main.set_ylim((-0.1, 1.1))
        
        # Create a legend
        main.legend([a, b], ["inlier", "outlier"], loc="upper left")
        #ax = main.gca()
        params = main.annotate("Nn: "+numNeighbors+" "+"Con: "+" "+str(conp)+"%", xy=(1, 1),  xycoords='data', 
            xytext=(0.98, 0.95), textcoords='axes fraction',
            bbox=dict(boxstyle="round", fc="w", alpha=0.5),
            horizontalalignment='right', verticalalignment='top',)
        
        params.set_alpha(.5)
        
        zoom = plt.subplot(212)
        zoom.set_title("Zoom")
        a = zoom.scatter(X[outl == 1, 0], X[outl == 1, 1], c='white', edgecolor='k', s=20)
        b = zoom.scatter(X[outl == -1, 0], X[outl == -1, 1], c='red', edgecolor='k', s=20)
        zoom.axis('tight')
        zoom.set_xlim((-0.1, 1.1))
        zoom.set_ylim(((ymean-(0.1*ystd)), (ymean+ystd)))
       
        plt.subplots_adjust(hspace=0.5)
        figfile = "NWChem_F" + str(funId) + "_" + "Nn" + numNeighbors + "_" + "Con" + str(conp) + "%.pdf"
        f.savefig(figfile)
        
