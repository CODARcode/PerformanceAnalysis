from mpi4py import MPI
import sleeper

def myfunc(sleep_time):
    print("Hello world (Python) (time=%f s)" % sleep_time)
    sleeper.sleep_c(sleep_time)

for i in range(39):
    myfunc(1)

myfunc(10)

