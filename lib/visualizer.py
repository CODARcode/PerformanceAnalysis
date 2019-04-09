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
from utils.dataWorker import dataWorker
import json
import requests as req
#import matplotlib as mpl
#if os.environ.get('DISPLAY','') == '':
#    print('No display found; using non-interactive Agg backend...\n')
#    mpl.use('Agg')


class Visualizer():
    """This class provides an interface to Chimbuko's visualization software.
    It works in both online and offline mode. The online mode sends the trace data
    in a streaming fashion to the visualization server through requests API. 
    In offline mode it produces .json files that can be parsed by the visualization 
    software.
    """

    def __init__(self, config, log=None, rank=-1):
        """This is the constructor for the Visualizer class.
        The visualizer has two main modes defined by the `VizMethod` variable.
            - offline: writes .json files that the visualization software parses
            - online: sends the data to the visualization server via the requests API.
      
        Args:
            config: config object
        """
        self.log = log
        self.rank = rank

        self.vizMethod = str(config['Visualizer']['VizMethod'])
        self.vizUrl = config['Visualizer']['VizUrl']
        self.outputDir = config['Visualizer']['OutputDir']

        try:
            self.onlyAbnormal = config['Visualizer'].getboolean('OnlyAbnormal')
        except KeyError:
            self.onlyAbnormal = False

        try:
            self.saveType = config['Visualizer']['SaveType']
        except KeyError:
            self.saveType = 'json'

        try:
            self.n_workers = config['Visualizer'].getint('UseWorker')
        except KeyError:
            self.n_workers = 1

        #self.traceHeader = {'data': 'trace'}
        self.funData = []
        self.countData = []
        self.commData = []
        self.anomalyData = []

        if self.rank >= 0:
            self.outputDir = self.outputDir + '-{:03d}'.format(self.rank)

        # potentinally, there could be multiple workers depending on the performance.
        # currently, it only creates single worker.
        self.worker = None
        if self.vizMethod in ['online', 'offline']:
            self.worker = None
            if self.n_workers > 0:
                self.worker = dataWorker(log=log, saveType=self.saveType)

            if self.vizMethod == 'offline' and os.path.exists(self.outputDir):
                shutil.rmtree(self.outputDir)

            if self.vizMethod == 'offline' and not os.path.exists(self.outputDir):
                os.makedirs(self.outputDir)

    def join(self, force_to_quit):
        if self.worker is not None:
            self.worker.join(force_to_quit, self.rank)

    def sendReset(self):
        """Send signal to visualization server to restart itself."""
        if self.vizMethod == 'off': return

        resetDict = {'type': 'reset'}
        if self.vizMethod == "online":
            req.post(self.vizUrl + '/events', json=resetDict)
        elif self.vizMethod == "offline":
            fn = "reset.{:03d}".format(self.rank) \
                if self.rank >= 0 \
                else "reset"
            if self.worker is not None:
                self.worker.put(self.vizMethod,
                                os.path.join(self.outputDir, fn),
                                resetDict, self.rank, 0)
        else:
            raise ValueError("Unsupported method: %s" % self.vizMethod)

    def sendEventType(self, eventType, frame_id):
        """Send method for sending event types and thier id(s) to the visualization server.

        Args:
            eventType (list): of event types
                such as function EXIT/ENTRY or SEND/RECEIVE for communication events.
            frame_id (int): frame id
        """
        if self.vizMethod == 'off': return

        eventDict = {'type': 'event_types', 'value': eventType}
        if self.vizMethod == "online":
            r = req.post(self.vizUrl + '/events', json=eventDict)
        elif self.vizMethod == "offline":
            fn = "et.{:06d}.{:03d}".format(frame_id, self.rank) \
                if self.rank >= 0 \
                else "et.{:06d}".format(frame_id)
            if self.worker is not None:
                self.worker.put(self.vizMethod,
                                os.path.join(self.outputDir, fn),
                                eventDict, self.rank, frame_id)
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
        if self.vizMethod == 'off': return

        funDict = {'type': 'functions', 'value': funMap}
        if self.vizMethod == "online":
            req.post(self.vizUrl, json={'type': 'functions', 'value': funMap})
        elif self.vizMethod == "offline":
            fn = "functions.{:06d}.{:03d}".format(frame_id, self.rank) \
                if self.rank >=0 \
                else "functions.{:06d}".format(frame_id)
            if self.worker is not None:
                self.worker.put(self.vizMethod,
                                os.path.join(self.outputDir, fn),
                                funDict, self.rank, frame_id)
        else:
            raise ValueError("Unsupported method: %s" % self.vizMethod)

    def sendData(self, execData, funMap, getStat, frame_id=0):
        """
        Send function, counter and communication data as well as the results of the analysis
        of interest and id(s) of outlier data points.

        Args:
        """
        if self.vizMethod == 'off': return

        execDict = {}
        stat = {}
        for funid, execList in execData.items():
            for d in execList:
                if self.onlyAbnormal and d.label == 1:
                    continue
                key = d.get_id()
                value = d.to_dict()
                execDict[str(key)] = value

            n_total, n_abnormal, mean, std, factor = getStat(funid)
            key = funMap[funid]
            stat[key] = {
                "total": n_total,
                "factor": factor,
                "abnormal": n_abnormal,
                "mean": mean,
                "std": std,
            }

        if self.saveType == 'json':
            traceDict = {
                'executions': execDict,
                'stat': stat
            }
        else:
            traceDict = execData
            # traceDict = {
            #     'executions': execData,
            #     'stat': stat
            # }

        if self.vizMethod == "online":
            if self.worker is not None:
                self.worker.put(self.vizMethod, self.vizUrl + '/executions', traceDict,
                                self.rank, frame_id)
            else:
                try:
                    r = req.post(self.vizUrl + '/executions', json=traceDict)
                    r.raise_for_status()
                except req.exceptions.HTTPError as e:
                    if self.log is not None: self.log.info("Http Error: ", e)
                except req.exceptions.ConnectionError as e:
                    if self.log is not None: self.log.info("Connection Error: ", e)
                except req.exceptions.Timeout as e:
                    if self.log is not None: self.log.info("Timeout Error: ", e)
                except req.exceptions.RequestException as e:
                    if self.log is not None: self.log.info("OOps: something else: ", e)
                except Exception as e:
                    if self.log is not None: self.log.info("Really unknown error: ", e)

        elif self.vizMethod == "offline":
            fn = "trace.{:06d}.{:03d}".format(frame_id, self.rank) \
                if self.rank >= 0 \
                else "trace.{:06d}".format(frame_id)
            if self.worker is not None:
                self.worker.put(self.vizMethod,
                                os.path.join(self.outputDir, fn),
                                traceDict, self.rank, frame_id)

        else:
            raise ValueError("Unsupported method: %s" % self.vizMethod)
