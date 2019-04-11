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
import time
import uuid
import numpy as np

from definition import *
from parser import Parser
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
        self.stop_loop = int(self.config['Debug']['StopLoop'])
        # to see logging on the terminal, uncomment below (for develop purpose)
        try:
            h = self.config['Debug']['AddHandler']
            if h == 'stdout':
                self.log.addHandler(logging.StreamHandler(sys.stdout))
        except KeyError:
            pass

        # Parser: data handler
        self.parser = Parser(self.config, self.log, self.rank, self.comm)

        # Event: event handler
        self.event = Event(self.log)
        # Outlier: outlier detector
        self.outlier = Outlier(self.config, self.log)
        # Visualizer: visualization handler
        self.maxDepth = int(self.config['Visualizer']['MaxFunDepth'])
        self.visualizer = Visualizer(self.config, self.log, self.rank)
        self._init()

        # init variable
        self.status = True
        self.n_outliers = 0
        self.n_funcalls = 0

        # measure average time performance
        self.t_funstack = 0.
        self.t_anomaly = 0.
        self.t_vis = 0.

    def _init(self):
        self._init_event(self.parser.Method == 'BP')
        if self.parser.Method == 'BP':
            # reset visualization server
            self.visualizer.sendReset()
            self.visualizer.sendEventType(list(self.parser.getEventType().values()), frame_id=0)
            self.visualizer.sendFunMap(self.parser.getFunMap(), frame_id=0)

    def _init_event(self, force_to_init=False):
        if force_to_init or self.parser.Method != 'BP':
            self.event.setEventType(list(self.parser.getEventType().values()))
            self.event.setFunMap(self.parser.getFunMap())
        self.event.clearFunTime()

    def _process_func_comm_data(self):
        """processing on  function and communication data and generate v2 data for visualization"""
        try:
            funcData = self.parser.getFunData()
        except AssertionError:
            self.log.info("[{:d}][V2] Frame has no function data...".format(self.rank))
            funcData = []

        try:
            commData = self.parser.getCommData()
        except AssertionError:
            self.log.info("[{:d}][V2] Frame has no communication data...".format(self.rank))
            commData = []

        n_funcData = len(funcData)
        n_commData = len(commData)
        idx_fun = 0
        idx_com = 0

        fun_data = funcData[idx_fun] if idx_fun < n_funcData else None
        com_data = commData[idx_com] if idx_com < n_commData else None

        ts_fun = fun_data[FUN_IDX_TIME] if fun_data is not None else np.inf
        ts_com = com_data[COM_IDX_TIME] if com_data is not None else np.inf

        # self.log.info("[{:d}][before] {}".format(self.rank, fun_data))
        while idx_fun < n_funcData or idx_com < n_commData:
            if ts_fun <= ts_com and fun_data is not None:
                if not self.event.addFun(fun_data, str(uuid.uuid4())):
                    self.status = False
                    self.log.info(
                        "\n ***** [{:d}] Call stack violation! *****\n".format(self.rank))
                    break
                idx_fun += 1

                fun_data = funcData[idx_fun] if idx_fun < n_funcData else None
                ts_fun = fun_data[FUN_IDX_TIME] if fun_data is not None else np.inf

            elif com_data is not None:
                if not self.event.addComm(com_data):
                    self.status = False
                    self.log.info(
                        "\n ***** [{:d}] Error in adding communication data event ***** \n".format(
                            self.rank))
                    break
                idx_com += 1

                com_data = commData[idx_com] if idx_com < n_commData else None
                ts_com = com_data[COM_IDX_TIME] if com_data is not None else np.inf
        # self.log.info("[{:d}][after] {}".format(self.rank, funcData[0]))

    def _run_anomaly_detection(self):
        """run anomaly detection with v2 data"""
        try:
            functime = self.event.getFunTime()
        except AssertionError:
            self.log.info("[{:d}][V2] Only contains open functions so no anomaly detection.".format(
                self.rank))
            return 0, 0

        self.outlier.initLocalStat()
        for funid, fcalls in functime.items():
            self.outlier.addLocalStat(fcalls, funid)
        self.outlier.pushLocalStat()

        n_outliers = 0
        n_funcalls = 0
        #outlier_stat = dict()
        for funid, fcalls in functime.items():
            n_funcalls += len(fcalls)
            self.outlier.compOutlier(fcalls, funid)
            outliers = self.outlier.getOutlier()

            _n_outliers = 0
            for fcall, label in zip(fcalls, outliers):
                fcall.set_label(label)
                if label == -1:
                    _n_outliers += 1

            n_outliers += _n_outliers
            if _n_outliers > 0:
                self.outlier.addAbnormal(funid, _n_outliers)
        return n_outliers, n_funcalls

    # def _process_counter_data(self):
    #     """(unused) processing on counter data and currently it is not used."""
    #     try:
    #         countData = self.parser.getCountData()
    #     except AssertionError:
    #         self.log.info("[{:d}] Frame has no counter data...".format(self.rank))
    #         return
    #
    #     self.event.initCountData(len(countData))
    #     for data in countData:
    #         if not self.event.addCount(np.append(data, np.uint64(self.event_id))):
    #             self.status = False
    #             self.log.info("\n\n\n[{:d}] Error in adding count data event!\n\n\n".format(self.rank))
    #             break
    #         self.event_id += 1

    def _process_for_viz(self):
            funtime = self.event.getFunTime()
            self.visualizer.sendData(
                execData=funtime,
                funMap=self.parser.getFunMap(),
                getStat=self.outlier.getStatViz,
                frame_id=self.parser.getStatus(),
            )

    def process(self):
        # check current status of the parser
        self.status = self.parser.getStatus() >= 0
        if not self.status: return
        if 1 <= self.stop_loop < self.parser.getStatus():
            self.log.info("[{}] Terminated with stop_loop condition!".format(self.rank))
            self.status = False
            return
        self.log.info("[{}] Frame: {}".format(self.rank, self.parser.getStatus()))

        # Initialize event
        self._init_event()

        # process on function event and communication event
        t_start = time.time()
        self._process_func_comm_data()
        if not self.status: return
        t_end = time.time()
        self.t_funstack += t_end - t_start

        # detect anomalies in function call data
        t_start = time.time()
        n_outliers, n_funcalls = self._run_anomaly_detection()
        t_end = time.time()
        self.n_outliers += n_outliers
        self.n_funcalls += n_funcalls
        self.log.info("[{:d}] Numer of outliers per frame: {}".format(self.rank, n_outliers))
        self.log.info("[{:d}] Number of function calls: {}".format(self.rank, n_funcalls))
        self.t_anomaly += t_end - t_start

        # process on counter event
        # NOTE: do we need to parse counter data that wasn't used anywhere.
        # self._process_counter_data()
        if not self.status: return
        self.log.info("[{}] End Frame: {}".format(self.rank, self.parser.getStatus()))

        # visualization
        if n_outliers > 0 or not self.visualizer.onlyAbnormal:
            t_start = time.time()
            self._process_for_viz()
            t_end = time.time()
            self.t_vis += t_end - t_start

        # go to next stream
        self.parser.getStream()

    def finalize(self):
        self.log.info("[{:d}] Finalize:".format(self.rank))
        stack_size = self.event.getFunStackSize()
        if stack_size > 0:
            self.log.info("[{:d}] Function stack is not empty: {}".format(self.rank, stack_size))
            self.log.info("[{:d}] Possible call stack violation!".format(self.rank))
            self.event.printFunStack()

        self.log.info("[{:d}] Total number of outliers: {}".format(self.rank, self.n_outliers))
        self.log.info("[{:d}] Total number of funcalls: {}".format(self.rank, self.n_funcalls))

        self.parser.adiosFinalize()
        self.event.clearFunTime()


def usage():
    pass

if __name__ == '__main__':
    from mpi4py import MPI

    # check argument and print usages()

    # MPI
    comm = MPI.COMM_WORLD
    rank = comm.Get_rank()
    size = comm.Get_size()

    if size == 1:
        rank = -1

    configFile = sys.argv[1]
    driver = Chimbuko(configFile, rank, MPI.COMM_SELF)

    n_frames = 0
    start = time.time()
    while driver.status:
        driver.process()
        if driver.status: n_frames+=1
    driver.finalize()
    end = time.time()

    driver.log.info("[{:d}] Total number of frames: {:d}".format(rank, n_frames))
    driver.log.info("[{:d}] Total running time: {}s".format(rank, end - start))
    driver.log.info("[{:d}] Avg. process  time: {}s".format(rank, (end - start)/n_frames))
    driver.log.info("[{:d}] Avg. funstack time: {}s".format(rank, driver.t_funstack/n_frames))
    driver.log.info("[{:d}] Avg. anomaly  time: {}s".format(rank, driver.t_anomaly/n_frames))
    driver.log.info("[{:d}] Avg. vis      time: {}s".format(rank, driver.t_vis/n_frames))

    # waiting until all data is sent to VIS
    driver.visualizer.join(True)
    driver.log.info("[{:d}] All data is sent to VIS!".format(rank))

    comm.Barrier()
    if rank == 0 or rank == -1:
        driver.outlier.shutdown_parameter_server()
        driver.log.info("[{:d}] request parameter server shutdown!!!".format(rank))