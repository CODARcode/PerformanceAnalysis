"""@package Perfanal
This module implements the interface between the analysis code and the visualization server. 
Author(s): 
    Gyorgy Matyasfalvi (gmatyasfalvi@bnl.gov)
Created: 
    September, 2018
    
    pydoc -w visualizer
    
"""

import os
import json
import configparser
import time
import requests as req
import matplotlib as mpl
import numpy as np
if os.environ.get('DISPLAY','') == '':
    print('No display found; using non-interactive Agg backend...\n')
    mpl.use('Agg')
import adios as ad


class Visualizer():
    """This class provides an interface to Chimbuko's visualization software.
    It works in both online and offline mode. The online mode sends the trace data
    in a streaming fashion to the visualization server through requests API. 
    In offline mode it produces .json files that can be parsed by the visualization 
    software.
    """

    def __init__(self, configFile):
        """This is the constructor for the Visualizer class.
        The visualizer has two main modes defined by the VizMethod variable one is the offline, which writes
        .json files that the visualization software then parses, the other is online and sends the data to 
        the visualization server via the requests API.
      
        Args:
            configFile (string): The configuration files's name.
          
        Returns:
            No return value.
        """
        self.config = configparser.ConfigParser()
        self.config.read(configFile)
        self.vizMethod = self.config['Visualizer']['VizMethod']
        self.vizUrl = self.config['Visualizer']['VizUrl']
        self.outFile = self.config['Visualizer']['OutputFile']
        self.traceHeader = {'data': 'trace'}
        self.funData = []
        self.countData = []
        self.commData = []
        self.anomalyData = []
        
        
    def sendCombinedData(self, funData, countData, commData, funOfInt, outlId, outct):
        """Send method for sending function, counter and communication data as well as the
        results of the analysis function of interest and id(s) of outlier data points.
        
        Args:
            funData (list): function call data
            countData (list): counter data
            commData (list): communication data
            funOfInt (list): function id(s) of functions that have outliers
            outlId (list): event id(s) associated with events that are classified as outliers
            outct (int): frame id
            
        Returns:
            No return value.
        """  
        if self.vizMethod == "online":
            # Send data to viz server
            dataList = []
            try:
                assert(type(funData) is np.ndarray)
                dataList += funData.tolist()
            except:
                print("No function call data to visualize...")
            
            try:
                assert(type(countData) is np.ndarray)
                dataList += countData.tolist()
            except:
                print("No counter data to visualize...")
            
            try:
                assert(type(commData) is np.ndarray)
                dataList += commData.tolist()
            except:
                print("No communication data to visualize...")
            
            r = req.post(self.vizUrl, json={'type': 'info', 'value': {'events': dataList, 'foi': funOfInt, 'labels': outlId}})
            #time.sleep(3)
            #if r.status_code != 201:
            #    raise ApiError('Trace post error:'.format(r.status_code))
            
        if self.vizMethod == "offline":
            # Dump data
            dataList = []
            try:
                assert(type(funData) is np.ndarray)
                dataList += funData.tolist()
            except:
                print("No function call data to visualize...")
            
            try:
                assert(type(countData) is np.ndarray)
                dataList += countData.tolist()
            except:
                print("No counter data to visualize...")
            
            try:
                assert(type(commData) is np.ndarray)
                dataList += commData.tolist()
            except:
                print("No communication data to visualize...")
            
            traceDict={'type': 'info', 'value': {'events': dataList, 'foi': funOfInt, 'labels': outlId}}
            traceFileName = self.outFile + "trace." + str(outct) + ".json"
            with open(traceFileName, 'w') as outfile:
                json.dump(traceDict, outfile)
            outfile.close()
            # This line of code was added to check whether we ever encounter a non nan value in countData[i][7]
            #===================================================================
            # dataList = funData.tolist() + countData.tolist() + commData.tolist()
            # for i in range(0,countData.shape[0]):
            #     if str(countData[i][7]) != str('nan'):
            #         print(type(countData[i][7]))
            #         print(str(countData[i][7]))
            #         raise Exception("countData has non nan entry in column 7")
            #===================================================================
    
    def sendTraceData(self, funData, countData, commData, outct):
        """Send method for sending function, counter and communication.
        (Will be deprecated...)
        
        Args:
            funData (list): function call data
            countData (list): counter data
            commData (list): communication data
            outct (int): frame id
            
        Returns:
            No return value.
        """ 
        if self.vizMethod == "online":
            # Send data to viz server
            dataList = funData.tolist() + countData.tolist() + commData.tolist()
            r = req.post(self.vizUrl, json={'type':'events', 'value': dataList})
            # time.sleep(5)
            #if r.status_code != 201:
            #    raise ApiError('Trace post error:'.format(r.status_code))
        if self.vizMethod == "offline":
            # Dump data
            traceFileName = self.outFile + "trace." + str(outct) + ".json"
            with open(traceFileName, 'w') as outfile:
                json.dump(funData.tolist() + countData.tolist() + commData.tolist(), outfile)
            outfile.close()  
            
    def sendOutlIds(self, outlId, outct):
        """Send method for sending the id(s) of outlier data points.
        (Will be deprecated...)
        
        Args:
            outlId (list): event id(s) associated with events that are classified as outliers
            outct (int): frame id
            
        Returns:
            No return value.
        """ 
        if self.vizMethod == "online":
            # Send data to viz server
            r = req.post(self.vizUrl, json = {'type':'labels', 'value': outlId})
           # time.sleep(1)
           # if r.status_code != 201:
           #     raise ApiError('Anomaly post error:'.format(r.status_code))
        if self.vizMethod == "offline":
            # Dump data
            anomalyFileName = self.outFile + "anomaly." + str(outct) + ".json"
            with open(anomalyFileName, 'w') as outfile:
                json.dump(outlId, outfile)
            outfile.close()   
    
    def sendFunMap(self, funMap, outct):
        """Send method for sending the dictionary of function id(s) (key) and their corresponding names (value).
        (Will be deprecated...)
        
        Args:
            funMap (dictionary): function id (int) key and function name (string) value
            outct (int): frame id
            
        Returns:
            No return value.
        """
        if self.vizMethod == "online":
            # Send data to viz server
            r = req.post(self.vizUrl, json={'type':'functions', 'value': funMap})
           # time.sleep(1)
           # if r.status_code != 201:
           #     raise ApiError('Function map post error:'.format(r.status_code))
        if self.vizMethod == "offline":
            # Dump data            
            funDict={'type': 'functions', 'value': funMap}    
            funMapFileName = self.outFile + "functions." + str(outct) + ".json"  
            with open(funMapFileName, 'w') as outfile:
                json.dump(funMap, outfile)
            outfile.close()
    
    def sendFunOfInt(self, funOfInt, outct):
        """Send method for sending function of interest id(s) to the visualization server.
        These are id(s) of functions that are associated with outlier data points.
        
        Args:
            funOfInt (list): function id(s) of functions that have outliers
            outct (int): frame id
            
        Returns:
            No return value.
        """
        if self.vizMethod == "online":
            r = req.post(self.vizUrl, json={'type':'foi', 'value':funOfInt})
            # if r.status_code != 201:
            #     raise ApiError('Function map post error:'.format(r.status_code))
        if self.vizMethod == "offline":
            # Dump data
            funOfIntFileName = self.outFile + "foi." + str(outct) + ".json"
            with open(funOfIntFileName, 'w') as outfile:
                json.dump(funOfInt, outfile)
            outfile.close()
            
    def sendEventType(self, eventType, outct):
        """Send method for sending event types and thier id(s) to the visualization server.
        
        Args:
            eventType (list): of event types such as function EXIT/ENTRY or SEND/RECEIVE for communication events.
            outct (int): frame id
            
        Returns:
            No return value.
        """
        if self.vizMethod == "online":
            r = req.post(self.vizUrl, json={'type':'event_types', 'value':eventType})
            #if r.status_code != 201:
            #    raise ApiError('Function map post error:'.format(r.status_code))
        if self.vizMethod == "offline":
            eventTypeFileName = self.outFile + "et." + str(outct) + ".json" 
            eventDict = {'type':'event_types', 'value':eventType}
            with open(eventTypeFileName, 'w') as outfile:
                json.dump(eventType, outfile)
            outfile.close()
            
    def sendReset(self):
        """Send signal to visualization server to restart itself.
        
        Args:
            No arguments.
            
        Returns:
            No return value.
        """
        if self.vizMethod == "online":
            r = req.post(self.vizUrl, json={'type':'reset'})
            #if r.status_code != 201:
            #    raise ApiError('Function map post error:'.format(r.status_code))
        if self.vizMethod == "offline":
            resetDict ={'type':'reset'}
            resetFileName = self.outFile + "reset.json"
            with open(resetFileName, 'w') as outfile:
                json.dump(resetDict, outfile)
            outfile.close()
    
    #def addFunData(self, funData):
    #    self.funData.extend(funData.tolist())
        
    #def addCountData(self, countData):
    #    self.countData.extend(countData.tolist())
        
    #def addCommData(self, commData):
    #    self.commData.extend(commData.tolist())
        
    
