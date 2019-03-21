"""
simpler reader

(will be deleted)

Author(s)
    Sungsoo Ha (sungsooha@bnl.gov)

Created:
    March 12, 2019
"""
import pyAdios as ADIOS
from mpi4py import MPI

comm = MPI.COMM_WORLD
rank = comm.Get_rank()
size = comm.Get_size()

reader = ADIOS.pyAdios('SST')
reader.open('tau-metrics-{:d}.bp'.format(rank), ADIOS.OpenMode.READ, MPI.COMM_SELF)

status = reader.current_step()
while status >= 0:


    frame = reader.stream.read('frame')
    print('[READER][{:d}] Frame-{:d}, {}'.format(rank, status, frame))

    vars = reader.available_variables()
    # for name, info in vars.items():
    #     print(name, info.shape)
    var = vars['event_timestamps']
    shape = var.shape

    #data = reader.read_variable('event_timestamps')
    #reader.stream.read(name, start, count)
    data = reader.stream.read('event_timestamps', [0, 0], shape)
    #data = reader.fh.read_variable('event_timestamps')
    print('[R][{:d}][{:d}] event_timestamps: {}'.format(rank, status, data.shape))


    status = reader.advance()
reader.close()

# from mpi4py import MPI
# import adios2
#
# comm = MPI.COMM_WORLD
# rank = comm.Get_rank()
# size = comm.Get_size()
#
# fr = adios2.open('tau-metrics.bp',"r", comm, 'SST')
# fr_step = fr.__next__()
# step = fr_step.current_step()
#
# try:
#     while True:
#         indata = fr_step.read("event_timestamps")
#         print(step, indata.shape)
#         fr_step = fr.__next__()
#         step = fr_step.current_step()
# except StopIteration:
#     pass
# fr.close()

# for fr_step in fr:
#     step = fr_step.current_step()
#     # step_vars = fr_step.available_variables()
#     # for name, info in step_vars.items():
#     #     print("variable name: ", name)
#     #     for key, value in info.items():
#     #         print("\t" + key + ": " + value)
#     #     print("\n")
#     indata = fr_step.read("event_timestamps")
#     print(step, indata.shape)
#
#     fr_step.end_step()


# def get_read_stream(fn='tau-metrics.bp'):
#     with adios2.open(fn, "r", comm, 'SST') as fr:
#         for fr_step in fr:
#             yield fr_step
#             # step = fr_step.current_step()
#             # step_vars = fr_step.available_variables()
#             # for name, info in step_vars.items():
#             #     print("variable name: ", name)
#             #     for key, value in info.items():
#             #         print("\t" + key + ": " + value)
#             #     print("\n")
#             # indata = fr_step.read("event_timestamps")
#             # print(indata.shape)
#
#             fr_step.end_step()
#     yield None
#
# stream = get_read_stream()
# fr_step = next(stream)
# while fr_step is not None:
#     print(fr_step.current_step())
#     data = fr_step.read("event_timestamps")
#     print(data.shape)
#     fr_step = next(stream)
