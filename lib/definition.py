"""@package Chimbuko

Definition of data structure

Authos(s):
    Sungsoo Ha (sungsooha@bnl.gov)

Last Modified:
    April 11, 2019
"""

# ----------------------------------------------------------------------------
# a function event vector index scheme (event_timestamps from TAU)
#       - 0: program id
#       - 1: mpi rank id
#       - 2: thread id
#       - 3: entry or exit id
#       - 4: function id
#       - 5: timestamp
# ----------------------------------------------------------------------------
FUN_IDX_P     = 0
FUN_IDX_R     = 1
FUN_IDX_T     = 2
FUN_IDX_EE    = 3
FUN_IDX_FUNC  = 4
FUN_IDX_TIME  = 5
FUN_IDX_EVENT = 6

# ----------------------------------------------------------------------------
# a count event vector index scheme (counter_values from TAU)
#       - 0: program id
#       - 1: mpi rank id
#       - 2: thread id
#       - 3: counter id
#       - 4: counter value
#       - 5: timestamp
# ----------------------------------------------------------------------------
CNT_IDX_P         = 0
CNT_IDX_R         = 1
CNT_IDX_T         = 2
CNT_IDX_COUNT     = 3
CNT_IDX_COUNT_VAL = 4
CNT_IDX_TIME      = 5

# ----------------------------------------------------------------------------
# a communication event vector index scheme (comm_timestamps from TAU)
#       - 0: program id
#       - 1: mpi rank id
#       - 2: thread id
#       - 3: event id (either SEND or RECV)
#       - 4: tag id
#       - 5: partner id
#       - 6: num bytes
#       - 7: timestamp
# ----------------------------------------------------------------------------
COM_IDX_P       = 0
COM_IDX_R       = 1
COM_IDX_T       = 2
COM_IDX_SR      = 3
COM_IDX_TAG     = 4
COM_IDX_PARTNER = 5
COM_IDX_BYTES   = 6
COM_IDX_TIME    = 7

# ----------------------------------------------------------------------------
# Data structure for VIS module
# ----------------------------------------------------------------------------
class CommData(object):
    """communication data"""
    def __init__(self, comm_type, src, dst, tid, msg_size, msg_tag, ts):
        """
        constructor
        Args:
            comm_type : (str), either 'SEND' or 'RECV'
            src: (int), source rank id
            dst: (int), destination rank id
            tid: (int), thread id
            msg_size: (int), message size
            msg_tag: (int), message tag
            ts: (int), timestamp
        """
        self.type = comm_type
        self.src = src
        self.dst = dst
        self.tid = tid
        self.msg_size = msg_size
        self.msg_tag = msg_tag
        self.ts = ts

    def to_dict(self):
        """Convert to dictionary following structure in VIS module"""
        return {
            "event-type": self.type,
            "source-node-id": int(self.src),
            "destination-node-id": int(self.dst),
            "thread-id": int(self.tid),
            "message_size": str(self.msg_size),
            "message_tag": str(self.msg_tag),
            "time": str(self.ts)
        }

class ExecData(object):
    """Execution data structure"""
    def __init__(self, _id):
        """
        constructor
        Args:
            _id: unique id
        """
        self._id = _id

        self.funName = '__Unknown_function' # function name
        self.pid = 0                        # program index
        self.tid = 0                        # thread index
        self.rid = 0                        # rank index
        self.fid = 0                        # function index

        self.label = 1                      # anomaly label (1 or -1)
        self.entry = 0                      # entry timestamp
        self.exit  = 0                      # exit timestamp
        self.runtime = 0                    # execution time (= exit - entry)

        self.children = []                  # list of child executions
        self.parent = -1                    # parent execution, -1 if it is root execution

        self.message = []                   # list of associated communication data

    def __str__(self):
        """print output"""
        return "[{}][{}][{}][{}] {} sec, {}, {}, {}, {}".format(
            self.pid, self.tid, self.rid, self.funName,
            self.runtime,
            'root' if self.parent == -1 else 'child',
            len(self.children), len(self.message),
            'normal' if self.label == 1 else 'abnormal'
        )

    def to_dict(self):
        """convert to dictionary following structure needed by VIS module"""
        return {
            "prog names": str(self.pid),
            "name": self.funName,
            "comm ranks": int(self.rid),
            "threads": int(self.tid),
            "findex": str(self._id),
            "anomaly_score": str(self.label),
            "parent": str(self.parent),
            "children": [str(c) for c in self.children],
            "entry": str(self.entry),
            "exit": str(self.exit),
            "messages": [msg.to_dict() for msg in self.message]
        }

    def get_id(self):
        """get id"""
        return self._id

    def set_parent(self, _id):
        """set parent id"""
        self.parent = _id

    def set_label(self, label):
        """set label"""
        self.label = label

    def get_label(self):
        """get label"""
        return self.label

    def add_child(self, _id):
        """add child id"""
        self.children.append(_id)

    def update_entry(self, fname, pid, rid, tid, fid, entry):
        """update execution entry information"""
        self.funName = fname
        self.pid = pid
        self.rid = rid
        self.tid = tid
        self.fid = fid
        self.entry = entry

    def update_exit(self, fid, _exit):
        """update execution exit information, must have entry information in advance!"""
        if self.fid != fid or self.entry > _exit: return False
        self.exit = _exit
        self.runtime = _exit - self.entry
        return True

    def add_message(self, msg):
        """add communication infomation associated with this execution"""
        self.message.append(msg)


