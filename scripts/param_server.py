from flask import Flask, jsonify, request
from collections import defaultdict
from algorithm.runstats import RunStats

app = Flask(__name__)

class ParamServer(object):
    def __init__(self):
        self.ps = defaultdict(lambda: RunStats())

    def clear(self):
        self.ps = defaultdict(lambda: RunStats())

    def update(self, funcid, stat:list):
        _ps = self.ps[int(funcid)]
        _ps.update(*stat)
        return _ps.stat()

    def get(self, funcid):
        _ps = self.ps[int(funcid)]
        return _ps.stat()

# parameter server
ps = ParamServer()

@app.route("/clear", methods=['POST'])
def clear():
    ps.clear()
    return jsonify({'message': 'okay'})

@app.route("/stat/<int:pid>", methods=['POST'])
def stat(pid):
    stat = ps.get(int(pid))
    return jsonify({'id': pid, 'stat': stat})

@app.route("/update", methods=['POST'])
def update():
    param = request.get_json(force=True)
    funid = int(param.get('id'))
    stat = param.get('stat')
    stat = ps.update(funid, stat)
    return jsonify({'id': funid, 'stat': stat})

@app.route("/")
def index():
    return jsonify({'message': 'This is a parameter server.'})

if __name__ == "__main__":
    import sys
    if len(sys.argv) == 2:
        try:
            log = open(sys.argv[1], 'w')
        except IOError:
            log = None

        if log is not None:
            sys.stdout = sys.stderr = log

    app.run(
        host='0.0.0.0',
        port=5500
    )