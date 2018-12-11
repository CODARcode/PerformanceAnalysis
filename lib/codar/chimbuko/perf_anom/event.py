"""@package Chimbuko
This module processes events such as function calls, communication and processor counters 
Author(s): 
    Gyorgy Matyasfalvi (gmatyasfalvi@bnl.gov)
Created: 
    August, 2018
"""

from collections import deque
from collections import defaultdict
import configparser
import numpy as np

class  Event():
    """Event class keeps track of event information such as function call stack (funStack) and 
    function execution times (funtime).
    In addition the class contains methods for processing events and event related information.
    
    Attributes:
        Each event is associated with:: 
            A work flow component or program, indicated by p
            A node or MPI rank indicated by r
            A thread indicated by t 
    """
    
    def __init__(self, configFile):
      """This is the constructor for the Event class.
      
      Args:
          configFile (string): The configuration files's name.
          
      Returns:
          No return value.
      """
          
      self.funStack = {} # A dictionary of function calls; keys: program, mpi rank, thread
      self.funMap = None # If set, this is a dictionary of function ids; key: function id (int) value function name (string)
      self.funtime = {} # A result dictionary containing a list for each function id which has the execution time; key: function id, 
      #self.funList = None # or initialize to [] # For sending events to the viz server that have already exited
      self.maxFunDepth = defaultdict(int)
      self.entry = None
      self.exit = None 
      self.stackSize = 0
      self.funData = None
      #self.funDataTemp = None # For sending events to the viz server that have already exited
      self.countData = None
      self.commData = None
      self.fidx = 0
      self.ctidx = 0
      self.coidx = 0
    
    
    def setEventType(self, eventType):
        """Sets entry and exit variables.
        
        Args:
            eventType (list): where the index corresponding to the ENTRY/EXIT entries is the respective id for that event
                              i.e. if we have a list: ['ENTRY', 'EXIT'] then entry events have id 0 and exit id 1  
            
        Returns:
            No return value.
        """
        try:
            self.entry = eventType.index('ENTRY')
            self.exit = eventType.index('EXIT')
        except ValueError:
            print("ENTRY or EXIT event not present...")
        
        
    def setFunMap(self, funMap):
        """Sets funMap variable.
        
        Args:
            funMap (dictionary): a dictionary where key (int) is the function id and value (string) is the function name
            
        Returns:
            No return value.
        """
        self.funMap = funMap
              
    
    def createCallStack(self, p, r, t): 
      """Creates a stack for each p,r,t combination.
      
      Args:
          p (int): program id
          r (int): rank id
          t (int): thread id
      
      Returns:
          No return value.
      """
      
      if p not in self.funStack:
        self.funStack[p] = {}
      if r not in self.funStack[p]:
        self.funStack[p][r] = {}
      if t not in self.funStack[p][r]:
        self.funStack[p][r][t] = deque()
   
    
    
    def addFun(self, event): 
      """Adds function call to call stack
      
      Args:
          event (numpy array of ints): [program, mpi rank, thread, entry/exit, function id, timestamp, event id], obtained from the adios module's "event_timestamps" variable
          
      Returns:
          bool: True, if the event was successfully added to the call stack without any violations (including timestamp i.e. exit event's timestamp > entry event's timestamp), False otherwise.  
      """
      self.createCallStack(event[0],event[1],event[2]) # Make sure stack corresponding to (program, mpi rank, thread) is created
      
      if event[3] == self.entry:
        self.funStack[event[0]][event[1]][event[2]].append(event) # If entry event, add event to call stack
        self.funData[self.fidx,0:5] = event[0:5]
        self.funData[self.fidx,11:13] = event[5:7]
        self.fidx += 1
        return True
      elif event[3] == self.exit:
        pevent = self.funStack[event[0]][event[1]][event[2]].pop() # If exit event, remove corresponding entry event from call stack
        if pevent[4] != event[4] or pevent[5] > event[5]:
          print("entry event:", pevent)
          print("exit event:", event)
          #raise Exception("\nCall Stack Violation!\n") TODO remove
          return False
        self.funData[self.fidx,0:5] = event[0:5]
        self.funData[self.fidx,11:13] = event[5:7]
        self.fidx += 1
        if event[4] not in self.maxFunDepth:  
            self.maxFunDepth[event[4]] = len(self.funStack[event[0]][event[1]][event[2]])
        else:
            if len(self.funStack[event[0]][event[1]][event[2]]) > self.maxFunDepth[event[4]]:
                self.maxFunDepth[event[4]] = len(self.funStack[event[0]][event[1]][event[2]])
        
        # TODO I think the first if condition can be removed since we're computing anomalies for all functions        
        # if pevent[4] in self.funmap: # If event corresponds to a function call of interest compute execution time
        if pevent[4] not in self.funtime:
            self.funtime[pevent[4]] = [] # If function id is new to results dictionary create list 
        self.funtime[pevent[4]].append([event[0], event[1], event[2], event[4], pevent[5], event[5] - pevent[5], pevent[6]])
          #This portion is only needed if the visualization requires to send function calls that have exited
          #self.funList.append(pevent)
          #self.funList.append(event)
        return True
      else:
        return True # Event is not an exit or entry event
    
    #This portion is only needed if the visualization requires to send function calls that have exited
    #def initFunData(self, numEvent):
    #    self.funList = []
        
    def initFunData(self, numEvent):
        """Initialize numpy array funData to store function calls in format required by visualization.
        
        Args:
            numEvent (int): number of function calls that will be required to store
            
        Returns:
            No return value.
        """
        self.funData = np.full((numEvent, 13), np.nan)
        
    def getFunData(self):
        """Get method for funData.
        
        Args:
            No arguments.
            
        Returns:
            Reference to funData or empty list.
        """
        try:
            assert(self.funData is not None)
            return self.funData
        except:
            return [] 
        #This portion is only needed if the visualization requires to send function calls that have exited
        #self.funList.sort(key=lambda x: x[6])
        #self.funData = np.full((len(self.funList), 13), np.nan)
        #self.funDataTemp = np.array(self.funList)
        #self.funData[:,0:5] = self.funDataTemp[:,0:5]
        #self.funData[:,11:13] = self.funDataTemp[:,5:7]
        
    def clearFunData(self):
        """Clear funData created by initFunData() method.
        
        Args:
            No arguments.
            
        Returns:
            No return value.
        """
        self.fidx = 0
        try:
            assert(self.funData is not None)
            del self.funData
            self.funData = None
        except:
            self.funData = None
        #This portion is only needed if the visualization requires to send function calls that have exited
        #self.funList.clear()
        #if self.funDataTemp is None:
        #    pass
        #else:
        #    del self.funDataTemp
        
    def addCount(self, event):
        """Add counter data and store in format required by visualization.
        
        Args:
            event (numpy array of int-s): [program, mpi rank, thread, counter id, counter value, timestamp, event id].
            
        Returns:
            bool: True, if the event was successfully added, False otherwise.
        """       
        self.countData[self.ctidx,0:3] = event[0:3]
        self.countData[self.ctidx,5:7] = event[3:5]
        self.countData[self.ctidx,11:13] = event[5:7]
        self.ctidx += 1
        return True
        
    def initCountData(self, numEvent):
        """Initialize numpy array countData to store counter values in format required by visualization.
        
        Args:
            numEvent (int): number of counter events that will be required to store
            
        Returns:
            No return value.
        """
        self.countData = np.full((numEvent, 13), np.nan)
    
    def getCountData(self):
        """Get method for countData.
        
        Args:
            No arguments.
            
        Returns:
            Reference to countData or empty list.
        """
        try:
            assert(self.countData is not None)
            return self.countData
        except:
            return [] 
    
    def clearCountData(self):
        """Clear countData created by initCountData() method.
        
        Args:
            No arguments.
            
        Returns:
            No return value.
        """
        self.ctidx = 0
        try:
            assert(self.countData is not None)
            del self.countData
            self.countData = None
        except:
            self.countData = None
        
        
    def addComm(self, event):
        """Add communication data and store in format required by visualization.
        
        Args:
            event (numpy array of int-s): [program, mpi rank, thread, tag, partner, num bytes, timestamp, event id].
            
        Returns:
            bool: True, if the event was successfully added, False otherwise.
        """          
        self.commData[self.coidx,0:4] = event[0:4]
        self.commData[self.coidx,8:11] = event[4:7]
        self.commData[self.coidx,11:13] = event[7:9]
        self.coidx += 1
        return True
        
    def initCommData(self, numEvent):
        """Initialize numpy array commData to store communication data in format required by visualization.
        
        Args:
            numEvent (int): number of communication events that will be required to store
            
        Returns:
            No return value.
        """
        self.commData = np.full((numEvent, 13), np.nan)
    
    def getCommData(self):
        """Get method for commData.
        
        Args:
            No arguments.
            
        Returns:
            Reference to commData or empty list.
        """
        try:
            assert(self.commData is not None)
            return self.commData
        except:
            return [] 
    
    def clearCommData(self):
        """Clear commData created by initCommData() method.
        
        Args:
            No arguments.
            
        Returns:
            No return value.
        """
        self.coidx = 0
        try:
            assert(self.commData is not None)
            del self.commData
            self.commData = None
        except:
            self.commData = None
    
    
    def getFunTime(self):
        """Get method for function exection time data.
        
        Args:
            No arguments.
            
        Returns:
            Reference to funtime or empty list.
        """
        assert(len(self.funtime) > 0), "Frame only contains open functions (Assertion)..."
        return self.funtime
  
    
    def clearFunTime(self):
        """Clear function execution time data.
        
        Args:
            No arguments.
            
        Returns:
            No return value.
        """
        self.funtime.clear()
    

    def getFunStack(self):
        """Get method for function call stack.
        
        Args:
            No arguments.
            
        Returns:
            Reference to funStack a dictionary of stacks (deque-s).
        """
        return self.funStack
    
    
    def countStackSize(self, d, s):
        """Given a dictionary of stacks/arrays/deques counts the total length of those stacks/arrays/deques.
        
        Args:
            d (dict): a dictionary of stacks/arrays/deques
            s (int): total length of stacks/arrays/deques in the dictionary
            
        Returns:
            No return value.
        """
        for k, v in d.items():
            if isinstance(v, dict):
                self.countStackSize(v,s)
            else:
                s += len(v)
    
    def getFunStackSize(self):
        """Counts the total number of function calls still in the stack.
        
        Args:
            No argumets.
            
        Returns:
            Integer, corresponding to the total number of function calls still in in the stack.
        """
        self.stackSize = 0
        self.countStackSize(self.funStack, self.stackSize)        
        return self.stackSize   

    def printFunStack(self):
        """Prints the function call stack.
        
        Args:
            No argumets.
            
        Returns:
            No return value.
        """
        print("self.funStack = ", self.funStack)


    def getMaxFunDepth(self):
        """Get method for the maximum depth of the function call stack.
        
        Args:
            No argumets.
            
        Returns:
            Integer.
        """
        return self.maxFunDepth
 
    