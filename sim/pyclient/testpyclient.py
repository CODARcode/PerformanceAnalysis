import socket
import sys
import json
from time import sleep
import random
from chimbukoSim import chimbukoSim
import os
import glob

''' cleanup Provdb files in server directory '''
DOI = "../../install/sim/main"
files_to_remove = glob.glob(DOI+"provdb.*")
[os.remove(f) for f in files_to_remove if os.path.exists(f)]

cs = chimbukoSim()
cs.connect()
#cs.initialize()
data_dic = {}
initialized = False
for x in range(2):
    data_dic['data'] = [random.randrange(1, 50, 1) for _ in range(500)]
    data_dic['data'].append(random.randint(2000,5000)) #
    cs.send(data_dic)
    sleep(1)

#Inserting 2 labelled anomalies
data_dic['data'] = [random.randrange(100, 500, 10) for _ in range(500)]
data_dic['data'].append(random.randint(10000,12000))
data_dic['data'].append(random.randint(11000,13000))
cs.send(data_dic)

#Insert 10 Anomalies
#data_dic['data'] = [random.randrange(500, 5000, 10) for _ in range(500)]
#[data_dic['data'].append(random.randint(20000,30000)) for _ in range(10)]
#cs.send(data_dic)

