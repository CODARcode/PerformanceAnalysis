"""@package Chimbuko
This module implements the interface between the analysis code and the visualization server. 
Author(s): 
    Gyorgy Matyasfalvi (gmatyasfalvi@bnl.gov)
    Sungsoo Ha (sungsooha@bnl.gov)

Created: 
    September, 2018

Modified:
    March, 2019
    
    pydoc -w visualizer
    
"""
import os
import shutil
import json
import requests as req
import matplotlib as mpl
if os.environ.get('DISPLAY','') == '':
    print('No display found; using non-interactive Agg backend...\n')
    mpl.use('Agg')


class Visualizer():
    """This class provides an interface to Chimbuko's visualization software.
    It works in both online and offline mode. The online mode sends the trace data
    in a streaming fashion to the visualization server through requests API. 
    In offline mode it produces .json files that can be parsed by the visualization 
    software.
    """

    def __init__(self, config):
        """This is the constructor for the Visualizer class.
        The visualizer has two main modes defined by the `VizMethod` variable.
            - offline: writes .json files that the visualization software parses
            - online: sends the data to the visualization server via the requests API.
      
        Args:
            config: config object
        """
        self.vizMethod = config['Visualizer']['VizMethod']
        self.vizUrl = config['Visualizer']['VizUrl']
        self.outputDir = config['Visualizer']['OutputDir']
        #self.traceHeader = {'data': 'trace'}
        self.funData = []
        self.countData = []
        self.commData = []
        self.anomalyData = []

        if self.vizMethod == 'offline' and os.path.exists(self.outputDir):
            shutil.rmtree(self.outputDir)

        if self.vizMethod == 'offline' and not os.path.exists(self.outputDir):
            os.makedirs(self.outputDir)

    def sendReset(self):
        """Send signal to visualization server to restart itself."""
        resetDict = {'type': 'reset'}
        if self.vizMethod == "online":
            req.post(self.vizUrl, json=resetDict)
        elif self.vizMethod == "offline":
            fn = "reset.json"
            with open(os.path.join(self.outputDir, fn), 'w') as outfile:
                json.dump(resetDict, outfile, indent=4, sort_keys=True)
        else:
            raise ValueError("Unsupported method: %s" % self.vizMethod)

    def sendEventType(self, eventType, frame_id):
        """Send method for sending event types and thier id(s) to the visualization server.

        Args:
            eventType (list): of event types
                such as function EXIT/ENTRY or SEND/RECEIVE for communication events.
            frame_id (int): frame id
        """
        eventDict = {'type': 'event_types', 'value': eventType}
        if self.vizMethod == "online":
            r = req.post(self.vizUrl, json=eventDict)
        if self.vizMethod == "offline":
            fn = "et.{:06d}.json".format(frame_id)
            with open(os.path.join(self.outputDir, fn), 'w') as outfile:
                json.dump(eventDict, outfile)
        else:
            raise ValueError("Unsupported method: %s" % self.vizMethod)

    def sendFunMap(self, funMap, frame_id=0):
        """Send method for sending the dictionary
            - key: function id(s) and
            - value: their corresponding names.

        Args:
            funMap (dictionary): function id (int) key and function name (string) value
            frame_id (int): frame id
        """
        funDict = {'type': 'functions', 'value': funMap}
        if self.vizMethod == "online":
            req.post(self.vizUrl, json={'type': 'functions', 'value': funMap})
        elif self.vizMethod == "offline":
            fn = "functions.{:06d}.json".format(frame_id)
            with open(os.path.join(self.outputDir, fn), 'w') as outfile:
                json.dump(funDict, outfile, indent=4, sort_keys=True)
        else:
            raise ValueError("Unsupported method: %s" % self.vizMethod)

    def sendData_v1(self,
                 funData, countData, commData, funOfInt, outlId, frame_id=0,
                 eventType=None, funMap=None):
        """
        Send function, counter and communication data as well as the results of the analysis
        of interest and id(s) of outlier data points.

        Args:
            funData: (list), function call data
            countData: (list), counter data
            commData: (list), communication data
            funOfInt: (list), function id(s) of functions that have outliers
            outlId: (list), event id(s) associated with events that are classified as outliers
            frame_id: (int), frame id
            eventType: (list), event types
        """
        #todo: is this correct way to aggregate all data
        dataList = []
        dataList += funData
        #dataList += countData
        #dataList += commData

        traceDict = {
            'type': 'info',
            'value': {
                'events': dataList,
                'foi': funOfInt,
                'labels': outlId
            }
        }
        if eventType is not None:
            traceDict['value']['event_types'] = eventType

        if funMap is not None:
            traceDict['value']['functions'] = funMap

        # print('funcMap')
        # for k, v in traceDict['value']['functions'].items():
        #     print(k, v)
        # print('event types: ', traceDict['value']['event_types'])
        # print('funData')
        # for d in traceDict['value']['events']:
        #     print(d)
        # print(self.vizMethod, self.vizUrl)

        if self.vizMethod == "online":
            try:
                r = req.post(self.vizUrl + '/events', json=traceDict)
                r.raise_for_status()
            except req.exceptions.HTTPError as e:
                print("Http Error: ", e)
            except req.exceptions.ConnectionError as e:
                print("Connection Error: ", e)
            except req.exceptions.Timeout as e:
                print("Timeout Error: ", e)
            except req.exceptions.RequestException as e:
                print("OOps: something else: ", e)
            except Exception as e:
                print("Really unknown error: ", e)

        elif self.vizMethod == "offline":
            fn = "trace.{:06d}.json".format(frame_id)
            with open(os.path.join(self.outputDir, fn), 'w') as outfile:
                json.dump(traceDict, outfile, indent=4, sort_keys=True)
        else:
            raise ValueError("Unsupported method: %s" % self.vizMethod)

    def sendData_v2(self, execData, anomFunCount, funMap, getStat, frame_id=0):
        """
        Send function, counter and communication data as well as the results of the analysis
        of interest and id(s) of outlier data points.

        Args:
        """
        #execList = []
        execDict = {}
        stat = {}

        for funid, execList in execData.items():
            #n_normal = 0
            #n_abnormal = 0
            for d in execList:
                if d.label == 1:
                    #n_normal += 1
                    continue

                key = d.get_id()
                value = d.to_dict()
                #execList.append(value)
                execDict[str(key)] = value
                #n_abnormal += 1

            n_abnormal = anomFunCount[funid]
            if n_abnormal > 0:
                key = funMap[funid]
                total = getStat(funid)[0]
                n_normal = total - n_abnormal

                try:
                    ratio = n_abnormal / (n_normal + n_abnormal) * 100.
                except ZeroDivisionError:
                    ratio = 0.

                stat[key] = {
                    "abnormal": n_abnormal,
                    "regular": n_normal,
                    "ratio": ratio
                }
                #print(funid, ratio)

        traceDict = {
            'executions': execDict,
            'stat': stat
        }

        # print('executions: ')
        # print(execDict.keys())
        # for k, v in execDict.items():
        #     print(k, v)

        # print('stat: ')
        # for k, v in stat.items():
        #     print(k, v)

        if self.vizMethod == "online":
            try:
                r = req.post(self.vizUrl + '/executions', json=traceDict)
                r.raise_for_status()
            except req.exceptions.HTTPError as e:
                print("Http Error: ", e)
            except req.exceptions.ConnectionError as e:
                print("Connection Error: ", e)
            except req.exceptions.Timeout as e:
                print("Timeout Error: ", e)
            except req.exceptions.RequestException as e:
                print("OOps: something else: ", e)
            except Exception as e:
                print("Really unknown error: ", e)


        elif self.vizMethod == "offline":
            fn = "trace.{:06d}.json".format(frame_id)
            with open(os.path.join(self.outputDir, fn), 'w') as outfile:
                json.dump(traceDict, outfile, indent=4, sort_keys=True)
        else:
            raise ValueError("Unsupported method: %s" % self.vizMethod)
