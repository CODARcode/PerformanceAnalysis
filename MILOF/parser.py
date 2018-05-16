import adios as ad
import numpy as np
import scipy.io as sio

#print (ad.__version__)

method = "BP"
init = "verbose=3;"
num_steps = 1

ad.read_init(method, parameters=init)

f = ad.file("data/tau-metrics-updated/tau-metrics.bp", method, is_stream=True, timeout_sec=10.0)

i = 0
attr = np.array(['prog_names', 'comm_ranks', 'threads', 'event_types', 'func_names', 'counters', 'counter_value', 'event_types', 'tag', 'partner', 'num_bytes', 'timestamp']).reshape(1, 12)
# prog_names is the indexed set of program names in the attributes
# comm_ranks is the MPI rank
# threads is the thread ID (rank)
# event_types is the indexed set of event types in the attributes
# func_names is the indexed set of timer names in the attributes
# counters is the indexed set of counter names in the attributes
# tag is the MPI tag
# partner is the other side of a point-to-point communication
# num_bytes is the amount of data sent

data = attr

while True:
	print(">>> step:", i)

	vname = 'event_timestamps'
	if vname in f.vars:
		var = f.var[vname]
		num_steps = var.nsteps
		event  = var.read(nsteps=num_steps)
		data_event = np.zeros((event.shape[0], 12), dtype=object) + np.nan
		data_event[:, 0:5] = event[:, 0:5]
		data_event[:, 11] = event[:, 5]
		data = np.concatenate((data, data_event), axis=0)

	vname = 'counter_values'
	if vname in f.vars:
		var = f.var[vname]
		num_steps = var.nsteps
		counter  = var.read(nsteps=num_steps)
		data_counter = np.zeros((counter.shape[0], 12), dtype=object) + np.nan
		data_counter[:, 0:3] = counter[:, 0:3]
		data_counter[:, 5:7] = counter[:, 3:5]
		data_counter[:, 11] = counter[:, 5]
		data = np.concatenate((data, data_counter), axis=0)
	
	vname = 'comm_timestamps'
	if vname in f.vars:
		var = f.var[vname]
		num_steps = var.nsteps
		comm  = var.read(nsteps=num_steps)
		data_comm = np.zeros((comm.shape[0], 12), dtype=object) + np.nan
		data_comm[:, 0:4] = comm[:, 0:4]
		data_comm[:, 8:11] = comm[:, 4:7]
		data_comm[:, 11] = comm[:, 7]
		data = np.concatenate((data, data_comm), axis=0)

	print("data size =", data.shape)
	print(">>> advance to next step ... ")
	if (f.advance() < 0):
		break

	i += 1

f.close()

print(">>> finish passing data.")
print(">>> sort data by timestamp...")
data = np.concatenate((attr, data[data[1:, 11].argsort()+1]), axis=0)
print(">>> show first 20 rows:")
print(data[0:20, :])
print(">>> Write complete data to a csv file.")
np.savetxt("data.csv", data[0:10000, :], delimiter=",", fmt="%s")
print(">>> Complete.")
