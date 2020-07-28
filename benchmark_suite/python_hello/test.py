import time
from mpi4py import MPI

def myfunc(sleep_time):
    time.sleep(sleep_time)
    print("Hello world")

for i in range(15):
    myfunc( float(i)/10 )

