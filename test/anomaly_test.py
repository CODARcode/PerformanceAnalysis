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

