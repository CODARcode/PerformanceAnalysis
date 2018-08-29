"""
Run anomaly detection algorithms on various data sets
Authors: Shinjae Yoo (sjyoo@bnl.gov), Gyorgy Matyasfalvi (gmatyasfalvi@bnl.gov)
Create: August, 2018
"""

import sys
import time
import os
import configparser
import pickle
import numpy as np
import outlier


# Proces config file
config = configparser.ConfigParser()
config.read(sys.argv[1])
pickleFile = config['Parser']['PickleFile']
dataType = config['Parser']['DataType']
funId = int(config['Parser']['FunId'])

# Load data (deserialize)
with open(pickleFile, 'rb') as handle:
    data = pickle.load(handle)

if dataType == 'funtime':
    funIds = list(data.keys())
    if funId in funIds:
        pass
    else:
        raise Exception("Invalid function id ...")
    
    X = np.array(data[funId])
    print("X = ", X, "\n\n\n")

otl = outlier.Outlier(sys.argv[1])



