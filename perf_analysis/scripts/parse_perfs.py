import json
import glob

perf_jsons = "./ad_perf.0.*.json"

files = glob.glob(perf_jsons)

perfs={}

for f in files:
	with open(f, 'rb') as fin:
		rank = int(f.split('.')[-2])
		j = json.load(fin)
		
		if rank not in perfs:
			perfs[rank] = {}
			perfs[rank]["ad_parse_input_begin_step_ms"] = j["ad_parse_input_begin_step_ms"]
			perfs[rank]["ad_parse_input_total_ms"] = j["ad_parse_input_total_ms"]
			perfs[rank]["ad_run_total_step_time_excl_parse_ms"] = j["ad_run_total_step_time_excl_parse_ms"]
			perfs[rank]["param_update_us"] = j["param_update_us"]
			perfs[rank]["ad_run_anom_detection_time_ms"] = j["ad_run_anom_detection_time_ms"]

''' Begin procesing perf metrics across ranks '''
#analysis = {'ad_parse_input_total_ms':{}, 
#		'ad_run_total_step_time_excl_parse_ms': {},
#		'param_update_us':{},
#		'ad_run_anom_detection_time_ms':{} }

ad_parse_input_begin_step_ms_mean = []
ad_parse_input_begin_step_ms_stddev = []
ad_parse_input_total_ms_mean = []
ad_parse_input_total_ms_stddev = []
ad_run_total_step_time_excl_parse_ms_mean = []
ad_run_total_step_time_excl_parse_ms_stddev = []
param_update_us_mean = []
param_update_us_stddev = []
ad_run_anom_detection_time_ms_mean = []
ad_run_anom_detection_time_ms_stddev = []

keys = [int(key) for key in perfs.keys()]
keys.sort()
for rank in keys:
	ad_parse_input_begin_step_ms_mean.append( perfs[rank]["ad_parse_input_begin_step_ms"]["mean"] )
	ad_parse_input_begin_step_ms_stddev.append( perfs[rank]["ad_parse_input_begin_step_ms"]["stddev"] )

	ad_parse_input_total_ms_mean.append( perfs[rank]["ad_parse_input_total_ms"]["mean"] )
	ad_parse_input_total_ms_stddev.append( perfs[rank]["ad_parse_input_total_ms"]["stddev"] )

	ad_run_total_step_time_excl_parse_ms_mean.append( perfs[rank]["ad_run_total_step_time_excl_parse_ms"]["mean"] )
	ad_run_total_step_time_excl_parse_ms_stddev.append( perfs[rank]["ad_run_total_step_time_excl_parse_ms"]["stddev"] )

	param_update_us_mean.append( perfs[rank]["param_update_us"]["mean"] )
	param_update_us_stddev.append( perfs[rank]["param_update_us"]["stddev"] )

	ad_run_anom_detection_time_ms_mean.append( perfs[rank]["ad_run_anom_detection_time_ms"]["mean"] )
	ad_run_anom_detection_time_ms_stddev.append( perfs[rank]["ad_run_anom_detection_time_ms"]["stddev"] )

'''Writing out lists to txt files '''
with open("ad_parse_input_begin_step_ms_mean.txt", "wb") as f:
	f.writelines("%s\n" % val for val in ad_parse_input_begin_step_ms_mean)

with open("ad_parse_input_begin_step_ms_stddev.txt", 'wb') as f:
	f.writelines("%s\n" % val for val in ad_parse_input_begin_step_ms_stddev)

with open("ad_parse_input_total_ms_mean.txt", 'wb') as f:
	f.writelines("%s\n" % val for val in ad_parse_input_total_ms_mean)

with open('ad_parse_input_total_ms_stddev.txt', 'w') as f:
	f.writelines("%s\n" % val for val in ad_parse_input_total_ms_stddev)

with open('ad_run_total_step_time_excl_parse_ms_mean.txt', 'w') as f:
	f.writelines("%s\n" % val for val in ad_run_total_step_time_excl_parse_ms_mean)

with open('ad_run_total_step_time_excl_parse_ms_stddev.txt', 'w') as f:
	f.writelines("%s\n" % val for val in ad_run_total_step_time_excl_parse_ms_stddev)

with open('param_update_us_mean.txt', 'w') as f:
	f.writelines("%s\n" % val for val in param_update_us_mean)

with open('param_update_us_stddev.txt', 'w') as f:
	f.writelines("%s\n" % val for val in param_update_us_stddev)

with open('ad_run_anom_detection_time_ms_mean.txt', 'w') as f:
	f.writelines("%s\n" % val for val in ad_run_anom_detection_time_ms_mean)

with open('ad_run_anom_detection_time_ms_stddev.txt', 'w') as f:
	f.writelines("%s\n" % val for val in ad_run_anom_detection_time_ms_stddev)


with open("filtered_perfs.json", 'wb') as fout:
	json.dump(perfs, fout, indent=2, sort_keys=True)



