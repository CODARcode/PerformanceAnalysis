import sys
import re
import math

argc = len(sys.argv)
if(argc != 2):
    print("Require a filename")
    sys.exit(1)

f = open(sys.argv[1], 'r')

vals = []
for line in f:
    m = re.search(r'\d+\s(\d+)', line)
    if m:
        v = m.group(1)
        #print(v)
        vals.append(v)

f.close()

#Take only the middle half of the data
ndata = len(vals)
q = int(ndata/4)
middle = vals[q:ndata-q]
nmiddle = len(middle)
print("%d vals, %d in middle half" % (ndata,nmiddle))

maxval = 0
minval = 1e6
mean = 0.0
for v in middle:
    ival = int(v)
    maxval = max(ival,maxval)
    minval = min(ival,minval)
    mean += float(v)
mean /= float(nmiddle)

var = 0.0
for v in middle:
    var += (float(v) - mean)**2
var /= float(nmiddle)

stddev = math.sqrt(var)

print( "Stats on middle half:  mean=%f std.dev=%f max=%f min=%f" % (mean,stddev,maxval,minval) )
