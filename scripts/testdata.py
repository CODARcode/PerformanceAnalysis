import numpy as np

class TestTraceData:
    def __init__(self):
        self.VAR_SCALAR = [
            'program_count', 'comm_rank_count', 'thread_count',
            'event_type_count', 'timer_count', 'timer_event_count',
            'counter_count', 'counter_event_count', 'comm_count'
        ]
        self.program_count       = np.array([0], dtype=np.int)
        self.comm_rank_count     = np.array([0], dtype=np.int)
        self.thread_count        = np.array([0], dtype=np.int)
        self.event_type_count    = np.array([0], dtype=np.int)
        self.timer_count         = np.array([0], dtype=np.int)
        self.timer_event_count   = np.array([0], dtype=np.uint)
        self.counter_count       = np.array([0], dtype=np.int)
        self.counter_event_count = np.array([0], dtype=np.uint)
        self.comm_count          = np.array([0], dtype=np.uint)

        self.VAR_ARRAY = [
            ('event_timestamps', 'timer_event_count', 6),
            ('counter_values', 'counter_event_count', 6),
            ('comm_timestamps', 'comm_count', 8)
        ]
        self.event_timestamps = np.zeros((1, 6), dtype=np.uint64)
        self.counter_values   = np.zeros((1, 6), dtype=np.uint64)
        self.comm_timestamps  = np.zeros((1, 8), dtype=np.uint64)

    def update(self, step, rank=0):
        self.program_count[:] = step + rank
        self.comm_rank_count[:] = step + rank
        self.thread_count[:] = step + rank
        self.timer_event_count[:] = step + rank
        self.counter_count[:] = step + rank
        self.counter_event_count[:] = step + rank
        self.comm_count[:] = step + rank

        # self.event_timestamps[:] = step + rank
        # self.counter_values[:] = step + rank
        # self.comm_timestamps[:] = step + rank
        self.event_timestamps = (step + rank) * np.ones((step+rank, 6), dtype=np.uint64)
        self.counter_values = (step + rank) * np.ones((step+rank, 6), dtype=np.uint64)
        self.comm_timestamps = (step + rank) * np.ones((step+rank, 8), dtype=np.uint64)