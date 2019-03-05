"""@package Chimbuko
This is the parser module currently capable of parsing Adios generated .bp files and
stream through Dataspaces.

Authos(s):
    Sungsoo Ha (sungsooha@bnl.gov)

Created:
    March, 2019

"""
import configparser
import logging
import numpy as np
import lib.pyAdios as ADIOS
from collections import defaultdict




class Parser(object):
    """Parser class

    provides an interface to Adios and is currently capable of parsing .bp files
    through the stream variable which serves as the Adios handle.

    In addition the class contains methods for extracting specific events from the Adios stream.
    """

    def __init__(self, configFile):
        """
        The parser has two main modes:
                BP mode: offline/batch processing
            non-BP mode: streaming via one of the Adios streaming methods
                            (e.g. DATASPACES or FLEXPATH)
        Args:
            configFile: (str), the configuration files's name.
        """
        # Initialize configparser
        self.config = configparser.ConfigParser(interpolation=None)
        self.config.read(configFile)

        # Initialize logger
        self.logFile   = self.config['Debug']['LogFile']
        self.logFormat = self.config['Debug']['Format']
        self.logLevel  = self.config['Debug']['LogLevel']

        logging.basicConfig(level=self.logLevel, format=self.logFormat, filename=self.logFile)
        self.log = logging.getLogger('PARSER')

        # Initialize parser mode
        self.parseMode = self.config['Parser']['ParseMode']

        # Initialize Adios variables
        self.Method = self.config['Adios']['Method']
        self.Parameters = self.config['Adios']['Parameters']
        self.inputFile = self.config['Adios']['InputFile']
        self.ad = None
        self.stream = None
        self.status = -1

        # attributes from inputFile (BP)
        self.bpAttrib = None
        self.bpNumAttrib = None

        # TAU information (?)
        self.numFun = 0                    # the number of functions
        self.funMap = defaultdict(int)     # function hash map
        self.eventType = defaultdict(int)  # eventType hash map

        if self.parseMode == "Adios":
            self.ad = ADIOS.pyAdios(self.Method, self.Parameters)
            self.ad.open(self.inputFile, ADIOS.OpenMode.READ)

            if self.Method == "BP":
                # This part is needed because the visualization code requries
                # function map and event type before any actual trace data is sent
                # todo: bpAttrib data structure may be not incompatible from adios 1.x
                # todo: need to check the code where bpAttrib is utilized.
                self.bpAttrib = self.ad.available_attributes()
                self.bpNumAttrib = len(self.bpAttrib)
                for attr_name in self.bpAttrib:
                    is_func = attr_name.startswith('timer')
                    is_event = attr_name.startswith('event_type')
                    if is_func or is_event:
                        key = int(attr_name.split()[1])
                        val = str(self.bpAttrib[attr_name].value())
                        if is_func:
                            self.funMap[key] = val
                        else:
                            self.eventType[key] = val
                self.numFun = len(self.funMap)
                # INFO
                self.log.info("Adios using BP method...")
                # DEBUG
                self.log.info("Number of attributes: %s" % self.bpNumAttrib)
                self.log.debug("Attribute names: \n" + str(self.bpAttrib.keys()))
                self.log.debug("Number of functions: %s" % self.numFun)
                self.log.debug("Function map: \n" + str(self.funMap))
            else:
                msg = "Using non-BP method: %s" % self.Method
                self.log.info(msg)
                raise NotImplementedError(msg)
        else:
            msg = "Unsupported parse mode: %s" % self.parseMode
            self.log.error(msg)
            raise Exception(msg)

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

        Returns:
            Reference to self.stream, which is essentially the return pyAdios object
        """
        if self.Method == "BP":
            self.status = self.ad.advance()
        else:
            raise ValueError("Unsupported method: %s" % self.Method)
        # INFO
        self.log.info("Adios stream status: %s" % self.status)
        return self.ad

    def getFunData(self):
        """

        Get method for accessing function call data, i.e. Adios variable "event_timestamps".

        The variable is defined as
            ad.define_var(g,
                "event_timestamps", "", ad.DATATYPE.unsigned_long,
                "timer_event_count,6", "timer_event_count,6", "0,0") in adios 1.x

        Returns:
            Return value to Adios variable read() method.
        """
        ydim = self.ad.read_variable("timer_event_count")

        if ydim is None:
            self.log.debug("Frame has no `timer_event_count`!")
            return None
        data = self.ad.read_variable("event_timestamps", count=[ydim, 6])
        if data is None:
            self.log.debug("Frame has no `event_timestamps`!")
            return None

        # INFO
        assert data.shape[0] > 0, "Function call data dimension is zero!"
        self.log.info("Frame has `event_timestamps: {}`".format(data.shape))

        # todo: is this correct return data? (adios 1.x: var.read(nsteps=numSteps))
        # todo: does this return all data in all steps? what is numSteps?
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
        if ydim is None:
            self.log.debug("Frame has no `counter_event_count`!")
            return None

        data = self.ad.read_variable("counter_values", count=[ydim, 6])
        if data is None:
            self.log.debug("Frame has no `counter_values`!")
            return None

        # INFO
        assert data.shape[0] > 0, "Counter data dimension is zero!"
        self.log.info("Frame has `counter_values: {}`".format(data.shape))

        # todo: is this correct return data? (adios 1.x: var.read(nsteps=numSteps))
        # todo: does this return all data in all steps? what is numSteps?
        return data

    def getCommData(self):
        """

        Get method for accessing function call data, i.e. Adios variable "comm_timestamps".

        The variable is defined as
            ad.define_var(g, "comm_timestamps", "", ad.DATATYPE.unsigned_long,
                "comm_count,8", "comm_count,8", "0,0") in adios 1.x

        Returns:
            Return value to Adios variable read() method.
        """
        ydim = self.ad.read_variable("comm_count")
        if ydim is None:
            self.log.debug("Frame has no `comm_count`!")
            return None

        data = self.ad.read_variable("comm_timestamps", count=[ydim, 8])
        if data is None:
            self.log.debug("Frame has no `comm_timestamps`!")
            return None

        # INFO
        assert data.shape[0] > 0, "Communication data dimension is zero!"
        self.log.info("Frame has `comm_timestamps: {}`".format(data.shape))

        # todo: is this correct return data? (adios 1.x: var.read(nsteps=numSteps))
        # todo: does this return all data in all steps? what is numSteps?
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
        if self.Method == "BP":
            self.log.debug("getBpAttrib(): \n" + str(self.bpAttrib))
            return self.bpAttrib
        else:
            raise NotImplementedError("getBpAttrib: not implemented yet for non-BP")

    def getBpNumAttrib(self):
        """
        Get method for accessing the number of Adios attributes".

        Returns:
            Reference to self.bpNumAttrib (int) i.e. the number of attributes.
        """
        if self.Method == "BP":
            self.log.debug("getBpNumAttrib(): %s" % self.bpNumAttrib)
            return self.bpNumAttrib
        else:
            raise NotImplementedError("getBpNumAttrib: not implemented yet for non-BP")

    def getNumFun(self):
        """
        Get method for accessing the number of different functions".

        Returns:
            Reference to self.numFun (int) i.e. the number of different functions.
        """
        if self.Method == "BP":
            self.log.debug("getNumFun(): %s" % self.numFun)
            return self.numFun
        else:
            raise NotImplementedError("getNumFun: not implemented yet for non-BP")

    def getFunMap(self):
        """
        Get method for accessing the function map which is a dictionary,
            key: function id
            value: function name

        Returns:
            Reference to self.funMap (dictionary).
        """
        if self.Method == "BP":
            self.log.debug("getFunMap(): \n" + str(self.funMap))
            return self.funMap
        else:
            raise NotImplementedError("getFunMap: not implemented yet for non-BP")

    def getEventType(self):
        """
        Get method for accessing a dictionary of event types,
            key: event id
            value: event type (e.g. EXIT, ENTRY, SEND, RECEIVE)

        Returns:
            Reference to self.eventType (dictionary).
        """
        if self.Method == 'BP':
            self.log.debug("getEventType(): \n" + str(self.eventType))
            return self.eventType
        else:
            raise NotImplementedError("getEventType: not implemented yet for non-BP")

    def adiosClose(self):
        """
        Close Adios stream
        """
        self.log.info("Closing Adios stream...")
        self.ad.close()

    def adiosFinalize(self):
        """
        Call Adios finalize method

        Currently, this has same effect as adiosClose
        """
        self.log.info("Finalize Adios method: %s" % self.Method)
        self.ad.close()


if __name__ == '__main__':
    configFile = '../test/test.cfg'
    prs = Parser(configFile)
    ctrl = 0
    outct = 0

    while ctrl >= 0:

        # Info
        prs.log.info("\n\nFrame: " + str(outct))
        prs.getBpNumAttrib()
        prs.getBpAttrib()
        prs.getNumFun()
        prs.getFunMap()
        prs.getEventType()

        # Stream function call data
        funStream = None
        try:
            prs.log.info("Accessing frame: " + str(outct) + " function call data...")
            funStream = prs.getFunData()
        except:
            prs.log.info("No function call data in frame: " + str(outct) + "...")

        if funStream is not None:
            prs.log.info("Function stream contains {} events...".format(len(funStream)))

        # Stream counter data
        countStream = None
        try:
            prs.log.info("Accessing frame: " + str(outct) + " counter data...")
            countStream = prs.getCountData()
        except:
            prs.log.info("No counter data in frame: " + str(outct) + "...")

        if countStream is not None:
            prs.log.info("Counter stream contains {} events...".format(len(countStream)))

        # Stream communication data
        commStream = None
        try:
            prs.log.info("Accessing frame: " + str(outct) + " communication data...")
            commStream = prs.getCommData()
        except:
            prs.log.info("No communication data in frame: " + str(outct) + "...")

        if commStream is not None:
            prs.log.info("Communication stream contains {} events".format(len(commStream)))

        outct += 1
        prs.getStream()
        ctrl = prs.getStatus()

    prs.adiosClose()
    prs.adiosFinalize()
    prs.log.info("\nParser test done...\n")

    # import pprint
    #
    # #prs = Parser('../test/test.cfg')
    # ad = ADIOS.pyAdios("BP", "verbose=3")
    # ad.open("../data/shortdemo/data/tau-metrics.bp", ADIOS.OpenMode.READ)
    # # fh = adios2.open("../data/shortdemo/data/tau-metrics.bp")
    # status = ad.current_step()
    # while status >= 0:
    #     print(status)
    #     #print(ad.available_variables())
    #     ydim = ad.read_variable("timer_event_count")
    #     data = ad.read_variable("event_timestamps", count=[ydim, 6])
    #
    #     if ydim is None:
    #         break
    #
    #     print(ydim, data[-1])
    #     status = ad.advance()
    #
    # ad.close()
    #
    # # for fh_step in fh:
    # #     step = fh_step.current_step()
    # #     print(step)