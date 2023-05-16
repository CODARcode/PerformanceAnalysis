import provdb_python.provdb_interact as pi
import provdb_python.provdb_analyze as pa
import provdb_python.provdb_counter_analyze as pca
import provdb_python.provdb_between_run_analyze as pbr
import provdb_python.provdb_viz_validate as pvv

import sys

def cli():
    args = sys.argv
    if(len(args) < 2):
        print("A tool must be provided")
        print("Valid tools are:")
        print("filter")
        print("basic-analysis")
        print("counter-analysis")
        print("between-run-analysis")
        print("viz-output-validate")
        sys.exit(0)
    tool = args[1]
    tool_args = args[2:]
    if tool == 'filter':
        pi.provdb_interact_filter(tool_args)
    elif tool == 'basic-analysis':
        pa.provdb_basic_analysis(tool_args)
    elif tool == 'counter-analysis':
        pca.provdb_counter_analysis(tool_args)
    elif tool == 'between-run-analysis':
        pbr.provdb_between_run_analysis(tool_args)
    elif tool == 'viz-output-validate':
        pvv.provdb_viz_validate(tool_args)
    else:
        print("Invalid tool")

