import pyAdios as ADIOS

reader = ADIOS.pyAdios('SST')
reader.open("test.bp", ADIOS.OpenMode.READ)

status = reader.current_step()
while status >= 0:
    print('Step: ', status)
    status = reader.advance()
reader.close()

