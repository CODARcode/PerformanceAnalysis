import os
import struct # for binary stream
import io     # for byte streaming handler

def read_cstr(stream):
    slen, = struct.unpack('<Q', stream.read(8))
    fmt = '<' + str(slen) + 's'
    return struct.unpack(fmt, stream.read(slen))[0].decode('ascii')

class CommData(object):
    def __init__(self):
        self.m_commType = 'commType'
        self.m_pid = 0
        self.m_rid = 0
        self.m_tid = 0
        self.m_src = 0
        self.m_tar = 0
        self.m_bytes = 0
        self.m_tag = 0
        self.m_ts = 0

    def __str__(self):
        return "{}: {}: {}: {}".format(self.m_ts, self.m_commType, self.m_src, self.m_tar)

    def to_dict(self):
        return {
            "event-type": self.m_commType,
            "source-node-id": int(self.m_src),
            "destination-node-id": int(self.m_tar),
            "thread-id": int(self.m_tid),
            "message_size": str(self.m_bytes),
            "message_tag": str(self.m_tag),
            "time": str(self.m_ts)
        }

    @staticmethod
    def fromBinary(stream):
        comm = CommData()
        comm.m_commType = read_cstr(stream)
        comm.m_pid, comm.m_rid, comm.m_tid = struct.unpack('<qqq', stream.read(3*8))
        comm.m_src, comm.m_tar = struct.unpack('<qq', stream.read(2*8))
        comm.m_bytes, comm.m_tag = struct.unpack('<qq', stream.read(2*8))
        comm.m_ts, = struct.unpack('<q', stream.read(8))
        return comm

class ExecData(object):
    def __init__(self):
        self.m_id = 'unique_id'
        self.m_funcname = 'funcname'
        self.m_pid = 0
        self.m_tid = 0
        self.m_rid = 0
        self.m_fid = 0
        self.m_entry = 0
        self.m_exit = 0
        self.m_runtime = 0
        self.m_label = 0
        self.m_parent = 'parent_id'
        self.m_children = []
        self.m_messages = []

    def __str__(self):
        str_general = "{}\npid: {}, rid: {}, tid: {}\nfid: {}, name: {}, label: {}\n" \
               "entry: {}, exit: {}, runtime: {}\n" \
               "parent: {}, # children: {}, # messages: {}".format(
            self.m_id, self.m_pid, self.m_rid, self.m_tid,
            self.m_fid, self.m_funcname, self.m_label,
            self.m_entry, self.m_exit, self.m_runtime,
            self.m_parent, len(self.m_children), len(self.m_messages)
        )
        str_children = "\nChildren: {}".format(self.m_children)
        str_message = "\nMessage: \n"
        for msg in self.m_messages:
            str_message += str(msg)
            str_message += "\n"
        return "{}{}{}".format(str_general, str_children, str_message)

    def to_dict(self):
        return {
            "prog names": str(self.m_pid),
            "name": self.m_funcname,
            "comm ranks": int(self.m_rid),
            "threads": int(self.m_tid),
            "findex": str(self.m_id),
            "anomaly_score": str(self.m_label),
            "parent": str(self.m_parent),
            "children": [str(c) for c in self.m_children],
            "entry": str(self.m_entry),
            "exit": str(self.m_exit),
            "messages": [msg.to_dict() for msg in self.m_messages]
        }
        # return {
        #     'id': self.m_id,
        #     'funcname': self.m_funcname,
        #     'pid': self.m_pid,
        #     'tid': self.m_tid,
        #     'rid': self.m_rid,
        #     'fid': self.m_fid,
        #     'entry': self.m_entry,
        #     'exit': self.m_exit,
        #     'runtime': self.m_runtime,
        #     'label': self.m_label
        # }

    def getNumChildren(self):
        return len(self.m_children)

    def getNumMessage(self):
        return len(self.m_messages)

    @staticmethod
    def fromBinary(stream):
        d = ExecData()
        d.m_id = read_cstr(stream)
        d.m_funcname = read_cstr(stream)
        d.m_pid, d.m_tid, d.m_rid, d.m_fid = struct.unpack('<qqqq', stream.read(4*8))
        d.m_entry, d.m_exit, d.m_runtime = struct.unpack('<qqq', stream.read(3*8))
        d.m_label, = struct.unpack('<i', stream.read(4))
        d.m_parent = read_cstr(stream)

        n_children, = struct.unpack('<Q', stream.read(8))
        d.m_children = [ read_cstr(stream) for _ in range(n_children) ]

        n_message, = struct.unpack('<Q', stream.read(8))
        d.m_messages = [ CommData.fromBinary(stream) for _ in range(n_message) ]

        return d

class ExecDataHead(object):
    offset = 1024
    class SeekPos(object):
        size = 32
        def __init__(self, _bytes, _endianess='<'):
            self.pos = 0
            self.n_exec = 0
            self.step = 0
            self.fid = 0

            fmt = '{}QQQQ'.format(_endianess)
            self.pos, self.n_exec, self.step, self.fid = struct.unpack(fmt, _bytes)

        def __str__(self):
            return 'pos: {}, n_exec: {}, step: {}, fid: {}'.format(
                self.pos, self.n_exec, self.step, self.fid
            )

    def __init__(self):
        self.endianess = '<'
        self.version = 0
        self.rank = 0
        self.nframes = 0
        self.algorithm = 0
        self.winsize = 0
        self.nparam = 0

        self.iparam = []
        self.dparam = []

        self.frameSeekPos = []

    def _load_head(self, headBytes):
        f = io.BytesIO(headBytes)
        endianess = struct.unpack('<c', f.read(1))[0].decode('utf-8')

        self.endianess = '<' if endianess == 'L' else '>'
        fmt = '{}IIIIII'.format(self.endianess)
        unpacked = struct.unpack(fmt, f.read(24))
        self.version = unpacked[0]
        self.rank = unpacked[1]
        self.nframes = unpacked[2]
        self.algorithm = unpacked[3]
        self.winsize = unpacked[4]
        self.nparam = unpacked[5]

        self.iparam = [struct.unpack('{}i'.format(self.endianess), f.read(4))[0] for _ in range(self.nparam)]
        self.dparam = [struct.unpack('{}d'.format(self.endianess), f.read(8))[0] for _ in range(self.nparam)]

    def loadFromIO(self, f):
        self._load_head(f.read(self.offset))
        self.frameSeekPos = [
            ExecDataHead.SeekPos(f.read(ExecDataHead.SeekPos.size)) for _ in range(self.nframes)
        ]

    def getSeekPos(self, step):
        if step < 0 or step > len(self.frameSeekPos):
            return None
        return self.frameSeekPos[step]

    def show(self):
        print("Endianess     : {}".format("Little" if self.endianess == '<' else "Big"))
        print("Version       : {}".format(self.version))
        print("Rank          : {}".format(self.rank))
        print("Num. Frames   : {}".format(self.nframes))
        print("Algorithm     : {}".format(self.algorithm))
        print("Window size   : {}".format(self.winsize))
        print("Num. Param    : {}".format(self.nparam))
        print("Param (int)   : {}".format(self.iparam))
        print("Param (double): {}".format(self.dparam))
        for i in range(self.nframes):
            print(self.frameSeekPos[i])

class ExecDataBody(object):
    def __init__(self):
        self.filename = ''
        self.file = None

    def __del__(self):
        if self.file is not None:
            self.file.close()

    def _open(self, filename):
        if self.file is not None:
            self.file.close()
        self.filename = filename
        self.file = open(filename, 'rb')

    def open(self, filename):
        if self.filename != filename:
            self._open(filename)

    def getFrame(self, seekpos, n_exec):
        self.file.seek(seekpos)
        frame = [ExecData.fromBinary(self.file) for _ in range(n_exec)]
        return frame

class ExecDataParser(object):
    def __init__(self):
        self.ddir = None
        self.dprefix = None
        self.drank = None

        self.head_fn = None
        self.head = ExecDataHead()
        self.body = ExecDataBody()

    def _load(self):
        with open(os.path.join(self.ddir, self.head_fn), "rb") as f:
            self.head.loadFromIO(f)
            self.head.show()

    def load(self, data_dir, prefix, rank):
        if not os.path.exists(data_dir):
            raise ValueError('data directory is not found: {}!'.format(data_dir))

        head_fn = '{}.{}.0.head.dat'.format(prefix, rank)
        if not os.path.exists(os.path.join(data_dir, head_fn)):
            raise ValueError('Cannot find head file: {}!'.format(head_fn))

        self.ddir = data_dir
        self.dprefix = prefix
        self.drank = rank
        self.head_fn = head_fn
        self._load()

    def getNumFrames(self):
        return self.head.nframes

    def getFrame(self, step=0):
        seekpos = self.head.getSeekPos(step)
        if seekpos is None:
            return None

        data_fn = '{}.{}.{}.data.dat'.format(self.dprefix, self.drank, seekpos.fid)
        if not os.path.exists(os.path.join(self.ddir, data_fn)):
            raise ValueError('Cannot find data file: {}!'.format(data_fn))

        self.body.open(os.path.join(self.ddir, data_fn))
        return self.body.getFrame(seekpos.pos, seekpos.n_exec)

if __name__ == "__main__":
    import pprint
    pp = pprint.PrettyPrinter(indent=4)

    parser = ExecDataParser()
    parser.load('./execdata', 'execdata', 9)

    for step in range(parser.getNumFrames()):
        frame = parser.getFrame(step)
        for d in frame:
            if d.getNumMessage():
                print(d)
                pp.pprint(d.to_dict())