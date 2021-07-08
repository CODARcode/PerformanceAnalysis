import socket
import sys
import json
from time import sleep
import random 

HOST, PORT = "0.0.0.0", 5005  #"localhost", 8899

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

s.connect((HOST, PORT))

data = ""
data_list = []
data_dic = {}

for x in range(10):
    print("pyclient: Step 1")
    #data += str(x+1)
    #data_list.append(x)
    #if 'data' in data_dic:
    #    data_dic['data'].append(x)
    #else:
    #    data_list.append(x)
    #    data_dic['data'] = data_list
    data_dic['data'] = [random.randrange(1, 50, 1) for i in range(499)]
    data_dic['data'].append(400) #Anomaly
    
    j = json.dumps(data_dic, indent=4)
    print("Sending Data : ",j)
    s.send(b'{}'.format(j))  #(data_dic))  #(b'Hello')
    print("pyclient: Step 2")
    print(str(s.recv(1000)))
    print(x)
    sleep(3)
