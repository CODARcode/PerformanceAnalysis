#A test application that acts to mock the receiving of data by the visualization module

from flask import Flask, request, jsonify
import time
import pprint

app = Flask(__name__)

n_posts = 0 # total number of post requests
t_start = 0 # time of the first post request
print_msg = False #whether to print the output to stdout (or log if -log option in use)

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
        if print_msg == True:
            pp = pprint.PrettyPrinter(indent=2)
            pp.pprint(request.get_json())

        if n_posts == 0:
            t_start = time.time()
        n_posts = n_posts + 1
        return "OK"
    return "Wrong content-type"

#A path that returns a copy of a JSON message
@app.route("/bouncejson", methods=['POST'])
def bouncejson():
    if request.headers['Content-Type'] == 'application/json':
        return jsonify(request.get_json())
    return "Wrong content-type"



@app.route("/")
def hello():
    return "Hello World!"

if __name__ == "__main__":
    import sys
    host = "0.0.0.0"
    port = 5000

    i=1
    while i < len(sys.argv):
        if sys.argv[i] == "-host":
            host = sys.argv[i+1]
            print("Set host to %s",host)
            i = i+2
        elif sys.argv[i] == "-port":
            port = sys.argv[i+1]
            print("Set port to %s",port)
            i = i+2
        elif sys.argv[i] == "-log":
            log = sys.argv[i+1]
            if len(log):
                try:
                    f = open(log, 'w')
                    sys.stdout = sys.stderr = f
                except IOError as e:
                    print('IOError: ', e)
                print("Logging output to %s",log)
            i = i+2
        elif sys.argv[i] == "-print_msg":
            print("Printing JSON output")
            print_msg = True
            i = i+1
        else:
            print("Invalid argument: %s", sys.argv[i])


    app.run(host=host, port=port, threaded=True)
