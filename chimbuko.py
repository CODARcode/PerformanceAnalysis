"""
This is main driver that calls analysis tools.

Author(s):
    Sungsoo Ha (sungsooha@bnl.gov)

Created:
    March 7, 2019
"""
import sys
import logging
import configparser
import numpy as np
from collections import defaultdict

from definition import *
from parser2 import Parser
from event import Event
from outlier import Outlier

class Chimbuko(object):
    def __init__(self, configFile:str):
        self.config = configparser.ConfigParser(interpolation=None)
        self.config.read(configFile)

        # Initialize logger
        logging.basicConfig(
            level=self.config['Debug']['LogLevel'],
            format=self.config['Debug']['Format'],
            filename=self.config['Debug']['LogFile']
        )
        self.log = logging.getLogger('CHIMBUKO')

        # Parser: data handler
        self.parser = Parser(self.config, self.log)

        # Event: event handler
        self.event = Event()
        # Outlier: outlier detector
        self.outlier = Outlier(self.config)
        # Visualizer: visualization handler
        self.maxDepth = int(self.config['Visualizer']['MaxFunDepth'])
        self.visualizer = None
        self._init()

        # init variable
        self.status = True
        # event id
        # note: maybe to generate unique id, is this okay with overflow?
        self.event_id = np.uint64(0)
        # total number of outliers
        self.n_outliers = 0
        # function data counter
        self.func_counter = defaultdict(int)
        # count data counter
        self.count_counter = defaultdict(int)
        # communication data counter
        self.comm_counter = defaultdict(int)
        # anomaly function
        self.anomFun = defaultdict(int)

    def _init(self):
        self._init_event(self.parser.Method == 'BP')

    def _init_event(self, force_to_init=False):
        if force_to_init or self.parser.Method != 'BP':
            self.event.setEventType(list(self.parser.eventType.values()))
        self.event.clearFunTime()
        self.event.clearFunData()
        self.event.clearCountData()
        self.event.clearCommData()

    def _process_func_data(self):
        try:
            funcData = self.parser.getFunData()
        except AssertionError:
            self.log.info("Frame-{} has no function data...".format(self.parser.getStatus()))
            return
        print('funData: ', funcData.shape)
        self.event.initFunData(len(funcData))
        for data in funcData:
            if not self.event.addFun(np.append(data, np.uint64(self.event_id))):
                self.status = False
                self.log.error(
                    "\n\n\nCall stack violation at Frame: {}, Event: {}!\n\n\n".format(
                        self.parser.getStatus(), self.event_id)
                )
                break
            self.event_id += 1
            self.func_counter[data[FUN_IDX_FUNC]] += 1
        print('stack size: ', self.event.getFunStackSize())
        #print(self.event.printFunStack())

    def _process_counter_data(self):
        try:
            countData = self.parser.getCountData()
        except AssertionError:
            self.log.info("Frame-{} has no counter data...".format(self.parser.getStatus()))
            return
        print('countData: ', countData.shape)
        self.event.initCountData(len(countData))
        for data in countData:
            if not self.event.addCount(np.append(data, np.uint64(self.event_id))):
                self.status = False
                self.log.error(
                    "\n\n\nError in adding count data event at Frame: {}, Event: {}!\n\n\n".format(
                        self.parser.getStatus(), self.event_id
                    )
                )
                break
            self.event_id += 1
            self.count_counter[data[CNT_IDX_COUNT]] += 1

    def _process_communication_data(self):
        try:
            commData = self.parser.getCommData()
        except AssertionError:
            self.log.info("Frame-{} has no communication data...".format(self.parser.getStatus()))
            return
        print('commData: ', commData.shape)
        self.event.initCommData(len(commData))
        for data in commData:
            if not self.event.addComm(np.append(data, np.uint64(self.event_id))):
                self.status = False
                self.log.error(
                    "\n\n\nError in adding communication data event at Frame: {}, "
                    "Event: {}!\n\n\n".format(self.parser.getStatus(), self.event_id)
                )
                break
            self.event_id += 1
            self.comm_counter[data[COM_IDX_TAG]] += 1

    def _run_anomaly_detection(self, funMap):
        try:
            functime = self.event.getFunTime()
        except AssertionError:
            self.log.info("Only contains open functions so no anomaly detection at Frame: {}".format(
                self.parser.getStatus()
            ))
            return

        outliers_id_str = []
        funOfInt = []
        for id, data in functime.items():
            data = np.array(data)
            n_data = len(data)

            self.outlier.compOutlier(data, id)
            outliers = self.outlier.getOutlier()
            outliers_id = data[outliers==-1, -1]

            outliers_id_str += np.array(outliers_id, dtype=np.str).tolist()

            maxFuncDepth = self.event.getMaxFunDepth()
            if n_data > np.sum(outliers) and maxFuncDepth[id] < self.maxDepth:
                funOfInt.append(str(funMap[id]))
            if len(outliers_id) > 0:
                self.anomFun[id] += 1

        return outliers_id_str, funOfInt


    def process(self):
        # check current status of the parser
        self.status = self.parser.getStatus() >= 0
        if not self.status: return

        print('Step: ', self.parser.getStatus())

        # Initialize event
        self._init_event()
        funMap = self.parser.getFunMap()

        # process on function event
        self._process_func_data()
        if not self.status: return

        # detect anomalies in function call data
        outlId, funOfInt = self._run_anomaly_detection(funMap)
        self.n_outliers += len(outlId)
        self.log.info("Numer of outliers per from: %s" % len(outlId))

        # process on counter event
        self._process_counter_data()
        if not self.status: return

        # process on communication event
        self._process_communication_data()
        if not self.status: return

        # go to next stream
        self.parser.getStream()

    def finalize(self):
        pass

def usage():
    pass

if __name__ == '__main__':
    # check argument and print usages()

    configFile = './test/test.cfg' #sys.argv[1]
    driver = Chimbuko(configFile)

    while driver.status:
        driver.process()

    driver.finalize()