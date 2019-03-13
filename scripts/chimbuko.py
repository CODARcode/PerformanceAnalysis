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
from visualizer import Visualizer

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
        self.log.addHandler(logging.StreamHandler(sys.stdout))

        # Parser: data handler
        self.parser = Parser(self.config, self.log)

        # Event: event handler
        self.event = Event()
        # Outlier: outlier detector
        self.outlier = Outlier(self.config, self.log)
        # Visualizer: visualization handler
        self.maxDepth = int(self.config['Visualizer']['MaxFunDepth'])
        self.visualizer = Visualizer(self.config)
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
        if self.parser.Method == 'BP':
            # reset visualization server
            self.visualizer.sendReset()
            self.visualizer.sendEventType(list(self.parser.eventType.values()), 0)
            self.visualizer.sendFunMap(self.parser.getFunMap(), 0)

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
            self.log.info("Frame has no function data...")
            return
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

    def _process_counter_data(self):
        try:
            countData = self.parser.getCountData()
        except AssertionError:
            self.log.info("Frame has no counter data...")
            return

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
            self.log.info("Frame has no communication data...")
            return

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
            return [], []

        outliers_id_str = []
        funOfInt = []
        for funid, data in functime.items():
            data = np.array(data)
            n_data = len(data)

            self.outlier.compOutlier(data, funid)
            outliers = self.outlier.getOutlier()
            outliers_id = data[outliers==-1, -1]

            outliers_id_str += np.array(outliers_id, dtype=np.str).tolist()

            #print('outliers', funid, len(outliers_id))

            maxFuncDepth = self.event.getMaxFunDepth()

            # FIXME: need smooth handling for unknown function and event type
            # NOTE: sometime, `funid` doesn't exist in funcMap. This time, it will add
            # NOTE: an entry, `funid` -> '__Unknown_function', and then return the value.
            # NOTE: At this time, key value must cast to int manually; otherwise it will cause
            # NOTE: runtime error when we dump the information into json file.
            if n_data > np.sum(outliers) and maxFuncDepth[funid] < self.maxDepth:
                funOfInt.append(str(funMap[int(funid)]))

            if len(outliers_id) > 0:
                self.anomFun[int(funid)] += 1

        return outliers_id_str, funOfInt

    def _dump_trace_data(self):
        if self.parser.Method == "BP":
            pass
        else:
            raise NotImplementedError("Unsupported parser method: %s" % self.parser.Method)

    def process(self):
        # check current status of the parser
        self.status = self.parser.getStatus() >= 0
        if not self.status: return
        self.log.info("\n\nFrame: %s" % self.parser.getStatus())

        # Initialize event
        self._init_event()
        funMap = self.parser.getFunMap()

        # process on function event
        self._process_func_data()
        if not self.status: return

        # detect anomalies in function call data
        outlId, funOfInt = self._run_anomaly_detection(funMap)
        self.n_outliers += len(outlId)
        self.log.info("Numer of outliers per frame: %s" % len(outlId))

        # process on counter event
        self._process_counter_data()
        if not self.status: return

        # process on communication event
        self._process_communication_data()
        if not self.status: return

        # dump trace data for visualization
        self.visualizer.sendData(
            self.event.getFunData().tolist(),
            self.event.getCountData().tolist(),
            self.event.getCommData().tolist(),
            funOfInt,
            outlId,
            frame_id=self.parser.getStatus(),
            funMap=None if self.parser.Method == 'BP' else funMap,
            eventType=None if self.parser.Method == 'BP' else list(self.parser.getEventType().values())
        )

        # go to next stream
        self.parser.getStream()

    def finalize(self):
        self.log.info("\n\nFinalize:")
        stack_size = self.event.getFunStackSize()
        if stack_size > 0:
            self.log.info("Function stack is not empty: %s" % stack_size)
            self.log.info("Possible call stack violation!")

        self.log.info("Total number of events: %s" % self.event_id)
        self.log.info("Total number of outliers: %s" % self.n_outliers)

        self.parser.adiosFinalize()
        self.event.clearFunTime()
        self.event.clearFunData()
        self.event.clearCountData()
        self.event.clearCommData()


def usage():
    pass

if __name__ == '__main__':
    import time
    # check argument and print usages()

    #configFile = './test/test.cfg' #sys.argv[1]
    configFile = sys.argv[1]
    driver = Chimbuko(configFile)
    n_frames = 0

    start = time.time()
    while driver.status:
        driver.process()
        n_frames+=1
    driver.finalize()
    end = time.time()

    print("Total number of frames: %s" % n_frames)
    print("Total running time: {}s".format(end - start))