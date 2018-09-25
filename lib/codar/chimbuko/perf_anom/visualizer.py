"""
Visualize outlier detection results 
Authors: Gyorgy Matyasfalvi (gmatyasfalvi@bnl.gov)
Create: September, 2018
"""

import os
import json
import configparser
import time
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
            dataList = funData.tolist() + countData.tolist() + commData.tolist()
            r = req.post(self.vizUrl, json={'type':'events', 'value': dataList})
            time.sleep(5)
            #if r.status_code != 201:
            #    raise ApiError('Trace post error:'.format(r.status_code))
        if self.vizMethod == "offline":
            # Dump data
            traceFileName = "trace." + str(outct) + ".json"
            with open(traceFileName, 'w') as outfile:
                json.dump(funData.tolist() + countData.tolist() + commData.tolist(), outfile)
            outfile.close()  
            
    def sendOutlIds(self, outlId, outct):
        if self.vizMethod == "online":
            # Send data to viz server
            r = req.post(self.vizUrl, json = {'type':'labels', 'value': outlId})
            time.sleep(1)
           # if r.status_code != 201:
           #     raise ApiError('Anomaly post error:'.format(r.status_code))
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
            r = req.post(self.vizUrl, json={'type':'functions', 'value': funMap})
            time.sleep(1)
           # if r.status_code != 201:
           #     raise ApiError('Function map post error:'.format(r.status_code))
        if self.vizMethod == "offline":
            # Dump data   
            with open("function.json", 'w') as outfile:
                json.dump(funMap, outfile)
            outfile.close()
    
    def sendFunOfInt(self, funOfInt, outct):
        if self.vizMethod == "online":
            r = req.post(self.vizUrl, json={'type':'foi', 'value':funOfInt})
            # if r.status_code != 201:
            #     raise ApiError('Function map post error:'.format(r.status_code))
        if self.vizMethod == "offline":
            # Dump data
            print("function of interest ids: ", funOfInt)
            funOfIntFileName = "foi." + str(outct) + ".json"
            with open(funOfIntFileName, 'w') as outfile:
                json.dump(funOfInt, outfile)
            outfile.close()
            
    def sendEventType(self, eventType):
        if self.vizMethod == "online":
            r = req.post(self.vizUrl, json={'type':'event_types', 'value':eventType})
            #if r.status_code != 201:
            #    raise ApiError('Function map post error:'.format(r.status_code))
        if self.vizMethod == "offline":
            with open("et.json", 'w') as outfile:
                json.dump(eventType, outfile)
            outfile.close()
    
    def addFunData(self, funData):
        self.funData.extend(funData.tolist())
        
    def addCountData(self, countData):
        self.countData.extend(countData.tolist())
        
    def addCommData(self, commData):
        self.commData.extend(commData.tolist())
        
    