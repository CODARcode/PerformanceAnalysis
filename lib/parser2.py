"""@package Chimbuko
This is the parser module currently capable of parsing Adios generated .bp files and
stream through Dataspaces.

Authos(s):
    Sungsoo Ha (sungsooha@bnl.gov)

Created:
    March, 2019

"""
import numpy as np
import copy
import pyAdios as ADIOS
from collections import defaultdict


class Parser(object):
    """Parser class

    provides an interface to Adios and is currently capable of parsing .bp files
    through the stream variable which serves as the Adios handle.

    In addition the class contains methods for extracting specific events from the Adios stream.
    """
    def __init__(self, config, log=None, rank=-1, comm=None):
        """
        The parser has two main modes:
                BP mode: offline/batch processing
            non-BP mode: streaming via one of the Adios streaming methods
                            (e.g. DATASPACES or FLEXPATH)
        Args:
            configFile: (str), the configuration files's name.
            log: logger
            rank: MPI rank, if rank=-1, it is not MPI run.
        """
        self.log = log
        self.rank = rank
        self.comm = comm


        # Initialize parser mode
        self.parseMode = config['Parser']['ParseMode']

        # Initialize Adios variables
        self.Method = config['Adios']['Method']
        self.Parameters = config['Adios']['Parameters']
        self.inputFile = config['Adios']['InputFile']
        self.ad = None
        self.stream = None
        self.status = -1

        if self.rank >= 0:
            # insert rank number into the inputFile
            pos = self.inputFile.rfind('.')
            if pos < 0:
                raise ValueError("[{:d}] Invalid input file name for the parser: {}".format(
                    self.rank, self.inputFile
                ))
            self.inputFile = self.inputFile[:pos] + "-{:d}".format(self.rank) + self.inputFile[pos:]

        if self.log is not None:
            self.log.info('[{:d}] Input File: {}, {}'.format(self.rank, self.inputFile, self.Method))


        # attributes from inputFile (BP)
        self.bpAttrib = None
        self.bpNumAttrib = None

        # TAU information
        # NOTE: with unknonw function id or event id, it will return '__Unknown_function'
        # NOTE: and '__Unknown_event', respectively.
        self.numFun = 0                    # the number of functions
        self.funMap = defaultdict(lambda: '__Unknown_function')     # function hash map
        self.eventType = defaultdict(lambda: '__Unknown_event')  # eventType hash map

        if self.parseMode == "Adios":
            self.ad = ADIOS.pyAdios(self.Method, self.Parameters)
            self.ad.open(self.inputFile, ADIOS.OpenMode.READ, self.comm)
            self.status = self.ad.current_step()
            self._update()
        else:
            if self.log is not None:
                self.log.error("[{:d}] Unsupported parse mode: {}".format(
                    self.rank, self.parseMode))
            raise ValueError("[{:d}] Unsupported parse mode: {}".format(self.rank, self.parseMode))

    def _update(self):
        self.bpAttrib = self.ad.available_attributes()
        self.bpNumAttrib = len(self.bpAttrib)

        self.funMap.clear()
        self.eventType.clear()
        for attr_name, data in self.bpAttrib.items():
            is_func = attr_name.startswith('timer')
            is_event = attr_name.startswith('event_type')
            if is_func or is_event:
                key = int(attr_name.split()[1])
                val = str(data.value())
                if is_func:
                    self.funMap[key] = val
                else:
                    self.eventType[key] = val
        self.numFun = len(self.funMap)
        if self.log is not None:
            self.log.debug("[{:d}] Number of attributes: {}".format(self.rank, self.bpNumAttrib))
        if self.log is not None:
            self.log.debug("Attribute names: \n" + str(self.bpAttrib.keys()))
        if self.log is not None: self.log.debug("Number of functions: %s" % self.numFun)
        if self.log is not None: self.log.debug("Function map: \n" + str(self.funMap))
        if self.log is not None: self.log.debug("Event type: \n" + str(self.eventType))

    def getParseMethod(self):
        """Get method for accessing parse method.

        Args:
            No arguments.

        Returns:
            Reference to self.Method (string).
        """
        return self.Method

    def getStream(self):
        """Get method for accessing Adios handle.

        If it is streamming mode (e.g. SST), it needs to update unlike BP mode which
        contains all information from the beginning.

        Returns:
            Reference to self.stream, which is essentially the return pyAdios object
        """
        self.status = self.ad.advance()
        if self.Method in ['SST']:
            self._update()
        if self.log is not None:
            self.log.info("[{:d}] Adios stream status: {}".format(self.rank, self.status))
        return self.ad

    def getFunData(self):
        """
        Get method for accessing function call data, i.e. Adios variable "event_timestamps".

        Returns:
            Return value to Adios variable read() method.
        """
        ydim = self.ad.read_variable("timer_event_count")
        assert ydim is not None, "[{:d}] Frame has no `timer_event_count`!".format(self.rank)

        if isinstance(ydim, np.ndarray):
            ydim = ydim[0]

        #data = self.ad.read_variable("event_timestamps")
        data = self.ad.read_variable("event_timestamps", count=[ydim, 6])
        assert ydim == data.shape[0], \
            "[{:d}] Frame has unexpected number of events!".format(self.rank)
        # assert data is not None, "[{:d}] Frame has no `event_timestamps`!".format(self.rank)

        assert data.shape[0] > 0, "[{:d}] Function call data dimension is zero!".format(self.rank)

        # validation
        self.log.info('[{:d}][timer] {}'.format(self.rank, data.shape))
        pid = data[:, 0]
        if not (pid < 1).all():
            self.log.info('[{:d}][timer] contain invalid program index!'.format(self.rank))
            self.log.info('{}:{}, {}, {}'.format(ydim, len(pid), np.nonzero(pid>=1), pid[pid>=1]))
            self.log.info(data.dtype)

        rid = data[:, 1]
        if not (rid < 2).all():
            self.log.info('[{:d}][timer] contain invalid rank index!'.format(self.rank))
            self.log.info('{}:{}, {}, {}'.format(ydim, len(rid), np.nonzero(rid>=2), rid[rid>=2]))

        tid = data[:, 2]
        if not (tid < 128).all():
            self.log.info('[{:d}][timer] contain invalid thread index!'.format(self.rank))
            self.log.info('{}:{}, {}, {}'.format(ydim, len(tid), np.nonzero(tid>=128), tid[tid>=128]))

        eid = data[:, 3]
        if not (eid < 4).all():
            self.log.info('[{:d}][timer] contain invalid event index!'.format(self.rank))
            self.log.info('{}:{}, {}, {}'.format(ydim, len(eid), np.nonzero(eid>=4), eid[eid>=4]))

        #return copy.deepcopy(data)
        return data

    def getCountData(self):
        """

        Get method for accessing function call data, i.e. Adios variable "counter_values".

        The variable is defined as
            ad.define_var(g, "counter_values", "", ad.DATATYPE.unsigned_long,
                "counter_event_count,6", "counter_event_count,6", "0,0") in adios 1.x

        Returns:
            Return value to Adios variable read() method.
        """
        ydim = self.ad.read_variable("counter_event_count")
        assert ydim is not None, "[{:d}] Frame has no `counter_event_count`!".format(self.rank)

        if isinstance(ydim, np.ndarray):
            ydim = ydim[0]

        # data = self.ad.read_variable("counter_values")
        data = self.ad.read_variable("counter_values", count=[ydim, 6])
        assert ydim == data.shape[0], \
            "[{:d}] Frame has unexpected number of counters!".format(self.rank)
        # assert data is not None, "[{:d}] Frame has no `counter_values`!".format(self.rank)

        assert data.shape[0] > 0, "[{:d}] Counter data dimension is zero!".format(self.rank)
        # if self.log is not None:
        #     self.log.info("[{:d}] Frame has `counter_values: {}`".format(self.rank, data.shape))

        return data

    def getCommData(self):
        """

        Get method for accessing function call data, i.e. Adios variable "comm_timestamps".

        Returns:
            Return value to Adios variable read() method.
        """
        ydim = self.ad.read_variable("comm_count")
        assert ydim is not None, "[{:d}] Frame has no `comm_count`!".format(self.rank)

        if isinstance(ydim, np.ndarray):
            ydim = ydim[0]

        # data = self.ad.read_variable("comm_timestamps")
        data = self.ad.read_variable("comm_timestamps", count=[ydim, 8])
        assert ydim == data.shape[0], \
            "[{:d}] Frame has unexpected number of comm!".format(self.rank)
        # assert data is not None, "[{:d}] Frame has no `comm_timestamps`!".format(self.rank)

        assert data.shape[0] > 0, "[{:d}] Communication data dimension is zero!".format(self.rank)

        # validation
        pid = data[:, 0]
        if not (pid < 1).all():
            self.log.info('[{:d}][comm] contain invalid program index!'.format(self.rank))

        rid = data[:, 1]
        if not (rid < 2).all():
            self.log.info('[{:d}][comm] contain invalid rank index!'.format(self.rank))

        tid = data[:, 2]
        if not (tid < 128).all():
            self.log.info('[{:d}][comm] contain invalid thread index!'.format(self.rank))

        eid = data[:, 3]
        if not (eid < 4).all():
            self.log.info('[{:d}][comm] contain invalid event index!'.format(self.rank))

        return data

    def getStatus(self):
        """
        Get method for accessing return value of Adios advance() method".

        Returns:
            Reference to self.status (i.e. return value of Adios advance() method).
        """
        return self.status

    def getBpAttrib(self):
        """
        Get method for accessing Adios attributes,
            -     BP:  we have all the information in advance
            - non-BP:  we need to generate the info for every frame.

        Args:
            No arguments.

        Returns:
            Reference to self.bpAttrib (dictionary) i.e. return value of Adios self.stream.attr.
        """
        return self.bpAttrib

    def getBpNumAttrib(self):
        """
        Get method for accessing the number of Adios attributes".

        Returns:
            Reference to self.bpNumAttrib (int) i.e. the number of attributes.
        """
        return self.bpNumAttrib

    def getNumFun(self):
        """
        Get method for accessing the number of different functions".

        Returns:
            Reference to self.numFun (int) i.e. the number of different functions.
        """
        return self.numFun

    def getFunMap(self):
        """
        Get method for accessing the function map which is a dictionary,
            key: function id
            value: function name

        Returns:
            Reference to self.funMap (dictionary).
        """
        return self.funMap

    def getEventType(self):
        """
        Get method for accessing a dictionary of event types,
            key: event id
            value: event type (e.g. EXIT, ENTRY, SEND, RECEIVE)

        Returns:
            Reference to self.eventType (dictionary).
        """
        return self.eventType

    def adiosClose(self):
        """
        Close Adios stream
        """
        if self.log is not None:
            self.log.info("[{:d}] Closing Adios stream...".format(self.rank))
        self.ad.close()

    def adiosFinalize(self):
        """
        Call Adios finalize method

        Currently, this has same effect as adiosClose
        """
        if self.log is not None:
            self.log.info("[{:d}] Finalize Adios method: {}".format(self.rank, self.Method))
        self.ad.close()