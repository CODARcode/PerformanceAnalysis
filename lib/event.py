"""@package Chimbuko
This module processes events such as function calls, communication and processor counters

Author(s): 
    Gyorgy Matyasfalvi (gmatyasfalvi@bnl.gov)
    Sungsoo Ha (sungsooha@bnl.gov)

Created:
    August, 2018

Last Modified
    April, 2019
"""
import numpy as np
from collections import deque
from collections import defaultdict
from definition import *

class Event(object):
    """
    Event class keeps track of event information such as
        1. function call stack (funStack) and
        2. function execution time (or data) (funtime).
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
        #   - key: function id (uint64)
        #   - value function name (string)
        self.funMap = None

        # Event type id
        self.entry = None  # id (int) of `ENTRY` event
        self.exit = None   # id (int) of `EXIT` event
        self.send = None   # id (int) of `SEND` event
        self.recv = None   # id (int) of `RECV` event

        # ---------------------------------------------------------------------
        # for function calls (event_timestamps)
        # ---------------------------------------------------------------------
        # A nested dictionary of function calls;
        #   [program id (int)]
        #           |----- [mpi rank id (int)]
        #                           |----- [thread id (int)] : deque()
        self.funStack = {}
        self.stackSize = 0 # stack size

        # dictionary to store function execution time information
        # - key: function id
        # - value: a list of exec data (dict)
        #self.funtime = {}
        self.funtime = defaultdict(list)

        # maximum function depth (dict)
        # - key: function id
        # - value: maximum depth
        self.maxFunDepth = defaultdict(int)

        # ndarray to store counter values, (n_events x 13)
        # self.countData = None # (unused, will revisite later)
        # counter data index
        # self.ctidx = 0 # (unused, will revisite later)

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

        try:
            self.recv = eventType.index('RECV')
            self.send = eventType.index('SEND')
        except ValueError:
            self.log.info("RECV or SEND event not present...")

    def setFunMap(self, funMap):
        """Sets funMap variable.

        Args:
            funMap (dictionary):
                key: (int) function id
                value: (string) function name
        """
        self.funMap = funMap

    def createCallStack(self, p, r, t): 
        """Creates a stack for each p,r,t combination.

        Args:
          p (int): program id
          r (int): rank id
          t (int): thread id
        """
        p = int(p)
        r = int(r)
        t = int(t)

        if self.funStack.get(p, None) is None:
            self.funStack[p] = dict()

        if self.funStack[p].get(r, None) is None:
            self.funStack[p][r] = dict()

        if self.funStack[p][r].get(t, None) is None:
            self.funStack[p][r][t] = deque()

    def addFun(self, event, event_id):
        """Adds function call to call stack
        Args:
            event:  function call event (see definition.py for details)
            event_id: (str), uuid

        Returns: (bool)
            True:  the event was successfully added to the call stack without any violations including
                - timestamp: i.e. exit event's timestamp > entry event's timestamp
            False: otherwise.
        """
        pid, rid, tid, eid, fid, ts = event

        # NOTE: with SST engine, sometimes, we get very large number for program index,
        # rank index, thread index and so on. It may be corrupted data or garbage data. This
        # issue is reported to adios team (Jong). In the mean time, if we observe these events,
        # we will igore it because it doesn't cause any stack violation so far.
        if pid > 100000 or rid > 100000 or tid > 100000:
            return True

        fname = self.funMap[fid]
        # Make sure stack corresponding to (program, mpi rank, thread) is created
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
            return True

        elif eid == self.exit:
            # If exit event, remove corresponding entry event from call stack
            execData = self.funStack[pid][rid][tid].pop()

            # check call stack violation
            if not execData.update_exit(fid, ts):
                return False

            self.maxFunDepth[fid] = max(
                self.maxFunDepth.get(fid, 0),
                len(self.funStack[pid][rid][tid])
            )

            self.funtime[fid].append(execData)
            return True
        # Event is not an exit or entry event
        return True

    def addComm(self, event):
        """
        Add communication data and store in format required by visualization.
        Args:
            event: communication event (see definition.py for details)
        Returns:
            bool: True, if the event was successfully added, False otherwise.
        """
        pid, rid, tid, eid, tag, partner, nbytes, ts = event
        if eid not in [self.send, self.recv]:
            if self.log is not None:
                self.log.debug("No attributes for SEND/RECV")
            return True

        try:
            execData = self.funStack[pid][rid][tid][-1]
        except IndexError:
            if self.log is not None:
                self.log.debug("Communication event before any function calls")
            return True
        except KeyError:
            if self.log is not None:
                self.log.debug("Communication event before any function calls")
            return True

        if eid == self.send:
            comType = 'send'
            comSrc = rid
            comDst = partner
        else:
            comType = 'receive'
            comSrc = partner
            comDst = rid

        comData = CommData(comm_type=comType, src=comSrc, dst=comDst, tid=tid,
                           msg_size=nbytes, msg_tag=tag, ts=ts)

        execData.add_message(comData)
        return True

    def clearFunTime(self):
        """Clear function execution time data."""
        self.funtime.clear()

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

        Args:
            d (dict): a dictionary of stacks/arrays/deques
        Returns:
            total stack size
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
        output = self.log.info if self.log is not None else print
        output('***** Start Function call stack *****')
        for pid, p_value in self.funStack.items():
            for rid, r_value in p_value.items():
                for tid, t_value in r_value.items():
                    #output("[{}][{}][{}]: {}".format(pid, rid, tid, len(t_value)))
                    if self.log is not None:
                        while len(t_value):
                            self.log.info(t_value.pop())
        output('***** End   Function call stack *****')

    # def initCountData(self, numEvent):
    #     """ (unused, commented just for now)
    #     Initialize numpy array countData to store counter values in the format
    #     required by visualization.
    #
    #     Args:
    #         numEvent (int): number of counter events that will be required to store
    #     """
    #     self.countData = np.full((numEvent, 13), np.nan)
    # def addCount(self, event):
    #     """ (unused, commented just for now)
    #     Add counter data and store in format required by visualization.
    #
    #     Args:
    #         event: count event (see definition.py for details)
    #     Returns:
    #         bool: True, if the event was successfully added, False otherwise.
    #     """
    #     self.countData[self.ctidx,  :3] = event[:3]
    #     self.countData[self.ctidx, 5:7] = event[3:5]
    #     self.countData[self.ctidx, 11:] = event[5:]
    #     self.ctidx += 1
    #     return True
    # def clearCountData(self):
    #     """Clear countData created by initCountData() method."""
    #     self.ctidx = 0
    #     self.countData = None
