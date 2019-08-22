from flask import Flask, request, jsonify
import logging
import struct

app = Flask(__name__)

log = logging.getLogger('werkzeug')
log.disabled = True
app.logger.disabled = True

def shutdown_server():
    func = request.environ.get('werkzeug.server.shutdown')
    if func is None:
        raise RuntimeError('Not running with the Werkzeug Server')
    func()

@app.route('/shutdown', methods=['POST'])
def shutdown():
    shutdown_server()
    return 'Server shutting down...\n'

@app.route("/post", methods=['POST'])
def new_message_from_ps():
    if request.headers['Content-Type'] == 'application/json':
        # print('Got json type, this should not happen!')
        return "JSON Message received."
    if request.headers['Content-Type'] == 'application/octet-stream':
        msg = struct.unpack('d', request.data)[0]
        # print('Got {}'.format(msg))
        # return jsonify({'payload': 0.0})
        return jsonify({'payload': msg})
    # print('wrong content-type')
    return "Wrong content-type"

@app.route("/")
def hello():
    return "Hello World!"

if __name__ == "__main__":
    host = "0.0.0.0"
    port = 5000
    app.run(host=host, port=port)
