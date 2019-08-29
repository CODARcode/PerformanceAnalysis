from flask import Flask, request, jsonify
import time

app = Flask(__name__)

n_posts = 0 # total number of post requests
t_start = 0 # time of the first post request

def shutdown_server():
    func = request.environ.get('werkzeug.server.shutdown')
    if func is None:
        raise RuntimeError('Not running with the Werkzeug Server')
    func()

@app.route('/shutdown', methods=['POST'])
def shutdown():
    t_end = time.time()
    if n_posts:
        print('Shutdown request.')
        print('# posts: ', n_posts)
        print('total process time: {} sec'.format(t_end - t_start))
        print('request rate: {} per sec'.format(
            (t_end - t_start) / n_posts
        ))

    shutdown_server()
    return 'Server shutting down...\n'

@app.route("/post", methods=['POST'])
def new_message_from_ps():
    global n_posts, t_start
    if request.headers['Content-Type'] == 'application/json':
        if n_posts == 0:
            t_start = time.time()
        n_posts = n_posts + 1
        return "OK"
    return "Wrong content-type"

@app.route("/")
def hello():
    return "Hello World!"

if __name__ == "__main__":
    import sys
    host = "0.0.0.0"
    port = 5000
    log = ''

    if len(sys.argv):
        host = sys.argv[1]
        port = int(sys.argv[2])

    if len(sys.argv) > 3:
        log = sys.argv[3]

    if len(log):
        try:
            f = open(log, 'w')
            sys.stdout = sys.stderr = f
        except IOError as e:
            print('IOError: ', e)

    app.run(host=host, port=port, threaded=True)
