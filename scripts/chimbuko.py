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

from definition import *
from parser2 import Parser
from event import Event
from outlier import Outlier
from visualizer import Visualizer

class Chimbuko(object):
    def __init__(self, configFile:str, rank=-1, comm=None):
        self.config = configparser.ConfigParser(interpolation=None)
        self.config.read(configFile)

        self.rank = rank
        self.comm = comm

        # Initialize logger
        log_filename = self.config['Debug']['LogFile']
        if self.rank >= 0:
            # insert rank number into the log_filename
            pos = log_filename.rfind('.')
            if pos < 0:
                raise ValueError("[{:d}] Invalid log file name: {}".format(
                    self.rank, log_filename
                ))
            log_filename = log_filename[:pos] + "-{:d}".format(self.rank) + log_filename[pos:]

        logging.basicConfig(
            level=self.config['Debug']['LogLevel'],
            format=self.config['Debug']['Format'],
            filename=log_filename
        )
        self.log = logging.getLogger('CHIMBUKO')
        # to see logging on the terminal, uncomment below (for develop purpose)
        #self.log.addHandler(logging.StreamHandler(sys.stdout))

        # Parser: data handler
        self.parser = Parser(self.config, self.log, self.rank, self.comm)

        # Event: event handler
        self.event = Event(self.log)
        # Outlier: outlier detector
        self.outlier = Outlier(self.config, self.log)
        # Visualizer: visualization handler
        self.maxDepth = int(self.config['Visualizer']['MaxFunDepth'])
        self.visualizer = Visualizer(self.config, self.log)
        self._init()

        # init variable
        self.status = True
        # event id
        # note: maybe to generate unique id, is this okay with overflow?
        self.event_id = np.uint64(0)
        # total number of outliers (will be deprecated)
        self.n_outliers = 0
        # function data counter (will be deprecated)
        #self.func_counter = defaultdict(int)
        # count data counter (will be deprecated)
        #self.count_counter = defaultdict(int)
        # communication data counter (will be deprecated)
        #self.comm_counter = defaultdict(int)
        # anomaly function (will be deprecated)
        #self.anomFun = defaultdict(int)

        # FIXME: to fix the index bug from sos_flow
        self.fix_index = int(self.config['Basic']['FixIndex'])
        self.ver = self.config['Basic']['Ver']

        #print("FIX INDEX: ", self.fix_index)
        #print("VERSION: ", self.ver)

    # def _log(self, msg:str, type='info'):
    #     msg = "[{:d}]{:s}".format(self.rank, msg) if self.rank>=0 else msg
    #     if type == 'info':
    #         self.log.info(msg)
    #     elif type == 'debug':
    #         self.log.debug(msg)
    #     elif type == 'error':
    #         self.log.error(msg)

    def _init(self):
        self._init_event(self.parser.Method == 'BP')
        if self.parser.Method == 'BP':
            # reset visualization server
            self.visualizer.sendReset()
            self.visualizer.sendEventType(list(self.parser.getEventType().values()), 0)
            self.visualizer.sendFunMap(self.parser.getFunMap(), 0)

    def _init_event(self, force_to_init=False):
        if force_to_init or self.parser.Method != 'BP':
            self.event.setEventType(list(self.parser.getEventType().values()))
            self.event.setFunMap(self.parser.getFunMap())
        self.event.clearFunTime()
        self.event.clearFunData()
        self.event.clearCountData()
        self.event.clearCommData()

    def _process_func_data_v1(self):
        try:
            funcData = self.parser.getFunData()
        except AssertionError:
            self.log.info("[{:d}][V1] Frame has no function data...".format(self.rank))
            return

        # FIXME: this is added to handle index bug in sos_flow
        funcData[:, FUN_IDX_P] -= self.fix_index
        funcData[:, FUN_IDX_EE] -= self.fix_index
        funcData[:, FUN_IDX_FUNC] -= self.fix_index

        # Can I assume that event_timestamps data is always sorted in the order
        # of function call time
        self.event.initFunData(len(funcData))
        for data in funcData:
            if not self.event.addFun_v1(np.append(data, np.uint64(self.event_id))):
                self.status = False
                self.log.error(
                    "\n ***** [{:d}] Call stack violation! ***** \n".format(self.rank))
                break
            self.event_id += 1
            #self.func_counter[data[FUN_IDX_FUNC]] += 1

    def _process_func_data_v2(self):
        try:
            funcData = self.parser.getFunData()
        except AssertionError:
            self.log.info("[{:d}][V2] Frame has no function data...".format(self.rank))
            return

        # FIXME: this is added to handle index bug in sos_flow
        funcData[:, FUN_IDX_P] -= self.fix_index
        funcData[:, FUN_IDX_EE] -= self.fix_index
        funcData[:, FUN_IDX_FUNC] -= self.fix_index

        # Can I assume that event_timestamps data is always sorted in the order
        # of function call time
        for data in funcData:
            if not self.event.addFun_v2(data, self.event_id):
                self.status = False
                self.log.info(
                    "\n ***** [{:d}] Call stack violation! ***** \n".format(self.rank))
                break
            self.event_id += 1
            #self.func_counter[data[FUN_IDX_FUNC]] += 1

    def _process_communication_data_v1(self):
        try:
            commData = self.parser.getCommData()
        except AssertionError:
            self.log.info("[{:d}][V1] Frame has no communication data...".format(self.rank))
            return

        # FIXME: this is added to handle index bug in sos_flow
        commData[:, COM_IDX_P] -= self.fix_index
        commData[:, COM_IDX_SR] -= self.fix_index

        self.event.initCommData(len(commData))
        for data in commData:
            if not self.event.addComm_v1(np.append(data, np.uint64(self.event_id))):
                self.status = False
                self.log.info(
                    "\n\n\n [{:d}] Error in adding communication data event!\n\n\n".format(self.rank))
                break
            self.event_id += 1
            #self.comm_counter[data[COM_IDX_TAG]] += 1

    def _process_communication_data_v2(self):
        try:
            commData = self.parser.getCommData()
        except AssertionError:
            self.log.info("[{:d}][V2] Frame has no communication data...".format(self.rank))
            return

        # FIXME: this is added to handle index bug in sos_flow
        commData[:, COM_IDX_P] -= self.fix_index
        commData[:, COM_IDX_SR] -= self.fix_index

        self.event.initCommData(len(commData))
        for data in commData:
            if not self.event.addComm_v1(np.append(data, np.uint64(self.event_id))):
                self.status = False
                self.log.info(
                    "\n\n\n [{:d}] Error in adding communication data event!\n\n\n".format(
                        self.rank))
                break
            self.event_id += 1
            #self.comm_counter[data[COM_IDX_TAG]] += 1

    def _process_func_comm_data_v2(self):
        try:
            funcData = self.parser.getFunData()
            # FIXME: this is added to handle index bug in sos_flow
            funcData[:, FUN_IDX_P] -= self.fix_index
            funcData[:, FUN_IDX_EE] -= self.fix_index
            funcData[:, FUN_IDX_FUNC] -= self.fix_index
        except AssertionError:
            self.log.info("[{:d}][V2] Frame has no function data...".format(self.rank))
            funcData = []

        try:
            commData = self.parser.getCommData()
            # FIXME: this is added to handle index bug in sos_flow
            commData[:, COM_IDX_P] -= self.fix_index
            commData[:, COM_IDX_SR] -= self.fix_index
        except AssertionError:
            self.log.info("[{:d}][V2] Frame has no communication data...".format(self.rank))
            commData = []


        n_funcData = len(funcData)
        n_commData = len(commData)
        idx_fun = 0
        idx_com = 0

        while idx_fun < n_funcData or idx_com < n_commData:
            fun_data = funcData[idx_fun] if idx_fun < n_funcData else None
            com_data = commData[idx_com] if idx_com < n_commData else None

            ts_fun = fun_data[FUN_IDX_TIME] if fun_data is not None else np.inf
            ts_com = com_data[COM_IDX_TIME] if com_data is not None else np.inf

            if ts_fun <= ts_com and fun_data is not None:
                if not self.event.addFun_v2(fun_data, self.event_id):
                    self.status = False
                    self.log.info(
                        "\n ***** [{:d}] Call stack violation! *****\n".format(self.rank))
                    break
                self.event_id += 1
                #self.func_counter[fun_data[FUN_IDX_FUNC]] += 1
                idx_fun += 1
            elif com_data is not None:
                if not self.event.addComm_v2(com_data):
                    self.status = False
                    self.log.info(
                        "\n ***** [{:d}] Error in adding communication data event ***** \n".format(
                            self.rank))
                    break
                #self.comm_counter[com_data[COM_IDX_TAG]] += 1
                idx_com += 1


    def _run_anomaly_detection_v1(self, funMap):
        try:
            functime = self.event.getFunTime()
        except AssertionError:
            self.log.info("[{:d}][V1] Only contains open functions so no anomaly detection.".format(
                self.rank))
            return [], []

        outliers_id_str = []
        funOfInt = []
        for funid, data in functime.items():
            data = np.array(data)
            n_fcalls = len(data) # the number of function calls

            self.outlier.compOutlier_v1(data, funid)
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
            if n_fcalls > np.sum(outliers) and maxFuncDepth[funid] < self.maxDepth:
                funOfInt.append(str(funMap[int(funid)]))

            if len(outliers_id) > 0:
                #self.anomFun[int(funid)] += 1
                self.outlier.addAbnormal(funid, len(outliers_id))

        return outliers_id_str, funOfInt

    def _run_anomaly_detection_v2(self, funMap):
        try:
            functime = self.event.getFunTime()
        except AssertionError:
            self.log.info("[{:d}][V2] Only contains open functions so no anomaly detection.".format(
                self.rank))
            return [], []

        outliers_id_str = []
        funOfInt = []
        for funid, fcalls in functime.items():
            #execdata = np.array(data)
            n_fcalls = len(fcalls) # the number of function calls

            self.outlier.compOutlier_v2(fcalls, funid)
            outliers = self.outlier.getOutlier()

            outliers_id = []
            for fcall, label in zip(fcalls, outliers):
                fcall.set_label(label)
                if label == -1:
                    outliers_id.append(fcall.get_id())

            outliers_id_str += np.array(outliers_id, dtype=np.str).tolist()

            maxFuncDepth = self.event.getMaxFunDepth()

            # FIXME: need smooth handling for unknown function and event type
            # NOTE: sometime, `funid` doesn't exist in funcMap. This time, it will add
            # NOTE: an entry, `funid` -> '__Unknown_function', and then return the value.
            # NOTE: At this time, key value must cast to int manually; otherwise it will cause
            # NOTE: runtime error when we dump the information into json file.
            if n_fcalls > np.sum(outliers) and maxFuncDepth[funid] < self.maxDepth:
                funOfInt.append(str(funMap[int(funid)]))

            if len(outliers_id) > 0:
                self.outlier.addAbnormal(funid, len(outliers_id))
                #self.anomFun[int(funid)] += len(outliers_id)


        return outliers_id_str, funOfInt

    def _process_counter_data(self):
        try:
            countData = self.parser.getCountData()
        except AssertionError:
            self.log.info("[{:d}] Frame has no counter data...".format(self.rank))
            return

        # FIXME: this is added to handle index bug in sos_flow
        countData[:, CNT_IDX_P] -= self.fix_index
        countData[:, CNT_IDX_COUNT] -= self.fix_index

        self.event.initCountData(len(countData))
        for data in countData:
            if not self.event.addCount(np.append(data, np.uint64(self.event_id))):
                self.status = False
                self.log.info("\n\n\n[{:d}] Error in adding count data event!\n\n\n".format(self.rank))
                break
            self.event_id += 1
            #self.count_counter[data[CNT_IDX_COUNT]] += 1


    def process(self):
        # check current status of the parser
        self.status = self.parser.getStatus() >= 0
        if not self.status: return
        self.log.info("\n\n")
        self.log.info("[{}] Frame: {}".format(self.rank, self.parser.getStatus()))

        # Initialize event
        self._init_event()
        funMap = self.parser.getFunMap()

        # process on function event and communication event
        if self.ver == 'v1':
            self._process_func_data_v1()
            self._process_communication_data_v1()
        else:
            # self._process_func_data_v2()
            # self._process_communication_data_v2()
            self._process_func_comm_data_v2()
        if not self.status: return

        # detect anomalies in function call data
        if self.ver == 'v1':
            outlId, funOfInt = self._run_anomaly_detection_v1(funMap)
        else:
            outlId, funOfInt = self._run_anomaly_detection_v2(funMap)
        self.n_outliers += len(outlId)
        self.log.info("[{:d}] Numer of outliers per frame: {}".format(self.rank, len(outlId)))

        # process on counter event
        # FIXME: do we need to parse counter data that wasn't used anywhere.
        self._process_counter_data()
        if not self.status: return
        self.log.info("[{}] End Frame: {}".format(self.rank, self.parser.getStatus()))

        if self.ver == 'v1':
            self.visualizer.sendData_v1(
                self.event.getFunData().tolist(),
                self.event.getCountData().tolist(),
                self.event.getCommData().tolist(),
                funOfInt,
                outlId,
                rank=self.rank,
                frame_id=self.parser.getStatus(),
                funMap=None if self.parser.Method == 'BP' else funMap,
                eventType=None if self.parser.Method == 'BP'
                               else list(self.parser.getEventType().values())
            )
        else:
            try:
                funtime = self.event.getFunTime()
                self.visualizer.sendData_v2(
                    execData=funtime,
                    funMap=funMap,
                    getCount=self.outlier.getCount,
                    frame_id=self.parser.getStatus(),
                    rank=self.rank
                )
            except AssertionError:
                pass

        # go to next stream
        self.parser.getStream()

    def finalize(self):
        self.log.info("\n\n [{:d}] Finalize:".format(self.rank))
        stack_size = self.event.getFunStackSize()
        if stack_size > 0:
            self.log.info("[{:d}] Function stack is not empty: {}".format(self.rank, stack_size))
            self.log.info("[{:d}] Possible call stack violation!".format(self.rank))
            self.event.printFunStack()

        self.log.info("[{:d}] Total number of events: {}".format(self.rank, self.event_id))
        self.log.info("[{:d}] Total number of outliers: {}".format(self.rank, self.n_outliers))

        self.parser.adiosFinalize()
        self.event.clearFunTime()
        self.event.clearFunData()
        self.event.clearCountData()
        self.event.clearCommData()


def usage():
    pass

if __name__ == '__main__':
    import time
    from mpi4py import MPI

    # check argument and print usages()

    # MPI
    comm = MPI.COMM_WORLD
    rank = comm.Get_rank()
    size = comm.Get_size()

    if size == 1:
        rank = -1

    #configFile = './test/test.cfg' #sys.argv[1]
    configFile = sys.argv[1]

    # currently, each rank will have its own anomaly detector, no communication among ranks
    driver = Chimbuko(configFile, rank, MPI.COMM_SELF)
    n_frames = 0

    start = time.time()
    while driver.status:
        driver.process()
        n_frames+=1
    driver.finalize()
    end = time.time()

    driver.log.info("[{:d}] Total number of frames: {:d}".format(rank, n_frames))
    driver.log.info("[{:d}] Total running time: {}s".format(rank, end - start))

    # waiting until all data is sent to VIS
    driver.visualizer.join(not driver.status, rank)
    driver.log.info("[{:d}] All data is sent to VIS!".format(rank))
