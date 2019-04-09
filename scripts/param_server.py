from flask import Flask, jsonify, request
from collections import defaultdict
from algorithm.runstats import RunStats
from threading import Lock
import json

app = Flask(__name__)

class ParamServer(object):
    def __init__(self):
        self.ps = defaultdict(lambda: RunStats())
        self.lock = Lock()

    def clear(self):
        self.ps = defaultdict(lambda: RunStats())

    def update(self, funcid, stat:list):
        with self.lock:
            _ps = self.ps[int(funcid)]
            _ps.update(*stat)
        return _ps.stat()

    def get(self, funcid):
        with self.lock:
            _ps = self.ps[int(funcid)]
        return _ps.stat()

    def add_abnormal(self, funcid, n_abnormal):
        with self.lock:
            _ps = self.ps[int(funcid)]
            _ps.add_abnormal(n_abnormal)
        return _ps.count()[1]


    def dump(self):
        with self.lock:
            data = {}
            for funid, stat in self.ps.items():
                data[funid] = [stat.mean(), stat.std(), *stat.stat()]
            with open('./ps_stat.json', 'w') as f:
                json.dump(data, f, indent=4, sort_keys=True)

# parameter server
ps = ParamServer()

@app.route("/add_abnormal", methods=['POST'])
def add_abnormal():
    param = request.get_json(force=True)
    funid = int(param.get('id'))
    n_abnormal = int(param.get('abnormal'))
    n_abnormal = ps.add_abnormal(funid, n_abnormal)
    return jsonify({'id': funid, 'abnormal': n_abnormal})

@app.route("/clear", methods=['POST'])
def clear():
    ps.clear()
    return jsonify({'message': 'okay'})

@app.route("/stat/<int:pid>", methods=['POST'])
def stat(pid):
    stat = ps.get(int(pid))
    return jsonify({'id': pid, 'stat': stat})

@app.route("/stat", methods=['POST'])
def all_stat():
    pass

@app.route("/update", methods=['POST'])
def update():
    param = request.get_json(force=True)
    funid = int(param.get('id'))
    stat = param.get('stat')
    stat = [float(d) for d in stat]
    stat = ps.update(funid, stat)
    return jsonify({'id': funid, 'stat': stat})

@app.route("/")
def index():
    return jsonify({'message': 'This is a parameter server.'})

if __name__ == "__main__":
    import sys
    import configparser

    host = '0.0.0.0'
    port = 5500
    log = ''
    try:
        configFile = sys.argv[1]
        config = configparser.ConfigParser(interpolation=None)
        config.read(configFile)

        url = config['Outlier']['PSUrl']
        if url.startswith('http'):
            url = url.split('//')[1]
        host = url[:-5]
        port = int(url[-4:])

        log = config['Outlier']['PSLog']
    except IOError as e:
        print('IOError: ', e)
    except KeyError as e:
        print('KeyError: ', e)

    try:
        f = open(log, 'w')
        sys.stdout = sys.stderr = f
    except IOError as e:
        print('IOError: ', e)

    app.run(host=host, port=port)
