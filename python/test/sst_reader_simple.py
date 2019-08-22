"""
simpler reader

Author(s)
    Sungsoo Ha (sungsooha@bnl.gov)

Created:
    March 12, 2019
"""
import pyAdios as ADIOS

reader = ADIOS.pyAdios('SST')
reader.open('tau-metrics.bp', ADIOS.OpenMode.READ)

status = reader.current_step()
while status >= 0:
    print('\nStep: ', status)
    vars = reader.available_variables()
    var = vars['event_timestamps']
    shape = var.shape

    data = reader.stream.read('event_timestamps', [0, 0], shape)
    print('event_timestamps: ', data.shape)
    status = reader.advance()
reader.close()