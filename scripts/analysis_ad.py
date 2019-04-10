import os
import pickle
import configparser
import time
import numpy as np
from collections import defaultdict
from definition import *
from outlier import Outlier

def load_filenames(data_root, data_dir_prefix, file_prefix, file_suffix):
    # data directories
    data_dirs = [
        os.path.join(data_root, d) for d in os.listdir(data_root)
        if data_dir_prefix in d and \
           os.path.isdir(os.path.join(data_root, d))
    ]
    data_dirs = sorted(data_dirs)

    # parse trace file names
    trace_filenames = []
    max_n_frames = 0
    for data_dir in data_dirs:
        filenames = [
            os.path.join(data_dir, fn) for fn in os.listdir(data_dir)
            if fn.startswith(file_prefix) and fn.endswith(file_suffix)
        ]
        filenames = sorted(filenames)
        trace_filenames.append(filenames)
        max_n_frames = max(max_n_frames, len(filenames))

    return trace_filenames

def load_execdata(fn):
    if fn is None: return None
    return pickle.load(open(fn, 'rb'))

def merge_execdata(execdata_list):
    merged_data = defaultdict(list)
    for execdata in execdata_list:
        if execdata is None: continue
        for funid, exections in execdata.items():
            merged_data[funid] += exections
    return merged_data

if __name__ == '__main__':
    class mylog:
        def info(self, msg):
            print(msg)
        def debug(self, msg):
            print(msg)
    import sys


    # data_root = sys.argv[1] #'../untracked'
    # data_dir_prefix = sys.argv[2] #'tmp'
    configFile = sys.argv[1] #'chimbuko.cfg'
    # data_root = '../untracked'
    # data_dir_prefix = 'dump'
    # configFile = 'chimbuko.cfg'
    file_prefix = 'trace'
    file_suffix = 'pkl'

    config = configparser.ConfigParser(interpolation=None)
    config.read(configFile)

    output_dir = config['Visualizer']['OutputDir']
    data_root = '/' + os.path.join(*output_dir.split('/')[:-1])
    data_dir_prefix = os.path.basename(output_dir)

    h = Outlier(config, force_no_ps=True, log=mylog())

    trace_filenames = load_filenames(
        data_root, data_dir_prefix,
        file_prefix, file_suffix
    )
    n_max_frames = np.max([len(files) for files in trace_filenames])
    for i in range(len(trace_filenames)):
        n_frames = len(trace_filenames[i])
        if n_frames < n_max_frames:
            trace_filenames[i] += [None] * (n_max_frames - n_frames)


    frame_idx = 0
    g_n_abnormal_ref = 0
    g_n_abnormal_pred = 0
    g_n_abnormal_match = 0
    g_n_funcalls = 0
    g_time = 0
    for filenames in zip(*trace_filenames):
        print(filenames)
        execData = merge_execdata([load_execdata(fn) for fn in filenames])

        l_n_abnormal_ref = 0
        l_n_abnormal_pred = 0
        l_n_abnormal_match = 0
        # ----- start anomaly detection
        l_n_funcalls = 0
        t_start = time.time()
        for funid, executions in execData.items():
            l_n_funcalls += len(executions)
            h.compOutlier(executions, funid)
            labels_ref = h.getOutlier()
            labels_pred = np.array([d.label for d in executions])

            n_abnormal_ref = np.sum(labels_ref == -1)
            n_abnormal_pred = np.sum(labels_pred == -1)
            n_abnormal_match = len([
                i for i, j in zip(labels_ref, labels_pred) if i==j and i==-1 and j==-1
            ])

            l_n_abnormal_ref += n_abnormal_ref
            l_n_abnormal_pred += n_abnormal_pred
            l_n_abnormal_match += n_abnormal_match
        # ----- end   anomaly detection
        t_end = time.time()
        g_time += t_end - t_start

        # h.dump_stat('../untracked/ref_stat-{:02d}.json'.format(frame_idx))

        print("Frame %s" % frame_idx)
        print("# abnormal ref: %s" % l_n_abnormal_ref)
        print("# abnormal pred: %s" % l_n_abnormal_pred)
        print("# abnormal match: %s" % l_n_abnormal_match)

        l_accuracy = 0.
        if l_n_abnormal_pred > 0:
            l_accuracy = l_n_abnormal_match / min(l_n_abnormal_pred, l_n_abnormal_ref)
        print("accuracy: {:.3f}".format(l_accuracy))

        g_n_abnormal_ref += l_n_abnormal_ref
        g_n_abnormal_pred += l_n_abnormal_pred
        g_n_abnormal_match += l_n_abnormal_match
        g_n_funcalls += l_n_funcalls

        frame_idx += 1

    print("\nFinal:")
    print("# abnormal ref: %s" % g_n_abnormal_ref)
    print("# abnormal pred: %s" % g_n_abnormal_pred)
    print("# abnormal match: %s" % g_n_abnormal_match)
    g_accuracy = 1.
    if g_n_abnormal_pred > 0:
        g_accuracy = g_n_abnormal_match / min(g_n_abnormal_pred, g_n_abnormal_ref)
    print("accuracy: %s" % g_accuracy)
    print("Avg. Time (per frame): {:.3f} sec".format(g_time/n_max_frames))
    print("# function calls: %s" % g_n_funcalls)