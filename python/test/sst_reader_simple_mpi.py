"""
simpler reader

(will be deleted)

Author(s)
    Sungsoo Ha (sungsooha@bnl.gov)

Created:
    March 12, 2019
"""

from mpi4py import MPI
import adios2
import os
import time
import numpy as np

comm = MPI.COMM_WORLD
rank = comm.Get_rank()
size = comm.Get_size()

fn = 'tau-metrics-{:d}.bp'.format(rank)
while not os.path.exists(fn + '.sst'):
    print("[{:d}] Waiting for {}!".format(rank, fn))
    time.sleep(1)
print("[{:d}] Find {}!".format(rank, fn))
fr = adios2.open(fn, 'r', MPI.COMM_SELF, 'SST')
print("[{:d}] start...".format(rank))
comm.Barrier()

try:
    while True:
        fr_step = fr.__next__()
        step_vars = fr_step.available_variables()

        step = fr_step.current_step()
        indata = fr_step.read("event_timestamps")
        print(rank, step, indata.shape)
        fr_step.end_step()

except StopIteration:
    print('[{:d}] stop iteration'.format(rank))
    pass
except Exception as e:
    print('[{:d}] some error: {}'.format(rank, e))


fr.close()







