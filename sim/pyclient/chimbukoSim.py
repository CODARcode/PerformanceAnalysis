import socket
import sys
import json
from time import sleep
import random 



class chimbukoSim:
    def __init__(self, host="0.0.0.0", port=5005):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.host = host
        self.port = port

    def connect(self):
        self.socket.connect((self.host, self.port))

    ''' data is dictionary '''
    def set_data(self, data):
        self.data = data

    def send(self, data=None):
        j = None
        if data is None:
            j = json.dumps(self.data, indent=4)
        else:
            j = json.dumps(data, indent=4)
        print('Sending data')
        self.socket.send(b'{}'.format(j))
        print('Response received from Chimbuko server : ')
        print(str(self.socket.recv(1000)))
        #sleep(1)

    def initialize(self, alg="sstd", sigma=12.0, hbos_thres=0.99, ranks=1):
        d = {}
        d["algorithm"] = alg
        if alg == "sstd":
            d["sigma"] = sigma
        elif alg == "hbos":
            d["hbos_thres"] = hbos_thres
        d["n_ranks"] = ranks
        
        j = json.dumps(d, indent=4)
        self.socket.send(b'{}'.format(j))
        print('Response received from Chimbuko server : ')
        ack = str(self.socket.recv(1000))
        print(ack)
        sleep(1)


