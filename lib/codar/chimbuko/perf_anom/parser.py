"""@package Chimbuko
This is the parser module currently capable of parsing Adios generated .bp files and stream through Dataspaces
Author(s): 
    Gyorgy Matyasfalvi (gmatyasfalvi@bnl.gov)
Created: 
    August, 2018
"""

import configparser
import logging
import adios_mpi as ad
from collections import defaultdict

class Parser():
    """Parser class provides an interface to Adios and is currently capable of parsing .bp files  
    through the stream variable which serves as the Adios handle.
    In addition the class contains methods for extracting specific events from the Adios stream.
    """
    
    def __init__(self, configFile):
      """This is the constructor for the Parser class.
      The parser has two main modes one is the BP, which is equivalent to offline/batch processing and non-BP, which
      allows streaming via one of the Adios streaming methods such as DATASPACES or FLEXPATH.
      
      Args:
          configFile (string): The configuration files's name.
          
      Returns:
          No return value.
      """
      
      # Initialize configparser
      self.config = configparser.ConfigParser(interpolation=None)
      self.config.read(configFile)
      
      # Initialize logger
      self.logFile = self.config['Debug']['LogFile']
      self.logFormat = self.config['Debug']['Format']
      
      self.logLevel = 'NOTSET' 
      logging.basicConfig(level=self.logLevel, format=self.logFormat, filename=self.logFile)
      self.log = logging.getLogger('PARSER')
      
      # Initialize parser mode
      self.parseMode = self.config['Parser']['ParseMode']
      
      # Initialize Adios variables
      self.Method = self.config['Adios']['Method']
      self.Parameters = self.config['Adios']['Parameters']
      self.inputFile = self.config['Adios']['InputFile']
      self.stream = None
      self.status = None
      self.bpAttrib = None
      self.bpNumAttrib = None
      self.numFun = 0
      self.funMap = defaultdict(int)
      self.eventType = defaultdict(int)
      
      if self.parseMode == "Adios":
        ad.read_init(self.Method, parameters=self.Parameters)
        self.stream = ad.file(self.inputFile, self.Method, is_stream=True, timeout_sec=-1.0)
 
        # Info
        msg = "Adios handle initialized with method: " + self.Method
        self.log.info(msg)
        
        if self.Method == "BP":
            # This part is needed because the visualization code requires function map and event type before any actual trace data is sent.
            self.bpAttrib = self.stream.attr
            self.bpNumAttrib = self.stream.nattrs
            for iter in self.bpAttrib: # extract function names and ids
                if iter.startswith('timer'):
                    self.numFun = self.numFun + 1
                    self.funMap[int(iter.split()[1])] = str(self.bpAttrib[iter].value.decode("utf-8")) # if iter is a string "timer 123" separate timer and 123 and assign 123 as integer key to function map and the function name which is stored in self.bpAttrib[iter].value as a value  
                if iter.startswith('event_type'):
                    self.eventType[int(iter.split()[1])] = str(self.bpAttrib[iter].value.decode("utf-8"))  
            # Info
            msg = "Adios using BP method..."
            self.log.info(msg)              
            # Debug
            msg = "Number of attributes: " + str(self.bpNumAttrib)
            self.log.debug(msg)
            msg = "Attribute names: \n" + str(self.bpAttrib)
            self.log.debug(msg)
            msg = "Number of functions: " + str(self.numFun)
            self.log.debug(msg)
            msg = "Function map: \n" + str(self.funMap)
            self.log.debug(msg)   
        else:
            # Info 
            msg = "Adios using non BP method..."
            self.log.info(msg)   
      else:
          msg = "Parse mode not supported..."
          self.log.error(msg)
          raise Exception(msg)
                        
            
    def getStream(self):
        """Get method for accessing Adios handle.
        
        Args:
            No arguments.
            
        Returns:
            Reference to self.stream, which is essentially the return value of adios.file() method.
        """
        self.status = self.stream.advance(timeout_sec=-1.0) #timeout_sec=-1.0
        # Info
        msg = "Adios stream status: " + str(self.status)
        self.log.info(msg)
        return self.stream
    
    
    def getFunData(self):
        """Get method for accessing function call data, i.e. Adios variable "event_timestamps".
        
        Args:
            No arguments.
            
        Returns:
            Return value to Adios variable read() method.
        """
        assert("event_timestamps" in self.stream.vars), "Frame has no function call data (Assertion)..."
        # Info
        msg = "(event_timestamps in self.stream.vars) is True..."
        self.log.info(msg)
        assert(self.stream.vars["event_timestamps"].shape[0] > 0), "Function call data dimension is zero (Assertion)..."
        # Info
        msg = "(self.stream.vars['event_timestamps'].shape[0] > 0) is True"
        self.log.info(msg)
        var = self.stream.var["event_timestamps"]
        numSteps = var.nsteps
        return var.read(nsteps=numSteps)
    
    
    def getCountData(self):
        """Get method for accessing counter data, i.e. Adios variable "counter_values".
        
        Args:
            No arguments.
            
        Returns:
            Return value to Adios variable read() method.
        """
        assert("counter_values" in self.stream.vars), "Frame has no counter data (Assertion)..."
        # Info
        msg = "(counter_values in self.stream.vars) is True..."
        self.log.info(msg)
        assert(self.stream.vars["counter_values"].shape[0] > 0), "Counter data dimension is zero (Assertion)..."
        # Info
        msg = "(self.stream.vars['counter_values'].shape[0] > 0) is True..."
        self.log.info(msg)
        var = self.stream.var["counter_values"]
        num_steps = var.nsteps
        return var.read(nsteps=num_steps)
    

    def getCommData(self):
        """Get method for accessing communication data, i.e. Adios variable "comm_timestamps".
        
        Args:
            No arguments.
            
        Returns:
            Return value to Adios variable read() method.
        """
        assert("comm_timestamps" in self.stream.vars), "Frame has no communication data (Assertion)..."
        # Info
        msg = "(comm_timestamps in self.stream.vars) is True..."
        self.log.info(msg)
        assert(self.stream.vars["comm_timestamps"].shape[0] > 0), "Communication data dimension is zero (Assertion)..."
        # Info
        msg = "(self.stream.vars['comm_timestamps'].shape[0] > 0) is True"
        self.log.info(msg)
        var = self.stream.var["comm_timestamps"]
        num_steps = var.nsteps
        return var.read(nsteps=num_steps)
    
    
    def getStatus(self):
        """Get method for accessing return value of Adios advance() method".
        
        Args:
            No arguments.
            
        Returns:
            Reference to self.status (i.e. return value of Adios advance() method).
        """
        return self.status
    
    
    def getBpAttrib(self):
        """Get method for accessing Adios attributes, if using BP method we have all the information in advance 
        for the non-BP method we need to generate the info for every frame.
        
        Args:
            No arguments.
            
        Returns:        
            Reference to self.bpAttrib (i.e. return value of Adios self.stream.attr).
        """   
        if self.Method == "BP":
            # Debug
            msg = "getBpAttrib(): \n" + str(self.bpAttrib)
            self.log.debug(msg)
            return self.bpAttrib
        else:
            self.bpAttrib = self.stream.attr
            # Debug
            msg = "getBpAttrib(): \n" + str(self.bpAttrib)
            self.log.debug(msg)
            return self.bpAttrib
    
    
    def getBpNumAttrib(self):
        """Get method for accessing the number of Adios attributes".
        
        Args:
            No arguments.
            
        Returns:        
            Reference to self.bpNumAttrib (i.e. the number of attributes).
        """
        if self.Method == "BP":
            # Debug
            msg = "getBpNumAttrib(): " + str(self.bpNumAttrib)
            self.log.debug(msg)
            return self.bpNumAttrib
        else: 
            self.bpNumAttrib = self.stream.nattrs
            # Debug
            msg = "getBpNumAttrib(): " + str(self.bpNumAttrib)
            self.log.debug(msg)
            return self.bpNumAttrib   
    
    
    def getNumFun(self):
        """Get method for accessing the number of different functions".
        
        Args:
            No arguments.
            
        Returns:        
            Reference to self.numFun (i.e. the number of different functions).
        """
        if self.Method == "BP":
            # Debug
            msg = "getNumFun(): " + str(self.numFun)
            self.log.debug(msg)
            return self.numFun
        else:
            self.bpAttrib = self.stream.attr
            self.numFun = 0
            for iter in self.bpAttrib:
                if iter.startswith('timer'):
                    self.numFun += 1
            # Debug
            msg = "getNumFun(): " + str(self.numFun)
            self.log.debug(msg)
            return self.numFun
                        
    
    def getFunMap(self):
        """Get method for accessing the function map which is a dictionary, where the key is the function id and 
        the value is the function name".
        
        Args:
            No arguments.
            
        Returns:        
            Reference to self.funMap.
        """
        if self.Method == "BP":
            # Debug
            msg = "getFunMap(): \n" + str(self.funMap)
            self.log.debug(msg)
            return self.funMap
        else:
            self.bpAttrib = self.stream.attr
            for iter in self.bpAttrib: # extract function names and ids
                if iter.startswith('timer'):
                    self.funMap[int(iter.split()[1])] = str(self.bpAttrib[iter].value.decode("utf-8")) # if iter is a string "timer 123" separate timer and 123 and assign 123 as integer key to function map and the function name which is stored in self.bpAttrib[iter].value as a value  
            # Debug
            msg = "getFunMap(): \n" + str(self.funMap)
            self.log.debug(msg)
            return self.funMap

    
    def getEventType(self):
        """Get method for accessing a dictionary of event types, where the key is the event id and the value
        the event type e.g. EXIT, ENTRY, SEND, RECEIVE.
        
        Args:
            No arguments.
            
        Returns:        
            Reference to self.funMap.
        """
        if self.Method == 'BP':
            # Debug
            msg = "getEventType(): \n" + str(self.eventType)
            self.log.debug(msg)
            return self.eventType
        else:
            self.bpAttrib = self.stream.attr
            for iter in self.bpAttrib: # extract event types  
                if iter.startswith('event_type'):
                    self.eventType[int(iter.split()[1])] = str(self.bpAttrib[iter].value.decode("utf-8"))  
            # Debug
            msg = "getEventType(): \n" + str(self.eventType)
            self.log.debug(msg) 
            return self.eventType


    def adiosClose(self):
        """Close Adios stream
        
        Args:
            No arguments.
            
        Returns:        
            No return value.
        """
        # Info
        msg = "Closing Adios stream..."
        self.log.info(msg)
        self.stream.close()

    def adiosFinalize(self):
        """Call Adios finalize method
        
        Args:
            No arguments.
            
        Returns:        
            No return value.
        """        
        # Info
        msg = "Finalize Adios method " + self.Method
        self.log.info(msg)
        ad.read_finalize(self.Method)



# Run parser 
if __name__ == "__main__":
    
    import sys
    
    prs = Parser(sys.argv[1])
    ctrl = 0
    outct = 0
    
    while ctrl >= 0:

        # Info 
        msg = "\n\nFrame: " + str(outct)
        prs.log.info(msg)
        prs.getBpNumAttrib()
        prs.getBpAttrib()
        prs.getNumFun()
        prs.getFunMap()
        prs.getEventType()

        # Stream function call data
        try:
            # Info
            msg = "Accessing frame: " + str(outct) + " function call data..."
            prs.log.info(msg)
            funStream = prs.getFunData()
            funDataOK = True
        except:
            # Info
            msg = "No function call data in frame: " + str(outct) + "..."
            prs.log.info(msg)
            funDataOK = False
    
        if funDataOK:
            # Info
            msg = "Function stream contains " + str(len(funStream)) + " events..."
            prs.log.info(msg)
            
        # Stream counter data
        try:
            # Info
            msg = "Accessing frame: " + str(outct) + " counter data..."
            prs.log.info(msg)
            countStream = prs.getCountData()
            countDataOK = True
        except:
            # Info
            msg = "No counter data in frame: " + str(outct) + "..."
            prs.log.info(msg)
            countDataOK = False
        
        if countDataOK:
            # Info
            msg = "Counter stream contains " + str(len(countStream)) + " events..."
            prs.log.info(msg)
        
        # Stream communication data
        try:
            # Info
            msg = "Accessing frame: " + str(outct) + " communication data..."
            prs.log.info(msg)
            commStream = prs.getCommData()
            commDataOK = True
        except:
            # Info
            msg = "No communication data in frame: " + str(outct) + "..."
            prs.log.info(msg)
            commDataOK = False
        
        if commDataOK:
            # Info
            msg = "Communication stream contains " + str(len(countStream)) + " events..."
            prs.log.info(msg)
            
    
        outct += 1
        prs.getStream()
        ctrl = prs.getStatus()
        
    
    prs.adiosClose()
    prs.adiosFinalize()
    prs.log.info("\nParser test done...\n")