from __future__ import print_function
import math
import sys
import os
import re
import datetime

class rankData:
    def __init__(self):
        self.rank = -1
        self.ndata = 0
        self.times = []
        self.io_steps = []
        self.incomplete = []
        self.total = []
        self.serviced = []
        self.new_req = []
    def parse(self, rank, stub):
        self.rank = rank

        filename = "%s.%d.log" % (stub,rank)
        if os.path.exists(filename) == False:
            print("File %s does not exist!" % filename)
            sys.exit(1)

        fh = open(filename, 'r')
        for line in fh:
            m = re.search(r'(\d+)\/(\d+)\/(\d+)\s(\d+)\:(\d+)\:(\d+)\sio_steps\:(\d+)\sprovdb_incomplete_async_sends\:(\d+)\sprovdb_total_async_send_calls\:(\d+)', line)
            if m:
                year = int(m.group(1))
                month = int(m.group(2))
                day = int(m.group(3))
                hour = int(m.group(4))
                minute = int(m.group(5))
                second = int(m.group(6))
               
                ts = (datetime.datetime(year,month,day,hour,minute,second) - datetime.datetime(1970,1,1)).total_seconds()
 
                lsteps = int(m.group(7))
                linc = int(m.group(8)) #incomplete sends at current time
                ltot = int(m.group(9))  #total reqs up to current time

                #How many did we service since the previous step?
                serviced = 0
                new_req = 0
                if(self.ndata == 0):
                    new_req = ltot
                    serviced = ltot - linc
                else:
                    #Total number of requests that need servicing in this step is the number left over from the last plus the number of new requests
                    new_req = ltot - self.total[-1]
                    carryover = self.incomplete[-1]
                    serviced = carryover + new_req - linc
               
                self.times.append(ts)
                self.io_steps.append(lsteps)
                self.incomplete.append(linc)
                self.total.append(ltot)
                self.serviced.append(serviced)
                self.new_req.append(new_req)
                self.ndata += 1



                #print("%d %d %d %d" % (ts,lsteps,linc,ltot))
                
#Ranks will start at slightly different times but same cadence so we just combine by the data point index
class shardData:
    def __init__(self, tsep, ndata_largest):
        self.incomplete = []
        self.total = []
        self.t = []  #start time of bin (not including t_start offset)
        self.serviced = []
        self.new_req = []
        for b in range(ndata_largest):
            self.incomplete.append(0)
            self.total.append(0)
            self.t.append( b*tsep )
            self.serviced.append(0)
            self.new_req.append(0)

    #r is a rankData object
    def insert(self, r):
        for i in range(len(r.times)):
            self.incomplete[i] += r.incomplete[i]
            self.total[i] += r.total[i]
            self.serviced[i] += r.serviced[i]
            self.new_req[i] += r.new_req[i]


def readShardMap(filename, ranks, shards):
    if os.path.exists(filename) == False:
        print("File %s does not exist!" % filename)
        sys.exit(1)

    fh = open(filename, 'r')
    out = []
    for i in range(ranks):
        line = fh.readline()
        shard = int(line)
        if shard < 0 or shard >= shards:
            print("Invalid shard index %d" % shard)
            sys.exit(1)
        out.append(shard)
    fh.close()
    return out
        



argc = len(sys.argv)
if argc < 4:
    print("Expect <script.py> <ranks> <shards> <directory> <options>")
    sys.exit(1)

ranks=int(sys.argv[1])
shards=int(sys.argv[2])
stub=sys.argv[3]

shard_map=None
for i in range(4,argc):
    if sys.argv[i] == "-load_shard_map":        
        shard_map = readShardMap(sys.argv[i+1], ranks, shards)
        
print("Ranks %d, shards %d" % (ranks,shards))


shard_data = [ [] for s in range(shards) ]   #[shard][rank within shard]
for r in range(ranks):
    if shard_map == None:
        shard = r % shards
    else:
        shard = shard_map[r]
        
    print("Rank %d shard %d" % (r,shard))
    d = rankData()
    d.parse(r, stub)
    shard_data[shard].append(d)

for s in range(shards):
    nranks = len(shard_data[s])
    print("Shard %d has #ranks %d" % (s,nranks))

#Find average time between writes
tt=0
nn=0

for s in range(shards):
    for r in range(len(shard_data[s])):
        prev = shard_data[s][r].times[0]
        for i in range(1,shard_data[s][r].ndata):
            t = shard_data[s][r].times[i]
            diff = t-prev
            tt += diff
            nn += 1
            prev = t

tsep = float(tt)/nn

print("Avg seconds between writes: %f" % (tsep))

#Find earliest and latest timestamp
t_earliest = shard_data[0][0].times[0]
t_latest = shard_data[0][0].times[-1]
ndata_largest = 0 #largest number of data points over ranks
for s in range(0, shards):
    for r in range(len(shard_data[s])):
        if shard_data[s][r].times[0] < t_earliest:
            t_earliest = shard_data[s][r].times[0]
        if shard_data[s][r].times[-1] > t_latest:
            t_latest = shard_data[s][r].times[-1]
        if len(shard_data[s][r].times) > ndata_largest:
            ndata_largest = len(shard_data[s][r].times)

print("Earliest and latest timestamp %d %d" % (t_earliest, t_latest) )

#Compute rates by shard
#Use only the data in the middle 50% of the run to avoid the startup and shutdown effects
# avg_rate_by_shard = [ None for s in range(shards) ]

# avg_rank_rate_by_shard = [ None for s in range(shards) ]
# max_rank_rate_by_shard = [ None for s in range(shards) ]
# min_rank_rate_by_shard = [ None for s in range(shards) ]
# stddev_rank_rate_by_shard = [ None for s in range(shards) ]


# for s in range(shards):
#     rates = []
#     for rdata in shard_data[s]:
#         start = int(rdata.ndata / 4)
#         end = int(3*rdata.ndata/4)

#         print("Start %d end %d" % (start,end))
#         for i in range(start, end):
#             tstep=0
#             if(i==0):
#                 #Assume tstep is uniform
#                 tstep = rdata.times[1] - rdata.times[0]
#             else:
#                 tstep = rdata.times[i] - rdata.times[i-1]
#             rates.append( float(rdata.serviced[i])/tstep )
#             print("%f " % rates[-1],end='')
#         print("")


#     avg_rate = 0
#     avg_rate2 = 0
#     min_rate = rates[0]
#     max_rate = rates[0]
#     for r in rates:
#         avg_rate += r / len(rates)
#         avg_rate2 += r*r / len(rates)
#         if r < min_rate:
#             min_rate = r
#         if r > max_rate:
#             max_rate = r

#     avg_rank_rate_by_shard[s] = avg_rate
#     stddev_rank_rate_by_shard[s] = math.sqrt( avg_rate2 - avg_rate * avg_rate )
#     min_rank_rate_by_shard[s] = min_rate
#     max_rank_rate_by_shard[s] = max_rate

#     #This is the average rate per shard, multiply by number of ranks to get average rate for shard
#     avg_rate_by_shard[s] = avg_rate * len(shard_data[s])
    
# print("Avg per-rank rates by shard (middle 50%)")
# for s in range(shards):
#     print("(%.1f +- %.1f) " % (avg_rank_rate_by_shard[s], stddev_rank_rate_by_shard[s])  , end='')
# print("")

# print("Min/max per-rank rates by shard (middle 50%)")
# for s in range(shards):
#     print("(%.1f %.1f)" % (min_rank_rate_by_shard[s], max_rank_rate_by_shard[s]) , end='')
# print("")

# print("Average rates by shard (middle 50%): ",end='')
# for s in range(shards):
#     print("%f " % avg_rate_by_shard[s], end='')
# print("")


#Bin data over time for all ranks in a shard because the timestamps won't match
print("Combining ranks by shard")

shard_data_combined = [ shardData(tsep, ndata_largest) for i in range(shards) ]
for s in range(shards):
  for r in shard_data[s]:
      shard_data_combined[s].insert(r)

print("Number of requests serviced and number of new requests by shard as a function of time")
for b in range(ndata_largest):
    print("%d " % shard_data_combined[0].t[b], end='')
    for s in range(shards):
        print("%d/%d " % (shard_data_combined[s].serviced[b], shard_data_combined[s].new_req[b]), end='')
    print("")


print("Number of requests/second serviced and new requests per second in time bin by shard")
for b in range(ndata_largest):
    print("%d " % shard_data_combined[0].t[b], end='')
    for s in range(shards):
        print("%.1f/%.1f " % ( float(shard_data_combined[s].serviced[b])/tsep,  float(shard_data_combined[s].new_req[b])/tsep   )  , end='')
    print("")


print("Defecit/surplus number of requests serviced in time bin (serviced - new) by shard")
for b in range(ndata_largest):
    print("%d " % shard_data_combined[0].t[b], end='')
    for s in range(shards):
        print("%d " % (shard_data_combined[s].serviced[b]- shard_data_combined[s].new_req[b]), end='')
    print("")

#Add the defecits to a file that can be parsed by xmgrace
f = open("defecits.dat", 'w')
for s in range(shards):
    for b in range(ndata_largest):
        f.write("%d %d\n" % (shard_data_combined[s].t[b],   shard_data_combined[s].serviced[b]- shard_data_combined[s].new_req[b]) )
    f.write("\n")
f.close()

print("Defecit/surplus number of requests serviced in time bin (serviced - new) total")
f = open("defecits_total.dat", 'w')
for b in range(ndata_largest):
    defecit=0
    for s in range(shards):
        defecit += shard_data_combined[s].serviced[b]- shard_data_combined[s].new_req[b]
    rr="%d %d" % (shard_data_combined[0].t[b], defecit)
    print(rr)
    f.write(rr + "\n")
f.close()


print("Aggregate Defecit/surplus number of requests serviced as a function of time (sum of defecits to now)")
f = open("defecits_aggregate_total.dat", 'w')
defecit_sum=0
for b in range(ndata_largest):
    defecit=0
    for s in range(shards):
        defecit += shard_data_combined[s].serviced[b]- shard_data_combined[s].new_req[b]
    defecit_sum += defecit
    rr="%d %d" % (shard_data_combined[0].t[b], defecit_sum)
    print(rr)
    f.write(rr + "\n")
f.close()


print("Requests serviced in time bin total")
f = open("serviced_total.dat", 'w')
for b in range(ndata_largest):
    serviced=0
    for s in range(shards):
        serviced += shard_data_combined[s].serviced[b]
    rr="%d %d" % (shard_data_combined[0].t[b], serviced)
    print(rr)
    f.write(rr + "\n")
f.close()


print("Aggregate requests serviced as a function of time (sum of serviced to now)")
f = open("serviced_aggregate_total.dat", 'w')
serviced_sum=0
for b in range(ndata_largest):
    serviced=0
    for s in range(shards):
        serviced += shard_data_combined[s].serviced[b]
    serviced_sum += serviced
    rr="%d %d" % (shard_data_combined[0].t[b], serviced_sum)
    print(rr)
    f.write(rr + "\n")
f.close()


print("Incomplete requests as a function of time total")
f = open("incomplete_total.dat", 'w')
for b in range(ndata_largest):
    incomplete=0
    for s in range(shards):
        incomplete += shard_data_combined[s].incomplete[b]
    rr="%d %d" % (shard_data_combined[0].t[b], incomplete)
    print(rr)
    f.write(rr + "\n")
f.close()



######################## rank 0 analysis #############################
rank0_shard=-1
rank0_elem=-1
for s in range(shards):
    for r in range(len(shard_data[s])):
        if(shard_data[s][r].rank == 0):
            rank0_shard=s
            rank0_elem=r
            
rank0 = shard_data[rank0_shard][rank0_elem]

if rank0.rank != 0:
    print("Error: expect rank 0, got %d" % rank0.rank)
    sys.exit(1)

print("Defecit/surplus number of requests serviced since last report for rank 0 (serviced - new) total")
f = open("defecits_total_rank0.dat", 'w')
for d in range(len(rank0.times)):
    defecit= rank0.serviced[d]- rank0.new_req[d]
    rr="%d %d" % (rank0.times[d] - rank0.times[0], defecit)
    print(rr)
    f.write(rr + "\n")
f.close()


print("Aggregate Defecit/surplus number of requests serviced as a function of time for rank 0 (sum of defecits to now)")
f = open("defecits_aggregate_total_rank0.dat", 'w')
defecit_sum=0
for d in range(len(rank0.times)):
    defecit= rank0.serviced[d]- rank0.new_req[d]
    defecit_sum += defecit
    rr="%d %d" % (rank0.times[d] - rank0.times[0], defecit_sum)
    print(rr)
    f.write(rr + "\n")
f.close()

print("Incomplete requests as a function of time for rank 0")
f = open("incomplete_total_rank0.dat", 'w')
for d in range(len(rank0.times)):
    incomplete= rank0.incomplete[d]
    rr="%d %d" % (rank0.times[d] - rank0.times[0], incomplete)
    print(rr)
    f.write(rr + "\n")
f.close()


