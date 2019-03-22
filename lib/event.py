"""@package Chimbuko
This module processes events such as function calls, communication and processor counters

Author(s): 
    Gyorgy Matyasfalvi (gmatyasfalvi@bnl.gov)
    Sungsoo Ha (sungsooha@bnl.gov)

Created:
    August, 2018
    
    pydoc -w event
    
"""
from collections import deque
from collections import defaultdict
import numpy as np

from definition import *

class  Event(object):
    """
    Event class keeps track of event information such as
        1. function call stack (funStack) and
        2. function execution times (funtime).
    In addition the class contains methods for processing events and event related information.
    
    Attributes:
        Each event is associated with:: 
            A work flow component or program, indicated by p
            A node or MPI rank indicated by r
            A thread indicated by t 
    """


    def __init__(self, log=None):
        """This is the constructor for the Event class."""
        self.log = log


        # a dictionary of function ids;
        #   - key: function id (int)
        #   - value function name (string)
        self.funMap = None

        # Event type id
        self.entry = None  # id (int) of `ENTRY` event
        self.exit = None   # id (int) of `EXIT` event
        #self.setEventType(eventType)

        # ---------------------------------------------------------------------
        # for function calls (event_timestamps)
        # ---------------------------------------------------------------------
        # A nested dictionary of function calls;
        #   [program id (int)]
        #           |----- [mpi rank id (int)]
        #                           |----- [thread id (int)] : deque()
        self.funStack = {}
        self.stackSize = 0 # stack size

        # function index
        self.fidx = 0

        # ndarray to store function calls (n_events x 13)
        # see definition.py for details
        # - this will be deprecated in the future
        self.funData = None

        # dictionary to store function execution time information
        # - key: function id
        # - value: a list of list, (old)
        #   [PID, RID, TID, FID, start time, execution time, event ID]
        # - value: a list of exec data (dict)
        #self.funtime = {}
        self.funtime = defaultdict(list)


        # maximum function depth (dict)
        # - key: function id
        # - value: maximum depth
        self.maxFunDepth = defaultdict(int)

        # ndarray to store counter values, (n_events x 13)
        # - a vector length of 13 contains count event data (length of 7) + additional info
        self.countData = None
        # counter data index
        self.ctidx = 0

        # ndarray to store communication values, (n_events x )
        self.commData = None
        # communication data index
        self.coidx = 0

        # todo: [VIS]
        # For sending events to the viz server that have already exited
        #self.funList = None
        # self.funDataTemp = None


    def setEventType(self, eventType:list):
        """Sets entry and exit variables.
        
        Args:
            eventType (list):
                the index corresponding to the ENTRY/EXIT entries is the respective id
                for that event i.e. if we have a list: ['ENTRY', 'EXIT'] then entry events have
                id 0 and exit id 1
        """
        if eventType is None: return
        try:
            self.entry = eventType.index('ENTRY')
            self.exit = eventType.index('EXIT')
        except ValueError:
            self.log.info("ENTRY or EXIT event not present...")

    def setFunMap(self, funMap):
        """Sets funMap variable.

        Args:
            funMap (dictionary):
                key: (int) function id
                value: (string) function name
        """
        self.funMap = funMap

    def initFunData(self, numEvent):
        """
        @ will be deprecated.

        Initialize funData to store function calls in format
        required by visualization.

        Args:
            numEvent (int):
                number of function calls that will be required to store
        """
        self.funData = np.full((numEvent, VIS_DATA_LEN), np.nan)

    def createCallStack(self, p, r, t): 
        """Creates a stack for each p,r,t combination.

        Args:
          p (int): program id
          r (int): rank id
          t (int): thread id

        Returns:
          No return value.
        """
        if self.funStack.get(p, None) is None:
            self.funStack[p] = dict()

        if self.funStack[p].get(r, None) is None:
            self.funStack[p][r] = dict()

        if self.funStack[p][r].get(t, None) is None:
            self.funStack[p][r][t] = deque()

    def addFun_v1(self, event):
        """Adds function call to call stack
        Args:
            event:  (ndarray[int])
                0: program id
                1: mpi rank id
                2: thread id
                3: entry or exit id
                4: function id
                5: timestamp
                6: event id
                obtained from the adios module's "event_timestamps" variable
        Returns: (bool)
            True:  the event was successfully added to the call stack without any violations including
                - timestamp: i.e. exit event's timestamp > entry event's timestamp
            False: otherwise.
        """
        # Make sure stack corresponding to (program, mpi rank, thread) is created
        pid, rid, tid, eid = event[:4]
        self.createCallStack(pid, rid, tid)

        if eid == self.entry:
            # If entry event, add event to call stack
            self.funStack[pid][rid][tid].append(event)
            self.funData[self.fidx, :5] = event[:5]
            self.funData[self.fidx, 11:] = event[5:]
            self.fidx += 1
            return True

        if eid == self.exit:
            # If exit event, remove corresponding entry event from call stack
            prev = self.funStack[pid][rid][tid].pop()

            # check call stack violation
            if prev[FUN_IDX_FUNC] != event[FUN_IDX_FUNC] or \
                prev[FUN_IDX_TIME] > event[FUN_IDX_TIME]:
                return False

            self.funData[self.fidx,:5] = event[:5]
            self.funData[self.fidx,11:] = event[5:]
            self.fidx += 1

            fid = event[FUN_IDX_FUNC]
            self.maxFunDepth[fid] = max(
                self.maxFunDepth.get(fid, 0),
                len(self.funStack[pid][rid][tid])
            )

            # NOTE I think the first if condition can be removed since
            # NOTE we're computing anomalies for all functions
            pfid = prev[FUN_IDX_FUNC]
            if self.funtime.get(pfid, None) is None: self.funtime[pfid] = []
            self.funtime[pfid].append(
                [
                    *event[:3], event[FUN_IDX_FUNC],       # PID, RID, TID, FID
                    prev[FUN_IDX_TIME],                    # start time
                    event[FUN_IDX_TIME] - prev[FUN_IDX_TIME],  # execution time
                    prev[FUN_IDX_EVENT]                    # event id
                ]
            )

            #todo: [VIS]
            #self.funList.append(pevent)
            #self.funList.append(event)
            return True
        # Event is not an exit or entry event
        return True

    def addFun_v2(self, event):
        """Adds function call to call stack
        Args:
            event:  (ndarray[int])
                0: program id
                1: mpi rank id
                2: thread id
                3: (MUST) entry or exit id
                4: function id (called timer id in sos_flow)
                5: timestamp
                6: event id

                obtained from the adios module's "event_timestamps" variable

        Returns: (bool)
            True:  the event was successfully added to the call stack without any violations including
                - timestamp: i.e. exit event's timestamp > entry event's timestamp
            False: otherwise.
        """
        # Make sure stack corresponding to (program, mpi rank, thread) is created
        pid, rid, tid, eid, fid, ts, event_id = event
        fname = self.funMap[fid]
        self.createCallStack(pid, rid, tid)

        if eid not in [self.entry, self.exit]:
            return True


        if eid == self.entry:
            # If entry event, add the event to call stack
            execData = ExecData(event_id)
            execData.update_entry(fname, pid, rid, tid, fid, ts)

            try:
                p_execData = self.funStack[pid][rid][tid][-1]
            except IndexError:
                p_execData = None
            if p_execData is not None:
                execData.set_parent(p_execData.get_id())
                p_execData.add_child(execData.get_id())

            self.funStack[pid][rid][tid].append(execData)

            #self.funData[self.fidx, :5] = event[:5]
            #self.funData[self.fidx, 11:] = event[5:]
            #self.fidx += 1
            return True

        if eid == self.exit:
            # If exit event, remove corresponding entry event from call stack
            execData = self.funStack[pid][rid][tid].pop()

            # check call stack violation
            if not execData.update_exit(fid, ts):
                return False
            # if prev[FUN_IDX_FUNC] != event[FUN_IDX_FUNC] or \
            #     prev[FUN_IDX_TIME] > event[FUN_IDX_TIME]:
            #     return False

            #self.funData[self.fidx,:5] = event[:5]
            #self.funData[self.fidx,11:] = event[5:]
            #self.fidx += 1

            #fid = event[FUN_IDX_FUNC]
            self.maxFunDepth[fid] = max(
                self.maxFunDepth.get(fid, 0),
                len(self.funStack[pid][rid][tid])
            )

            # NOTE I think the first if condition can be removed since
            # NOTE we're computing anomalies for all functions
            #pfid = prev[FUN_IDX_FUNC]
            self.funtime[fid].append(execData)
            # if self.funtime.get(fid, None) is None:
            #     self.funtime[fid] = []
            # self.funtime[pfid].append(
            #     [
            #         *event[:3], event[FUN_IDX_FUNC],       # PID, RID, TID, FID
            #         prev[FUN_IDX_TIME],                    # start time
            #         event[FUN_IDX_TIME] - prev[FUN_IDX_TIME],  # execution time
            #         prev[FUN_IDX_EVENT]                    # event id
            #     ]
            # )

            #todo: [VIS]
            #self.funList.append(pevent)
            #self.funList.append(event)
            return True
        # Event is not an exit or entry event
        return True

    def initCountData(self, numEvent):
        """
        Initialize numpy array countData to store counter values in the format
        required by visualization.

        Args:
            numEvent (int): number of counter events that will be required to store
        """
        self.countData = np.full((numEvent, VIS_DATA_LEN), np.nan)

    def addCount(self, event):
        """Add counter data and store in format required by visualization.

        Args:
            event (numpy array of int-s): [
                0: program,
                1: mpi rank,
                2: thread,
                3: counter id,
                4: counter value,
                5: timestamp,
                6: event id
            ].

        Returns:
            bool: True, if the event was successfully added, False otherwise.
        """
        self.countData[self.ctidx,  :3] = event[:3]
        self.countData[self.ctidx, 5:7] = event[3:5]
        self.countData[self.ctidx, 11:] = event[5:]
        self.ctidx += 1
        return True

    def initCommData(self, numEvent):
        """
        Initialize numpy array commData to store communication data in format
        required by visualization.

        Args:
            numEvent (int): number of communication events that will be required to store
        """
        self.commData = np.full((numEvent, VIS_DATA_LEN), np.nan)

    def addComm(self, event):
        """Add communication data and store in format required by visualization.
        Args:
            event (numpy array of int-s): [
                0: program,
                1: mpi rank,
                2: thread,
                3: ???,
                4: tag id,
                5: partner id,
                6: num bytes,
                7: timestamp
                8: evnet id
            ].

        Returns:
            bool: True, if the event was successfully added, False otherwise.
        """
        self.commData[self.coidx, :4] = event[:4]
        self.commData[self.coidx, 8:11] = event[4:7]
        self.commData[self.coidx, 11:] = event[7:]
        self.coidx += 1
        return True

    def clearFunTime(self):
        """Clear function execution time data."""
        self.funtime.clear()

    def clearFunData(self):
        """Clear funData created by initFunData() method."""
        self.fidx = 0
        self.funData = None
        #This portion is only needed if the visualization requires to send function calls that have exited
        #self.funList.clear()
        #if self.funDataTemp is None:
        #    pass
        #else:
        #    del self.funDataTemp

    def clearCountData(self):
        """Clear countData created by initCountData() method."""
        self.ctidx = 0
        self.countData = None

    def clearCommData(self):
        """Clear commData created by initCommData() method."""
        self.coidx = 0
        self.commData = None

    def getFunData(self):
        """Get method for funData."""
        return self.funData if self.funData is not None else np.array([])
        # This portion is only needed if the visualization requires to send function calls that have exited
        # self.funList.sort(key=lambda x: x[6])
        # self.funData = np.full((len(self.funList), 13), np.nan)
        # self.funDataTemp = np.array(self.funList)
        # self.funData[:,0:5] = self.funDataTemp[:,0:5]
        # self.funData[:,11:13] = self.funDataTemp[:,5:7]

    def getCountData(self):
        """Get method for countData."""
        return self.countData if self.countData is not None else np.array([])

    def getCommData(self):
        """Get method for commData."""
        return self.commData if self.commData is not None else np.array([])

    def getFunTime(self):
        """Get method for function exection time data."""
        assert (len(self.funtime) > 0), "Frame only contains open functions (Assertion)..."
        return self.funtime

    def getFunStack(self):
        """Get method for function call stack."""
        return self.funStack

    def countStackSize(self, d):
        """
        Given a dictionary of stacks/arrays/deques counts the total length
        of those stacks/arrays/deques.

        # A nested dictionary of function calls;
        #   [program id (int)]
        #           |----- [mpi rank id (int)]
        #                           |----- [thread id (int)] : deque()
        self.funStack = {}

        Args:
            d (dict): a dictionary of stacks/arrays/deques
            s (int): total length of stacks/arrays/deques in the dictionary

        Returns:
            No return value.
        """
        count = 0
        for k, v in d.items():
            if isinstance(v, dict):
                count += self.countStackSize(v)
            else:
                count += len(v)
        return count

    def getFunStackSize(self):
        """Counts the total number of function calls still in the stack.
        Returns:
            Integer, corresponding to the total number of function calls still in in the stack.
        """
        self.stackSize = self.countStackSize(self.funStack)
        return self.stackSize

    def getMaxFunDepth(self):
        """Get method for the maximum depth of the function call stack."""
        return self.maxFunDepth

    def printFunStack(self):
        """Prints the function call stack."""
        try:
            import pprint
            pp = pprint.PrettyPrinter(indent=3)
            pp.pprint(self.funStack)
        except ImportError:
            print("self.funStack = ", self.funStack)


    #This portion is only needed if the visualization requires to send function calls that have exited
    #def initFunData(self, numEvent):
    #    self.funList = []