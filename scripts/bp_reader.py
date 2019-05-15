"""
simpler reader

Author(s)
    Sungsoo Ha (sungsooha@bnl.gov)

Created:
    March 12, 2019
"""
import pyAdios as ADIOS
from definition import *

reader = ADIOS.pyAdios('BP')
reader.open('../data/mpi/tau-metrics-0.bp', ADIOS.OpenMode.READ)

N = 20

funMap = dict()
eventType = dict()
for name, info in reader.available_attributes().items():
    is_func = name.startswith('timer')
    is_event = name.startswith('event_type')
    if is_func or is_event:
        key = int(name.split()[1])
        val = str(info.value())
        if is_func: funMap[key] = val
        else: eventType[key] = val


status = reader.current_step()
while status >= 0:
    print('\nStep: ', status)
    vars = reader.available_variables()
    var = vars['event_timestamps']
    shape = var.shape

    ydim = reader.stream.read('timer_event_count')[0]
    data = reader.stream.read('event_timestamps', [0, 0], [ydim, 6])
    print('event_timestamps: ', data.shape)
    print('First 5 timers: ')
    for d in data[:N]:
        print("{}: {}, {}".format(
            d[FUN_IDX_TIME], eventType[d[FUN_IDX_EE]], funMap[d[FUN_IDX_FUNC]]
        ))
    print('Last 5 timers: ')
    for d in data[-N:]:
        print("{}: {}, {}".format(
            d[FUN_IDX_TIME], eventType[d[FUN_IDX_EE]], funMap[d[FUN_IDX_FUNC]]
        ))

    ydim = reader.stream.read('comm_count')[0]
    data = reader.stream.read('comm_timestamps', [0, 0], [ydim, 8])
    print('comm_timestamps: ', data.shape)
    print('First 5 comm: ')
    for d in data[:N]:
        print("{}: {}".format(
            d[COM_IDX_TIME], eventType[d[COM_IDX_SR]]
        ))
    print('Last 5 comm: ')
    for d in data[-N:]:
        print("{}: {}".format(
            d[COM_IDX_TIME], eventType[d[COM_IDX_SR]]
        ))

    status = reader.advance()
reader.close()