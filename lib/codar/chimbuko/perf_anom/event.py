"""
Keeps track of events such as function calls, communication and processor counters as streamed from adios 
Authors: Shinjae Yoo (sjyoo@bnl.gov), Gyorgy Matyasfalvi (gmatyasfalvi@bnl.gov)
Create: August, 2018
"""

from collections import deque
import configparser

class  Event():
    def __init__(self, funMap, configFile):
      self.funstack = {} # A dictionary of function calls; keys: program, mpi rank, thread
      self.funmap = funMap # A dictionary of function ids; key: function id
      self.funtime = {} # A result dictionary containing a list for each function id which has the execution time; key: function id, 
    
    
    def checkCallStack(self, p, r, t): # As events arrive build necessary data structure
      if p not in self.funstack:
        self.funstack[p] = {}
      if r not in self.funstack[p]:
        self.funstack[p][r] = {}
      if t not in self.funstack[p][r]:
        self.funstack[p][r][t] = deque()
   
    
    def addFun(self, event): # adds function call to call stack
      # Input event array is as obtained from the adios module's "event_timestamps" variable i.e.: 
      # [program, mpi rank, thread, entry/exit, function id, timestamp]
      self.checkCallStack(event[0],event[1],event[2]) # Make sure program, mpi rank, thread data structure is built
      
      if event[3] == 0:
        self.funstack[event[0]][event[1]][event[2]].append(event) # If entry event, add event to call stack
        return 0
      elif event[3] == 1:
        pevent = self.funstack[event[0]][event[1]][event[2]].pop() # If exit event, remove corresponding entry event from call stack
        if pevent[4] != event[4]:
          raise Exception("\nCall Stack Violation!\n")
          return 1
        if pevent[4] in self.funmap: # If event corresponds to a function call of interest compute execution time
          if pevent[4] not in self.funtime:
            self.funtime[pevent[4]] = [] # If function id is new to results dictionary create list 
          self.funtime[pevent[4]].append([event[0], event[1], event[2], event[4], pevent[5], event[5] - pevent[5]])
          return 0
      else:
        return 0 # Event is not an exit or entry event
    
    
    def getFunExecTime(self): # Returns the result dictionary
        if len(self.funtime) > 0:
            return self.funtime
        else:
          raise Exception("\nNo result dictionary!\n")
      

    def getFunStack(self):
        return self.funstack

    
    def getFunStackSize(self):
        return len(self.funstack)

    
    def printFunStack(self):
       print("self.funstack = ", self.funstack)

      
    