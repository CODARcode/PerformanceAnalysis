# Performance Data Analysis
| Branch | Status |
| :--- | :--- |
| master | [![Build Status](https://travis-ci.org/CODARcode/PerformanceAnalysis.svg?branch=master)](https://travis-ci.org/CODARcode/PerformanceAnalysis)  |
| develop | [![Build Status](https://travis-ci.org/CODARcode/PerformanceAnalysis.svg?branch=release)](https://travis-ci.org/CODARcode/PerformanceAnalysis)  |


This library provides a Python API to process [TAU](http://tau.uoregon.edu) performance profile and traces. Its modules support the following functionalities:

  - Parser: processes a given TAU trace (both streaming and batch through Adios).
  - Event: keeps track of event information such as function call stack and function execution times.
  - Outlier: detects outliers in performance of functions.
  - Visualizer: provides an interface to Chimbuko's visualization [software](https://github.com/CODARcode/ChimbukoVisualization) (both online and offline through requests API).

# Requirement

Our codebase requires Python 3.5 or higher 
and `python` and `pip` to be linked to Python 3.5 or higher.
All additional requirements can be found in the requirements.txt file.

# Installation

Run the following script: 'scripts/install-dependency.sh'

    bash scripts/install-dependency.sh

# Test

To run tests:

    make
    make test

# Example

[[1]]

The following example code demonstrates the basic usage of performance anomaly detection steps:

    from codar.chimbuko.perf_anom import n_gram

    ee_lst = n_gram.extract_entry_exit('test/data/reduced-trace.json')

'ee\_lst' contains entry and exit events of TAU trace files. To generate n\_gram statistics (with call stack depth 'k'), the following command can be used. It returns a pandas.DataFrame:

    df = n_gram.generate_n_grams_ct(ee_lst, 1, k=3, file_name = 'n_gram.df')

where '1' is job\_id and 'k=3' is for how many call depth to maintain. Note that the call depth to calculated from the leaf node, not from root node. 'file\_name' is for output pandas.DataFrame file, which is good for per trace processing.

[[2]]

Given df, the following command can detect performance anomalies using LOF(Local Outlier Factor) algorithm where 'ANALYZ:ANA\_TASK:UTIL\_PRINT' is call stack and k=10 neighborhood node examples is used and find 5 anomaly out of 4400 calls:

    an_lst = n_gram.perform_localOutlierFactor(df[df['kl'] == 'ANALYZ:ANA_TASK:UTIL_PRINT'], 10, float(5/4400))

Anomaly instances can be selected by doing the following:

    df[an_lst]

[[3]]

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

[[4]]

For convenience, we provide another option to process all these steps using one function:

    from codar.chimbuko.perf_anom import n_gram
    trace_fn_lst = ["data/reduced-trace.json", "data/reduced-trace2.json"]
    jid_lst = [1, 2]
    df = n_gram.detect_anomaly(trace_fn_lst, jid_lst, out_fn_prefix="results", call_depth=3, n_neighbors=10, n_func_call=5, n_anomalies=5)

This will generate final aggregated anomalies in a DataFrame along with classified anomaly labels.  You have less freedom to test but it alternative is provided for the convenience. 

'n_neighbors' is the number of neighborhood to estimate density, 'n_func_call' is about how many function call n\_gram to process (top k frequent function call n\_gram will be processed), and n_anomalies is about how many anomalies want to see.
