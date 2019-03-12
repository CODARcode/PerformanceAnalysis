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
#       - 3: tag id ?
#       - 4: partner id ?
#       - 5: ?
#       - 6: num bytes ?
#       - 7: timestamp
#       - 8: event id
#
#   [0, 5]: obtained from the adios module's "event_timestamps" variable
#   [6]: appended by chimbuko.py while processing data
# ----------------------------------------------------------------------------
COM_IDX_P = 0
COM_IDX_R = 1  # MPI rank id
COM_IDX_T = 2  # thread id
COM_IDX_TAG = 3
COM_IDX_PARTNER = 4
COM_IDX_UNKNOWN = 5
COM_IDX_BYTES = 6
COM_IDX_TIME = 7
COM_IDX_EVENT = 8
