from flask import Flask, jsonify, request, Response, render_template
import threading
from queue import Queue
import struct
import io
import time
# from flask_sse import sse
from gevent.pywsgi import WSGIServer


#
# Data stream
#
class ServerSideEvent(object):
    def __init__(self):
        self.events = {}

    def wait(self):
        ident = threading.get_ident()
        if ident not in self.events:
            self.events[ident] = [threading.Event(), time.time()]
            print('Create new thread')
        return self.events[ident][0].wait()

    def set(self):
        now = time.time()
        remove = []
        for ident, event in self.events.items():
            if not event[0].isSet():
                event[0].set()
                event[1] = now
            else:
                if now - event[1] > 10:
                    remove.append(ident)

        if len(remove) > 0:
            for ident in remove:
                del self.events[ident]

    def clear(self):
        self.events[threading.get_ident()][0].clear()

class ServerSideData(object):
    thread = None
    frame = None
    last_access = 0
    event = ServerSideEvent()
    data_q = Queue()

    for num in range(1000):
        data_q.put(num)

    def __init__(self):
        if ServerSideData.thread is None:
            ServerSideData.last_access = time.time()
            ServerSideData.thread = threading.Thread(target=self._run)
            ServerSideData.thread.start()

            while self.get_frame() is None:
                time.sleep(0.001)

    @classmethod
    def get_frame(cls):
        print('get_frame')
        cls.last_access = time.time()
        cls.event.wait()
        cls.event.clear()
        return cls.frame

    @classmethod
    def _run(cls):
        print('run ServerSideData thread: ', cls.data_q.qsize())
        while True:
            data = cls.data_q.get()
            cls.frame = data
            #cls.frame = data.to_dict()
            print('Frame: ', cls.frame)
            cls.event.set()
            cls.data_q.task_done()
            time.sleep(1)
            if time.time() - cls.last_access > 100:
                break
        print('kill ServerSideData thread')
        cls.thread = None

    @classmethod
    def publish(cls, data):
        cls.data_q.put(data)

def ServerSideDataGen(data:ServerSideData):
    while True:
        frame = data.get_frame()
        print('Gen: ', frame)
        time.sleep(1)
        yield "\ndata: {}\n\n".format(frame)


#
# Data parser
#
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
            'id': self.m_id,
            'funcname': self.m_funcname,
            'pid': self.m_pid,
            'tid': self.m_tid,
            'rid': self.m_rid,
            'fid': self.m_fid,
            'entry': self.m_entry,
            'exit': self.m_exit,
            'runtime': self.m_runtime,
            'label': self.m_label
        }

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

class DataParser(object):
    def __init__(self, name='worker', interval=1, maxsize=0):
        self.name = name
        self.interval = interval
        self.job_q = Queue(maxsize=maxsize)

        self._stop_event = threading.Event()

        thread = threading.Thread(target=self._run, args=())
        thread.daemon = True
        thread.start()

    def put(self, data):
        self.job_q.put(data)

    def join(self):
        self.job_q.join()
        self._stop_event.set()

    def _run(self):
        while not self._stop_event.isSet():
            data = self.job_q.get()

            f = io.BytesIO(data)

            # parse header
            version, rank, nframes, algorithm, winsize, nparam = \
                struct.unpack('<IIIIII', f.read(24))
            uparams = [ d[0] for d in struct.iter_unpack('<I', f.read(2*4*nparam)) ]
            dparams = [ d[0] for d in struct.iter_unpack('<d', f.read(8*nparam)) ]

            # print("Version    : ", version)
            # print("Rank       : ", rank)
            # print("Num. frames: ", nframes)
            # print("Algorithm  : ", algorithm)
            # print("Win. Size  : ", winsize)
            # print("Num. params: ", nparam)
            # print("Parameters (unsigned int): ", uparams)
            # print("Parameters (double)      : ", dparams)

            # parse contents
            n_exec = struct.unpack('<q', f.read(8))[0]
            execData = [ ExecData.fromBinary(f) for _ in range(n_exec) ]
            for d in execData:
                ServerSideData.publish(d)

            # print("Num. exec: ", n_exec)
            # for d in execData:
            #     print(d)

            self.job_q.task_done()
            self._stop_event.wait(self.interval)

dparser = DataParser()
app = Flask(__name__)

@app.route("/stream")
def stream():
    # return jsonify({"message": "ok"})
    #
    # print('stream')
    # return Response(
    #     'data',
    #     mimetype='text/event-stream'
    # )
    return Response(
        ServerSideDataGen(ServerSideData()),
        mimetype='text/event-stream',
        # headers={
        #     'Cache-Control': 'no-cache',
        #     'Access-Control-Allow-Credentials': 'true',
        #     'Access-Control-Allow-Methods': 'GET',
        #     'Access-Control-Expose-Headers': 'X-Events'
        # }
    )

@app.route("/post", methods=['POST'])
def post():
    data = request.get_data()
    dparser.put(data)
    return jsonify({"message": "ok"})

@app.route("/get", methods=['GET'])
def get():
    return jsonify({'message': 'test message'})

@app.route("/")
def index():
    #return render_template('index.html')
    return jsonify({'message': 'This is a parameter server.'})

if __name__ == "__main__":
    host = '0.0.0.0'
    port = 5500

    app.debug = True
    # server = WSGIServer(("", 5500), app)
    # server.serve_forever()
    app.run(host=host, port=port, threaded=True)
    dparser.join()
