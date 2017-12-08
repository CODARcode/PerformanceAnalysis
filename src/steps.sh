# 1. convert tau trace file to json
tau_trace2json tautrace.0.0.0.trc events.0.edf > reduced-trace.json

# 2. Tau trace json to entry_exit 
trace2ee.py reduced-trace

# 3. entry_exity json to call stack runtime dataframe  with three call stack depth
generate_n-grams.py ee-reduced-trace.json 3

# 4. visual analysis or anomaly detection
# Follow examples in Jupyter notebook
n_gram_all_analysis_(LocalOutlierFactor).ipynb
