"""@package Chimbuko
This is the parser module currently capable of parsing Adios generated .bp files 
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
            self.bpAttrib = self.stream.attr
            self.bpNumAttrib = self.stream.nattrs
            for iter in self.bpAttrib: # extract function names and ids
                if iter.startswith('timer'):
                    self.numFun = self.numFun + 1
                    self.funMap[int(iter.split()[1])] = str(self.bpAttrib[iter].value.decode("utf-8")) # if iter is a string "timer 123" separate timer and 123 and assign 123 as integer key to function map and the function name which is stored in self.bpAttrib[iter].value as a value  
                if iter.startswith('event_type'):
                    self.eventType[int(iter.split()[1])] = str(self.bpAttrib[iter].value.decode("utf-8"))  
                        
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
            self.log.info("Adios using non BP method...")   
      else:
          msg = "Parse mode not supported..."
          self.log.error(msg)
          raise Exception(msg)
                        
            
    def getStream(self):
        strm = self.stream
        self.status = self.stream.advance() #timeout_sec=-1.0
        return strm
    
    def getFunData(self): # get function call data from adios
        assert("event_timestamps" in self.stream.vars), "Frame has no function call data (Assertion)..."
        var = self.stream.var["event_timestamps"]
        numSteps = var.nsteps
        return var.read(nsteps=numSteps)
    
    def getCountData(self): # get counter data from adios
        assert("counter_values" in self.stream.vars), "Frame has no counter data (Assertion)..."
        var = self.stream.var["counter_values"]
        num_steps = var.nsteps
        return var.read(nsteps=num_steps)

    def getCommData(self): # get comm data from adios
        assert("comm_timestamps" in self.stream.vars), "Frame has no communication data (Assertion)..."
        var = self.stream.var["comm_timestamps"]
        num_steps = var.nsteps
        return var.read(nsteps=num_steps)
    
    
    def getStatus(self):
        return self.status
    
    
    def getBpAttrib(self):
        return self.bpAttrib
    
    
    def getBpNumAttrib(self):
        return self.bpNumAttrib
    
    
    def getNumFun(self):
        return self.numFun
    
    
    def getFunMap(self):
        return self.funMap
    
    
    def getEventType(self):
        return self.eventType




# Run parser 
if __name__ == "__main__":
    
    import sys
    
    prs = Parser(sys.argv[1])
    ctrl = 0
    outct = 0
    
    while ctrl >= 0:
        print("\n\nFrame: ", outct)

        # Stream function call data
        try:
            funStream = prs.getFunData()
            funDataOK = True
        except:
            funDataOK = False
            print("\nFrame has no function data...\n")
    
        if funDataOK:
            print("Function stream:")
            ct = 0                          
            for i in funStream:
                ct += 1
            print("Number of function events: ", ct, "\n")
            
        # Stream counter data
        try:
            countStream = prs.getCountData()
            countDataOK = True
        except:
            countDataOK = False
            print("\nFrame has no counter data...\n")
        
        if countDataOK:
            print("Counter stream:")
            ct = 0                         
            for i in countStream:
                ct += 1
            print("Number of counter events: ", ct, "\n")
        
        # Stream communication data
        try:
            commStream = prs.getCommData()
            commDataOK = True
        except:
            commDataOK = False
            print("\nFrame has no comm data...\n")
        
        if commDataOK:
            print("Comm stream:")  
            ct = 0                       
            for i in commStream:
                ct += 1
            print("Number of communication events: ", ct, "\n")
            
        outct += 1
        prs.getStream()
        ctrl = prs.getStatus()
        
    
    
    
    
    
    
    
    
    