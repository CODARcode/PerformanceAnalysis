"""
Visualize outlier detection results 
Authors: Gyorgy Matyasfalvi (gmatyasfalvi@bnl.gov)
Create: September, 2018
"""

import os
import json
import configparser
import requests as req
import matplotlib as mpl
if os.environ.get('DISPLAY','') == '':
    print('No display found; using non-interactive Agg backend...\n')
    mpl.use('Agg')
import adios as ad


class Visualizer():
    def __init__(self, configFile):
        self.config = configparser.ConfigParser()
        self.config.read(configFile)
        self.vizMethod = self.config['Visualizer']['VizMethod']
        self.vizUrl = self.config['Visualizer']['VizUrl']
        self.vizPath = self.config['Visualizer']['VizPath']
        self.dumpFile = self.config['Visualizer']['DumpFile']
        self.traceHeader = {'data': 'trace'}
        self.funData = []
        self.countData = []
        self.commData = []
        self.anomalyData = []
        
    
    def sendTraceData(self, funData, countData, commData, outct):
        if self.vizMethod == "online":
            # Send data to viz server
            r = req.post(self.vizUrl, 
                        data=json.dumps(funData.tolist() + 
                        countData.tolist() + 
                        commData.tolist()))
            if r.status_code != 201:
                raise ApiError('Trace post error:'.format(r.status_code))
        if self.vizMethod == "offline":
            # Dump data
            traceFileName = "trace." + str(outct) + ".json"
            with open(traceFileName, 'w') as outfile:
                json.dump(funData.tolist() + countData.tolist() + commData.tolist(), outfile)
            outfile.close()  
            
    def sendOutlIds(self, outlId, outct):
        if self.vizMethod == "online":
            # Send data to viz server
            r = req.post(self.vizUrl, data=json.dumps(outlId))
            if r.status_code != 201:
                raise ApiError('Anomaly post error:'.format(r.status_code))
        if self.vizMethod == "offline":
            # Dump data
            print("anomaly ids: ", outlId)
            anomalyFileName = "anomaly." + str(outct) + ".json"
            with open(anomalyFileName, 'w') as outfile:
                json.dump(outlId, outfile)
            outfile.close()   
    
    def sendFunMap(self, funMap):
        if self.vizMethod == "online":
            # Send data to viz server
            r = req.post(self.vizUrl, data=json.dumps(funMap))
            if r.status_code != 201:
                raise ApiError('Function map post error:'.format(r.status_code))
        if self.vizMethod == "offline":
            # Dump data   
            with open("function.json", 'w') as outfile:
                json.dump(funMap, outfile, indent=4)
            outfile.close()
    
    def addFunData(self, funData):
        self.funData.extend(funData.tolist())
        
    def addCountData(self, countData):
        self.countData.extend(countData.tolist())
        
    def addCommData(self, commData):
        self.commData.extend(commData.tolist())
        
    