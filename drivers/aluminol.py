"""
Run anomaly detection algorithms
Authors: Gyorgy Matyasfalvi (gmatyasfalvi@bnl.gov)
Create: August, 2018
"""

import sys
import time
import os
import configparser
import pickle
import numpy as np
np.set_printoptions(threshold=np.nan)
from luminol.anomaly_detector import AnomalyDetector


# Proces config file
config = configparser.ConfigParser()
config.read(sys.argv[1])
pickleFile = config['Parser']['PickleFile']
dataType = config['Parser']['DataType']


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
        if funId == 23: #23 or funId == 21:
            print("\n\n\nAnomaly detection for function: ", funId, " ...")
            
            # Save function data and Extract timestamp and exectuion time
            funData = X = np.array(data[funId])
            X = X[:,4:6]
            
            # Number of data points
            numPoints = X.shape[0]
            
            # Build dictionary
            tser = {X[k,0]: X[k,1] for k in range(0,numPoints)}
            
            my_detector = AnomalyDetector(tser)
            score = my_detector.get_all_scores()
            anomalies = my_detector.get_anomalies()
            for a in anomalies:
                time_period = a.get_time_window()  
                print("anomaly: ", a, "time_period: ", time_period)  
        
