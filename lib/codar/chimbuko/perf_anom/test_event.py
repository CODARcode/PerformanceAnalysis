import os
import sys
import configparser
import pickle
import numpy as np
import event
import unittest

class TestAddFun(unittest.TestCase):
    """Test adding a function call.
    """
    
    def setUp(self):
        """Setup function call trace data, which is a 2D numpy array,
        where each column corresponds to the following entries 
        [program, mpi rank, thread, entry/exit, function id, timestamp, event id] 
        """
        # Setting up 
        program = np.zeros((6,), np.uint64)
        mpirank = np.zeros((6,), np.uint64)
        thread = np.zeros((6,), np.uint64)
        event = np.array([0,0,0,1,1,1], np.uint64)
        passFunid = np.array([0,1,1,1,1,0], np.uint64)
        failFunid = np.array([0,1,1,1,0,1], np.uint64)
        timestamp = np.array([100,101,102,103,104,105], np.uint64)
        eventid = np.array([0,1,2,3,4,5], np.uint64)
        
        # Set up function call data that should pass
        passFunCall = np.zeros((6, 7), np.uint64)
        passFunCall[:,0] = program[:]
        passFunCall[:,1] = mpirank[:]
        passFunCall[:,2] = thread[:]
        passFunCall[:,3] = event[:]
        passFunCall[:,4] = passFunid[:]
        passFunCall[:,5] = timestamp[:]
        passFunCall[:,6] = eventid[:]
        
        pfile = "passFunCall.pickle"
        with open(pfile, 'wb') as handle:
            pickle.dump(passFunCall, handle, protocol=pickle.HIGHEST_PROTOCOL)
        
        # Set up function call data that should fail
        failFunCall = np.zeros((6, 7), np.uint64)
        failFunCall[:,0] = program[:]
        failFunCall[:,1] = mpirank[:]
        failFunCall[:,2] = thread[:]
        failFunCall[:,3] = event[:]
        failFunCall[:,4] = failFunid[:]
        failFunCall[:,5] = timestamp[:]
        failFunCall[:,6] = eventid[:]
        
        pfile = "failFunCall.pickle"
        with open(pfile, 'wb') as handle:
            pickle.dump(failFunCall, handle, protocol=pickle.HIGHEST_PROTOCOL)
    
    
    def tearDown(self):
        """Delete pickle files
        """
        pfile = "passFunCall.pickle"
        os.remove(pfile)
        pfile = "failFunCall.pickle"
        os.remove(pfile)
        
    
    def test_addFun(self):
        """Test function call data for call stack violations.
        """
        
        et = ['ENTRY', 'EXIT']
        
        pfile = "passFunCall.pickle"
        with open(pfile, 'rb') as handle:
            passFunCall = pickle.load(handle)
        
        passStack = True
        passEvn = event.Event("test.cfg")
        passEvn.setEventType(et)
        passEvn.initFunData(passFunCall.shape[0])
        for i in passFunCall:
            if passEvn.addFun(i): # Store function call data in event object
                pass
            else:
                passStack = False
                break    
        self.assertTrue(passStack)
        
        
        pfile = "failFunCall.pickle"
        with open(pfile, 'rb') as handle:
            failFunCall = pickle.load(handle)
        
        failStack = True
        failEvn = event.Event("test.cfg")
        failEvn.setEventType(et)
        failEvn.initFunData(failFunCall.shape[0])
        for i in failFunCall:
            if failEvn.addFun(i): # Store function call data in event object
                pass
            else:
                
                failStack = False
                break    
        self.assertFalse(failStack)
    

if __name__ == '__main__':
    unittest.main()
    
#if __name__ == '__main__':
    #addFun = TestAddFun()
    #addFun.setUp()
    #addFun.tearDown()     
            
        