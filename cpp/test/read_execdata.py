import os
import struct # for binary stream
import io     # for byte streaming handler

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

class ExecDataParser(object):
    def __init__(self):
        self.ddir = None
        self.dprefix = None
        self.drank = None

        self.head_fn = None
        self.data_fn = None

        self.head = ExecDataHead()

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

    def getFrame(self, step=0):
        pass



if __name__ == "__main__":
    parser = ExecDataParser()

    parser.load('./execdata', 'execdata', 0)
