# ----------------------------------------------------------------------------
# a function event vector index scheme
#   event:  (ndarray[int])
#       - 0: program id
#       - 1: mpi rank id
#       - 2: thread id
#       - 3: entry or exit id
#       - 4: function id
#       - 5: timestamp
#       - 6: event id
#
#   [0, 5]: obtained from the adios module's "event_timestamps" variable
#   [6]: appended by chimbuko.py while processing data
# ----------------------------------------------------------------------------
FUN_IDX_P = 0  # program id
FUN_IDX_R = 1  # MPI rank id
FUN_IDX_T = 2  # thread id
FUN_IDX_EE = 3  # entry/exit id
FUN_IDX_FUNC = 4  # function id
FUN_IDX_TIME = 5  # timestamp
FUN_IDX_EVENT = 6

# ----------------------------------------------------------------------------
# a count event vector index scheme
#   event:  (ndarray[int])
#       - 0: program id
#       - 1: mpi rank id
#       - 2: thread id
#       - 3: counter id
#       - 4: counter value
#       - 5: timestamp
#       - 6: event id
#
#   [0, 5]: obtained from the adios module's "event_timestamps" variable
#   [6]: appended by chimbuko.py while processing data
# ----------------------------------------------------------------------------
CNT_IDX_P = 0
CNT_IDX_R = 1  # MPI rank id
CNT_IDX_T = 2  # thread id
CNT_IDX_COUNT = 3 # counter id
CNT_IDX_COUNT_VAL = 4
CNT_IDX_TIME = 5  # timestamp
CNT_IDX_EVENT = 6

# ----------------------------------------------------------------------------
# todo: check
# a communication event vector index scheme
#   event:  (ndarray[int])
#       - 0: program id
#       - 1: mpi rank id
#       - 2: thread id
#       - 3: event id (either SEND or RECV)
#       - 4: tag id
#       - 5: partner id
#       - 6: num bytes
#       - 7: timestamp
#       - 8: event id
#
#   [0, 5]: obtained from the adios module's "event_timestamps" variable
#   [6]: appended by chimbuko.py while processing data
# ----------------------------------------------------------------------------
COM_IDX_P = 0
COM_IDX_R = 1  # MPI rank id
COM_IDX_T = 2  # thread id
COM_IDX_SR = 3
COM_IDX_TAG = 4
COM_IDX_PARTNER = 5
COM_IDX_BYTES = 6
COM_IDX_TIME = 7
COM_IDX_EVENT = 8

# ----------------------------------------------------------------------------
# for visualization
# - version: 'v1'
#
# vis trace event index scheme (funData, countData, commData in event.py)
#   event:  (ndarray[int])
#       -  0: program id
#       -  1: mpi rank id
#       -  2: thread id
#       -  3: entry/exit id or tag id
#       -  4: function id ??
#       -  5: counter id
#       -  6: counter value
#       -  7: event type ?
#       -  8: tag id
#       -  9: partner id
#       - 10: num bytes
#       - 11: timestamp
#       - 12: event id
# ----------------------------------------------------------------------------
VIS_DATA_LEN = 13

# ----------------------------------------------------------------------------
# for visualization
# - version: 'v2'
# ----------------------------------------------------------------------------
class CommData(object):
    def __init__(self):
        self.type = '__Unknown_type' # either 'SEND' or 'RECV'
        self.src = -1 # source rank id
        self.dst = -1 # destination rank id
        self.tid = -1 # thread id
        self.m_size = 0 # message size
        self.m_tag = 0  # message tag (is string?)
        self.ts = 0 # time stamp

class ExecData(object):
    def __init__(self, _id):
        self._id = _id

        self.funName = '__Unknown_function' # function name
        self.pid = 0 # program index
        self.tid = 0 # thread index
        self.rid = 0 # rank index
        self.fid = 0 # function index

        self.label = 0 # anomaly label (1 or -1)
        self.entry = 0 # entry timestamp
        self.exit  = 0 # exit timestamp
        self.runtime = 0 # execution time (= exit - entry)

        self.children = []
        self.parent = -1

        self.message = []

    def __str__(self):
        return str(dict(
            id=self._id,
            name=self.funName,
            pid=self.pid, tid=self.tid, rid=self.rid, fid=self.fid,
            entry=self.entry, exit=self.exit, runtime=self.runtime, label=self.label,
            parent=self.parent, children=self.children,
            message=self.message
        ))

    def to_dict(self):
        return {
            "prog names": self.pid,
            "name": self.funName,
            "comm ranks": self.rid,
            "threads": self.tid,
            "findex": self._id,
            "anomaly_score": self.label,
            "parent": self.parent,
            "children": self.children,
            "entry": self.entry,
            "exit": self.exit,
            "messages": self.message
        }

    def get_id(self):
        return self._id

    def set_parent(self, _id):
        self.parent = _id

    def set_label(self, label):
        self.label = label

    def add_child(self, _id):
        self.children.append(_id)

    def update_entry(self, fname, pid, rid, tid, fid, entry):
        self.funName = fname
        self.pid = pid
        self.rid = rid
        self.tid = tid
        self.fid = fid
        self.entry = entry

    def update_exit(self, fid, _exit):
        if self.fid != fid or self.entry > _exit: return False
        self.exit = _exit
        self.runtime = _exit - self.entry
        return True


