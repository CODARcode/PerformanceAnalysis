#An example python program that inserts an artificial anomaly with a regular period
import time
from mpi4py import MPI

def myfunc(sleep_time):
    print("Hello world (%f s)" % sleep_time)
    time.sleep(sleep_time)


base_time=0.3
anom_time=10.0
anom_freq=40
cycles=100
for i in range(cycles):
    if i > 0 and i % anom_freq == 0:
        print("Anomaly")
        myfunc(anom_time)
    else:
        print("Normal")
        myfunc(base_time)


