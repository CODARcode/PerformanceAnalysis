import ijson
import json
import datetime
import sys

eventTypes = ['entry_exit']#, 'send_receive', 'counter'] # three groups
#nodeID = ['0', '1', '2', '3', '4']
filename = sys.argv[1] + '.json'
out_fn = 'ee-' + sys.argv[1] + '-ee.json'
s0 = datetime.datetime.now()
for event_type in eventTypes:
            ts = datetime.datetime.now()
            print(ts)
            retList = []
            with open(filename) as json_file:
                objects = ijson.items(json_file, 'trace events.item')
                entries = (o for o in objects if (o['event-type'] == 'entry' or o['event-type'] == 'exit') and
                           'node-id' in o)
                c = 0
                for entry in entries:
                    if c % 10000 == 0:
                        print(c, entry)
                    c += 1
                    retList.append(entry)
                print('=================================Total:', c, '======================================')
            with open(out_fn, "w") as outfile:
                json.dump(retList, outfile)
            te = datetime.datetime.now()
            print(te)
            print(te - ts)
s1 = datetime.datetime.now()
print(s1 - s0)


