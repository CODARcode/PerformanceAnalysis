import ijson
import json
import datetime

eventTypes = ['entry_exit', 'send_receive', 'counter'] # three groups
nodeID = ['0', '1', '2', '3', '4']
filename = "reduced/tau.json"
s0 = datetime.datetime.now()
for event_type in eventTypes:
    print("============================", event_type, "======================================")
    if event_type == 'send_receive':
        ts = datetime.datetime.now()
        print(ts)
        retList = []
        with open(filename) as json_file:
            objects = ijson.items(json_file, 'trace events.item')
            entries = (o for o in objects if o['event-type'] == 'send' or o['event-type'] == 'receive')

            c = 0
            for entry in entries:
                if c % 10000 == 0:
                    print(c, entry)
                c += 1
                retList.append(entry)
            print('============================Total:', c, '======================================')
        with open("trace_" + event_type + ".json", "w") as outfile:
            json.dump(retList, outfile)
        te = datetime.datetime.now()
        print(te)
        print(te - ts)
    elif event_type == 'counter':
        ts = datetime.datetime.now()
        print(ts)
        retList = []
        with open(filename) as json_file:
            objects = ijson.items(json_file, 'trace events.item')
            entries = (o for o in objects if o['event-type'] == 'counter')

            c = 0
            for entry in entries:
                if c % 10000 == 0:
                    print(c, entry)
                c += 1
                retList.append(entry)
            print('============================Total:', c, '======================================')
        with open("trace_" + event_type + ".json", "w") as outfile:
            json.dump(retList, outfile)
        te = datetime.datetime.now()
        print(te)
        print(te - ts)
    else:
        for node_id in nodeID:
            print("============================", event_type, ",", node_id, "======================================")
            ts = datetime.datetime.now()
            print(ts)
            retList = []
            with open(filename) as json_file:
                objects = ijson.items(json_file, 'trace events.item')
                entries = (o for o in objects if (o['event-type'] == 'entry' or o['event-type'] == 'exit') and
                           'node-id' in o and o['node-id'] == node_id)
                c = 0
                for entry in entries:
                    if c % 10000 == 0:
                        print(c, entry)
                    c += 1
                    retList.append(entry)
                print('=================================Total:', c, '======================================')
            with open("trace_" + event_type + "_" + node_id + ".json", "w") as outfile:
                json.dump(retList, outfile)
            te = datetime.datetime.now()
            print(te)
            print(te - ts)
s1 = datetime.datetime.now()
print(s1 - s0)


