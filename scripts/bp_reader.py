"""
simpler reader

Author(s)
    Sungsoo Ha (sungsooha@bnl.gov)

Created:
    March 12, 2019
"""
import pyAdios as ADIOS

reader = ADIOS.pyAdios('BP')
reader.open('../data/mpi/tau-metrics-0.bp', ADIOS.OpenMode.READ)

status = reader.current_step()
while status >= 0:
    print('\nStep: ', status)
    vars = reader.available_variables()
    var = vars['event_timestamps']
    shape = var.shape

    ydim = reader.stream.read('timer_event_count')[0]
    data = reader.stream.read('event_timestamps', [0, 0], [ydim, 6])
    print('event_timestamps: ', data.shape)
    print(data[:10, -1])
    break
    status = reader.advance()
reader.close()