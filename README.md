# Performance Data Analysis

This library provides a Python API to process [TAU](http://tau.uoregon.edu) performance profile and traces. At the moment it supports the following functionality:

  - Extract function call entry and exsit event filtering from TAU trace
  - Generate call stack with the depth `k', the duration of call, job_id, node_id, and thread_id
  - Detect anomalies 

## Installation
All you have to do is executing 'scripts/install-dependency.sh' and you need pip3 preinstalled:

    bash scripts/install-dependency.sh

## Test
To run tests:

    make
    make test

## Example
The following example code demonstrate the basic usage of performance anomaly detection steps:

    from codar.chimbuku.perf_anom import n_gram

    ee_lst = n_gram.extract_entry_exit('test/data/reduced-trace.json')

'ee\_lst' contains entry and exit event of TAU trace files. To generate n\_gram statistics (with call stack depth 'k'), the following will generate pandas.DataFrame:

    df = n_gram.generate_n_grams_ct(ee_lst, 1, k=3, file_name = 'n_gram.df')

where '1' is job\_id and 'k=3' is for how many call depth to maintain. Note that here call depth to maintain is from the leaf node, not from root node. 'file\_name' is for output pandas.DataFrame file, which is good for per trace processing.

Given df, the following detect  performance anomaly using LOF(Local Outlier Factor) algorithm where 'ANALYZ:ANA\_TASK:UTIL\_PRINT' is call stack and k=10 neighborhood node examples is used and find 5 anomaly out of 4400 calls:

    an_lst = n_gram.perform_localOutlierFactor(df[df['kl'] == 'ANALYZ:ANA_TASK:UTIL_PRINT'], 10, float(5/4400))

Anomaly instances can be selected by simply doing the following:

    df[an_lst]

We can merge multiple n\_gram statistics for multiple trace files as follow:

    import pandas as pd
    import glob

    df_lst = []
    for file in glob.glob("*.df"):
        print("Processing :" + file)
    
        with open(file, 'rb') as handle:
            ldf = pd.read_pickle(handle)
            df_lst.append(ldf)
    df = pd.concat(df_lst)

