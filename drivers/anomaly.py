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
import parser
import event
import outlier


# Initialize parser and event classes
prs = parser.Parser(sys.argv[1])
evn = event.Event(prs.getFunMap(), sys.argv[1])

Points = Point()
clf = LocalOutlierFactor(n_neighbors=kpar, algorithm="kd_tree", leaf_size=30, metric='euclidean')
clf.fit(datastream)
Points.LOF = [-x for x in clf.negative_outlier_factor_.tolist()]
Points.lrd = clf._lrd.tolist()
dist, ind = clf.kneighbors()
Points.kdist = dist.tolist()
Points.knn = ind.tolist()
return Points