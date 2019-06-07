from mpi4py import MPI
import adios2
import pyAdios as ADIOS
from utils.testdata import TestTraceData

if __name__ == '__main__':
    comm = MPI.COMM_WORLD
    rank = comm.Get_rank()
    size = comm.Get_size()

    use_pyadios = True
    n_steps = 100
    data = TestTraceData()

    outputFile = "test-{:d}.bp".format(rank)

    if use_pyadios:
        writer = ADIOS.pyAdios('SST', 'QueueLimit=1')
        writer.open(outputFile, ADIOS.OpenMode.WRITE, MPI.COMM_SELF)
        for step in range(n_steps):
            # print("[{:d}]WRITE: {:d}".format(rank, step))
            data.update(step, rank)
            writer.write_variable("program_count", data.program_count)
            writer.write_variable("comm_rank_count", data.comm_rank_count)
            writer.write_variable("thread_count", data.thread_count)
            writer.write_variable("event_type_count", data.event_type_count)
            writer.write_variable("timer_count", data.timer_count)
            writer.write_variable("timer_event_count", data.timer_event_count)
            writer.write_variable("counter_count", data.counter_count)
            writer.write_variable("counter_event_count", data.counter_event_count)
            writer.write_variable("comm_count", data.comm_count)

            writer.write_variable(
                "event_timestamps", data.event_timestamps,
                [0,0], data.event_timestamps.shape
            )
            writer.write_variable(
                "counter_values", data.counter_values,
                [0,0], data.counter_values.shape
            )
            writer.write_variable(
                "comm_timestamps", data.comm_timestamps,
                [0,0], data.comm_timestamps.shape
            )

            writer.end_step()
        writer.close()
    else:
        with adios2.open(outputFile, "w", MPI.COMM_SELF, "SST") as fw:
            fw.set_parameters({'QueueLimit':'1'})
            for step in range(n_steps):
                # print("[{:d}]WRITE: {:d}".format(rank, step))
                data.update(step, rank)
                #print('write: ', step, rank, data.program_count)

                fw.write("program_count", data.program_count)
                fw.write("comm_rank_count", data.comm_rank_count)
                fw.write("thread_count", data.thread_count)
                fw.write("event_type_count", data.event_type_count)
                fw.write("timer_count", data.timer_count)
                fw.write("timer_event_count", data.timer_event_count)
                fw.write("counter_count", data.counter_count)
                fw.write("counter_event_count", data.counter_event_count)
                fw.write("comm_count", data.comm_count)

                fw.write("event_timestamps", data.event_timestamps,
                         data.event_timestamps.shape, [0,0], data.event_timestamps.shape)
                fw.write("counter_values", data.counter_values,
                         data.counter_values.shape, [0,0], data.counter_values.shape)
                fw.write("comm_timestamps", data.comm_timestamps,
                         data.comm_timestamps.shape, [0,0], data.comm_timestamps.shape)

                fw.end_step()