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

Given df, the following detect  performance anomaly using LOF(Local Outlier Factor) algorithm where 'ANALYZ:ANA\_TASK:UTIL\_PRINT' is call stack and k=10 neighborhood node examples is used and find 5 anomaly out of 4400 calls:

    an_lst = n_gram.perform_localOutlierFactor(df[df['kl'] == 'ANALYZ:ANA_TASK:UTIL_PRINT'], 10, float(5/4400))

Anomaly instances can be selected by simply doing the following:

    df[an_lst]

