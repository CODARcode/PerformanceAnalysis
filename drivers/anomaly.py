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
from sklearn.preprocessing import MinMaxScaler
import matplotlib.pyplot as plt
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
    
    # Extract timestamp and exectuion time
    X = np.array(data[funId])
    X = X[:,4:6]
    
    # Scale data
    scaler = MinMaxScaler()
    X = scaler.fit_transform(X)
    
otl = outlier.Outlier(sys.argv[1])
otl.lofComp(X)
score = otl.getScore()
outl = otl.getOutlier()

plt.title("Local Outlier Factor (LOF)")
a = plt.scatter(X[outl == 1, 0], X[outl == 1, 1], c='white', edgecolor='k', s=20)
b = plt.scatter(X[outl == -1, 0], X[outl == -1, 1], c='red', edgecolor='k', s=20)
plt.axis('tight')
plt.xlim((-0.1, 1.1))
plt.ylim((-0.1, 1.1))
plt.legend([a, b], ["inlier", "outlier"], loc="upper right")
plt.show()



