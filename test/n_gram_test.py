import unittest
import os 
import json  
import time
import requests

from os import environ

import shutil
try:
    from ConfigParser import ConfigParser  # py2
except BaseException:
    from configparser import ConfigParser  # py3

from pprint import pprint  

from codar.chimbuku.perf_anom import n_gram
import pickle
import pandas as pd


class codarTest(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        config_file = environ.get('CODAR_DEPLOYMENT_CONFIG', None)
        cls.cfg = {}
        config = ConfigParser()
        config.read(config_file)
        for nameval in config.items('n_gram'):
            cls.cfg[nameval[0]] = nameval[1]

    @classmethod
    def tearDownClass(cls):
        if hasattr(cls, 'attr_to_delete'):
            print('Delete attr ')


    def test_extract_entry_exit(self):
        input_test_fn = "data/reduced-trace.json"
        result = n_gram.extract_entry_exit(input_test_fn)

        self.assertEqual(232164, len(result))

    def test_generate_n_grams_ct(self):
        input_test_fn = "data/ee_lst.json"
        with open(input_test_fn) as data_file:
            ee_lst = json.load(data_file)
        df = n_gram.generate_n_grams_ct(ee_lst, 1, k=3, file_name='test_out.df')

        self.assertEqual(696486, df.size)
        #TODO: add existance of test_out.df and the equivalance of it with data/n_gram.df

    def test_perform_localOutlierFactor(self):
        input_test_fn = "data/n_gram.df"
        with open(input_test_fn, 'rb') as handle:
            df = pd.read_pickle(handle)
            result = n_gram.perform_localOutlierFactor(df[df['kl'] == 'ANALYZ:ANA_TASK:UTIL_PRINT'], 10, (5/4400))

        self.assertEqual(4400, result.size)

    def test_detect_anomaly(self):
        trace_fn_lst = ["data/reduced-trace.json", "data/reduced-trace2.json"]
        jid_lst = [1, 2]
        df = n_gram.detect_anomaly(trace_fn_lst, jid_lst)

        self.assertEqual(313166, df.size)
