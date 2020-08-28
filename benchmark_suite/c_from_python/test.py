from mpi4py import MPI
import sleeper

def myfunc(sleep_time):
    sleeper.sleep_c(sleep_time)
    print("Hello world")


for i in range(39):
    sleeper.sleep_c(1)
    print("Hello world")
sleeper.sleep_c(20)
print("ANOM")
