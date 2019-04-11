from mpi4py import MPI
import adios2
import pyAdios as ADIOS
from utils.testdata import TestTraceData

def show_vars(vars:dict):
    for name, info in vars.items():
        print("variable name: ", name)
        for key, value in info.items():
            print("\t" + key + ": " + value)
        print("\n")

def check(name, data, ref):
    if not (data == ref).all():
        # print("data:")
        # print(data)
        # print("ref")
        # print(ref)
        raise ValueError("{:s} read failed".format(name))

if __name__ == '__main__':
    comm = MPI.COMM_WORLD
    rank = comm.Get_rank()
    size = comm.Get_size()

    use_pyadios = True
    data = TestTraceData()

    inputFile = "test-{:d}.bp".format(rank)

    if use_pyadios:
        reader = ADIOS.pyAdios("SST")
        reader.open(inputFile, ADIOS.OpenMode.READ, MPI.COMM_SELF)
        step = reader.current_step()
        while step >= 0:
            # print("[{:d}]READ: {:d}".format(rank, step))
            data.update(step, rank)

            in_program_count = reader.read_variable("program_count")
            in_comm_rank_count = reader.read_variable("comm_rank_count")
            in_thread_count = reader.read_variable("thread_count")
            in_event_type_count = reader.read_variable("event_type_count")
            in_timer_count = reader.read_variable("timer_count")
            in_timer_event_count = reader.read_variable("timer_event_count")
            in_counter_count = reader.read_variable("counter_count")
            in_counter_event_count = reader.read_variable("counter_event_count")
            in_comm_count = reader.read_variable("comm_count")

            check("program_count", in_program_count, data.program_count)
            check("comm_rank_count", in_comm_count, data.comm_count)
            check("thread_count", in_thread_count, data.thread_count)
            check("event_type_count", in_event_type_count, data.event_type_count)
            check("timer_count", in_timer_count, data.timer_count)
            check("timer_event_count", in_timer_event_count, data.timer_event_count)
            check("counter_count", in_counter_count, data.counter_count)
            check("counter_event_count", in_counter_event_count, data.counter_event_count)
            check("comm_count", in_comm_count, data.comm_count)

            in_timer_event_count = in_timer_event_count[0]
            in_counter_event_count = in_counter_event_count[0]
            in_comm_count = in_comm_count[0]
            in_event_timestamps = reader.read_variable(
                "event_timestamps", [0, 0], [in_timer_event_count, 6])
            in_counter_values = reader.read_variable(
                "counter_values", [0, 0], [in_counter_event_count, 6])
            in_comm_timestamps = reader.read_variable(
                "comm_timestamps", [0, 0], [in_comm_count, 8])

            check("event_timestamps", in_event_timestamps, data.event_timestamps)
            check("counter_values", in_counter_values, data.counter_values)
            check("comm_timestamps", in_comm_timestamps, data.comm_timestamps)

            step = reader.advance()

        reader.close()

    else:
        with adios2.open(inputFile, "r", MPI.COMM_SELF, "SST") as fr:
            for fr_step in fr:

                step = fr_step.current_step()

                # print("[{:d}]READ: {:d}".format(rank, step))

                data.update(step, rank)
                #print('read: ', step, rank, data.program_count)

                step_vars = fr_step.available_variables()
                #show_vars(step_vars)

                in_program_count = fr_step.read("program_count")
                in_comm_rank_count = fr_step.read("comm_rank_count")
                in_thread_count = fr_step.read("thread_count")
                in_event_type_count = fr_step.read("event_type_count")
                in_timer_count = fr_step.read("timer_count")
                in_timer_event_count = fr_step.read("timer_event_count")
                in_counter_count = fr_step.read("counter_count")
                in_counter_event_count = fr_step.read("counter_event_count")
                in_comm_count = fr_step.read("comm_count")

                #print(step, rank, in_program_count, data.program_count)
                #print(step, rank, (in_program_count == data.program_count).all())

                check("program_count", in_program_count, data.program_count)
                check("comm_rank_count", in_comm_count, data.comm_count)
                check("thread_count", in_thread_count, data.thread_count)
                check("event_type_count", in_event_type_count, data.event_type_count)
                check("timer_count", in_timer_count, data.timer_count)
                check("timer_event_count", in_timer_event_count, data.timer_event_count)
                check("counter_count", in_counter_count, data.counter_count)
                check("counter_event_count", in_counter_event_count, data.counter_event_count)
                check("comm_count", in_comm_count, data.comm_count)

                in_timer_event_count = in_timer_event_count[0]
                in_counter_event_count = in_counter_event_count[0]
                in_comm_count = in_comm_count[0]
                in_event_timestamps = fr_step.read("event_timestamps", [0,0], [in_timer_event_count, 6])
                in_counter_values = fr_step.read("counter_values", [0,0], [in_counter_event_count,6])
                in_comm_timestamps = fr_step.read("comm_timestamps", [0,0], [in_comm_count,8])

                check("event_timestamps", in_event_timestamps, data.event_timestamps)
                check("counter_values", in_counter_values, data.counter_values)
                check("comm_timestamps", in_comm_timestamps, data.comm_timestamps)


                fr_step.end_step()

    print('[{}] SST TEST PASSED (w PSEUDO TRACE DATA)'.format(rank))
