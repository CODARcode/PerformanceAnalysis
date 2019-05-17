"""
Simple Adios2 SST writer

To test SST, it will read variable and attribute from a BP file and write it via SST step by step.
For the attribute, with BP file, we can get all attributes at the first step. As the result,
a reader via SST will get all attribute at the first step. But, in reality, the available
attributes can vary in each step.

NOTE: Attributes are belonging under IO class in ADIOS. So, attribute registered at step i might
NOTE: be also available at step j (j > i). But, it may be not true for the reverse way.

Author(s)
    Sungsoo Ha (sungsooha@bnl.gov)

Created:
    March 12, 2019

Last modified:
    March 20, 2019   support MPI
"""
import pyAdios as ADIOS
from mpi4py import MPI
import numpy as np
import sys

# data
VAR_SCALAR = [
    'program_count', 'comm_rank_count', 'thread_count',
    'event_type_count', 'timer_count', 'timer_event_count',
    'counter_count', 'counter_event_count', 'comm_count'
]
VAR_ARRAY = [
    ('event_timestamps', 'timer_event_count', 6),
    ('counter_values', 'counter_event_count', 6),
    ('comm_timestamps', 'comm_count', 8)
]

comm = MPI.COMM_WORLD
rank = comm.Get_rank()
size = comm.Get_size()

#inputFile = '../data/shortdemo/tau-metrics.bp'
#inputFile = '../data/longdemo/tau-metrics-nwchem.bp'
inputFile = sys.argv[1]
outputFile = 'tau-metrics-{:d}.bp'.format(rank)

# reader (from bp file for test)
reader = ADIOS.pyAdios('BP')
reader.open(inputFile, ADIOS.OpenMode.READ, MPI.COMM_SELF)
status = reader.current_step()

# writer (SST)
writer = ADIOS.pyAdios('SST', "QueueLimit=1")
writer.open(outputFile, ADIOS.OpenMode.WRITE, MPI.COMM_SELF)

# attribute holder
att_holder = dict()

# Write data one by one from the BP file
while status >= 0:
    print("[WRITER][{:d}]Frame-{:d}".format(rank, status))
    bWrite = False

    # ------------------------------------------------------------------------
    # Read variable
    #print('\n\nREAD...')
    scalar_data = []
    for name in VAR_SCALAR:
        scalar_data.append(reader.read_variable(name))

    array_data = []
    for name, ydim_name, xdim in VAR_ARRAY:
        ydim = scalar_data[VAR_SCALAR.index(ydim_name)]
        if ydim is not None:
            array_data.append(reader.read_variable(name, count=[ydim[0], xdim]))

            if name == 'event_timestamps':
                print('[W][{:d}][{:d}] event_timestamps: {}'.format(
                    rank, status, array_data[-1].shape))
        else:
            array_data.append(None)


    # ------------------------------------------------------------------------
    # Write variable
    #print('\n\nWRITE...')
    for name, data in zip(VAR_SCALAR, scalar_data):
        if data is not None:
            writer.write_variable(name, data)
            bWrite = True

    for (name, _, _), data in zip(VAR_ARRAY, array_data):
        if data is not None:
            writer.write_variable(name, data, [0,0], data.shape)
            bWrite = True

    # ------------------------------------------------------------------------
    # Read & Write attributes
    for att_name, att in reader.available_attributes().items():
        if att_holder.get(att_name, None) is None:
            writer.write_attribute(att_name, att.value())
            att_holder[att_name] = att.value()
            bWrite = True

    if bWrite:
        writer.write_variable('frame', np.array([status]))
        writer.end_step()
    status = reader.advance()

    # if status > 1:
    #     break

# Finalize
reader.close()
writer.close()