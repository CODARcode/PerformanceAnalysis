"""@package Chimbuko
This is the parser module currently capable of parsing Adios generated .bp files and
stream through Dataspaces.

Authos(s):
    Sungsoo Ha (sungsooha@bnl.gov)

Created:
    March, 2019

"""
import numpy as np
# PYTHONPATH coudl be set either /lib or /PerformanceAnalysis
try:
    import pyAdios as ADIOS
except ImportError:
    import lib.pyAdios as ADIOS

from collections import defaultdict


class Parser(object):
    """Parser class

    provides an interface to Adios and is currently capable of parsing .bp files
    through the stream variable which serves as the Adios handle.

    In addition the class contains methods for extracting specific events from the Adios stream.
    """
    def __init__(self, config, log=None):
        """
        The parser has two main modes:
                BP mode: offline/batch processing
            non-BP mode: streaming via one of the Adios streaming methods
                            (e.g. DATASPACES or FLEXPATH)
        Args:
            configFile: (str), the configuration files's name.
        """
        self.log = log

        # Initialize parser mode
        self.parseMode = config['Parser']['ParseMode']

        # Initialize Adios variables
        self.Method = config['Adios']['Method']
        self.Parameters = config['Adios']['Parameters']
        self.inputFile = config['Adios']['InputFile']
        self.ad = None
        self.stream = None
        self.status = -1

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
            self.ad.open(self.inputFile, ADIOS.OpenMode.READ)
            self.status = self.ad.current_step()
            self._update()
        else:
            if self.log is not None: self.log.error("Unsupported parse mode: %s" % self.parseMode)
            raise ValueError("Unsupported parse mode: %s" % self.parseMode)

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
        if self.log is not None: self.log.info("Number of attributes: %s" % self.bpNumAttrib)
        if self.log is not None: self.log.debug("Attribute names: \n" + str(self.bpAttrib.keys()))
        if self.log is not None: self.log.debug("Number of functions: %s" % self.numFun)
        if self.log is not None: self.log.debug("Function map: \n" + str(self.funMap))

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
        if self.log is not None: self.log.info("Adios stream status: %s" % self.status)
        return self.ad

    def getFunData(self):
        """
        Get method for accessing function call data, i.e. Adios variable "event_timestamps".

        The variable is defined as
            ad.define_var(g,
                "event_timestamps", "", ad.DATATYPE.unsigned_long,
                "timer_event_count,6", "timer_event_count,6", "0,0") in adios 1.x

        todo: after updating writer with adios2, all variables become self-describing!
        todo: don't require to fetch ydim!

        Returns:
            Return value to Adios variable read() method.
        """
        ydim = self.ad.read_variable("timer_event_count")
        assert ydim is not None, "Frame has no `timer_event_count`!"

        if isinstance(ydim, np.ndarray):
            ydim = ydim[0]

        data = self.ad.read_variable("event_timestamps", count=[ydim, 6])
        assert data is not None, "Frame has no `event_timestamps`!"

        assert data.shape[0] > 0, "Function call data dimension is zero!"
        if self.log is not None: self.log.info("Frame has `event_timestamps: {}`".format(data.shape))

        # todo: is this correct return data? (adios 1.x: var.read(nsteps=numSteps))
        # todo: does this return all data in all steps? what is numSteps?
        return data

    def getCountData(self):
        """

        Get method for accessing function call data, i.e. Adios variable "counter_values".

        The variable is defined as
            ad.define_var(g, "counter_values", "", ad.DATATYPE.unsigned_long,
                "counter_event_count,6", "counter_event_count,6", "0,0") in adios 1.x

        todo: after updating writer with adios2, all variables become self-describing!
        todo: don't require to fetch ydim!

        Returns:
            Return value to Adios variable read() method.
        """
        ydim = self.ad.read_variable("counter_event_count")
        assert ydim is not None, "Frame has no `counter_event_count`!"

        data = self.ad.read_variable("counter_values", count=[ydim, 6])
        assert data is not None, "Frame has no `counter_values`!"

        assert data.shape[0] > 0, "Counter data dimension is zero!"
        if self.log is not None: self.log.info("Frame has `counter_values: {}`".format(data.shape))

        # todo: is this correct return data? (adios 1.x: var.read(nsteps=numSteps))
        # todo: does this return all data in all steps? what is numSteps?
        return data

    def getCommData(self):
        """

        Get method for accessing function call data, i.e. Adios variable "comm_timestamps".

        The variable is defined as
            ad.define_var(g, "comm_timestamps", "", ad.DATATYPE.unsigned_long,
                "comm_count,8", "comm_count,8", "0,0") in adios 1.x

        todo: after updating writer with adios2, all variables become self-describing!
        todo: don't require to fetch ydim!

        Returns:
            Return value to Adios variable read() method.
        """
        ydim = self.ad.read_variable("comm_count")
        assert ydim is not None, "Frame has no `comm_count`!"

        data = self.ad.read_variable("comm_timestamps", count=[ydim, 8])
        assert data is not None, "Frame has no `comm_timestamps`!"

        assert data.shape[0] > 0, "Communication data dimension is zero!"
        if self.log is not None: self.log.info("Frame has `comm_timestamps: {}`".format(data.shape))

        # fixme: is this correct return data? (adios 1.x: var.read(nsteps=numSteps))
        # fixme: does this return all data in all steps? what is numSteps?
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
        if self.log is not None: self.log.info("Closing Adios stream...")
        self.ad.close()

    def adiosFinalize(self):
        """
        Call Adios finalize method

        Currently, this has same effect as adiosClose
        """
        if self.log is not None: self.log.info("Finalize Adios method: %s" % self.Method)
        self.ad.close()