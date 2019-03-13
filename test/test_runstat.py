import numpy as np
import unittest
import requests
from algorithm.runstats import RunStats

class TestRunStat(unittest.TestCase):
    """Test runstat with parameter server"""

    def setUp(self):
        pass

    def tearDown(self):
        pass

    def test_runstat(self):
        n_groups = 10
        mean = np.random.randint(0, 20, n_groups)
        std = np.random.randint(5, 10, n_groups)
        n_data = np.random.randint(100, 100000, n_groups)

        data = []
        stat = []
        for gid, _mean, _std, _n in zip(range(n_groups), mean, std, n_data):
            _stat = RunStats()
            _data = []
            for i in range(_n):
                d = np.random.normal(_mean, _std)
                _data.append(d)
                _stat.add(d)

            data.append(np.array(_data))
            stat.append(_stat)

            self.assertAlmostEqual(np.mean(_data), _stat.mean(), 2,
                                   "{:d}-th group mean error".format(gid))
            self.assertAlmostEqual(np.std(_data), _stat.std(), 2,
                                   "{:d}-th group std error".format(gid))

        g_stat = RunStats()
        data = np.hstack(data)
        for _stat in stat:
            g_stat = g_stat + _stat

        self.assertAlmostEqual(np.mean(data), g_stat.mean(), 2, "global mean error")
        self.assertAlmostEqual(np.std(data), g_stat.std(), 2, "global std error")

        bPS = False
        url = 'http://127.0.0.1:5000'
        try:
            r = requests.get(url)
            r.raise_for_status()
            bPS = True
        except requests.exceptions.HTTPError as e:
            print("Http Error: ", e)
        except requests.exceptions.ConnectionError as e:
            print("Connection Error: ", e)
        except requests.exceptions.Timeout as e:
            print("Timeout Error: ", e)
        except requests.exceptions.RequestException as e:
            print("OOps: Something Else, ", e)

        if bPS:
            requests.post(url + '/clear')
            for _stat in stat:
                requests.post(url + '/update', json={'id': 0, 'stat': _stat.stat()})
            resp = requests.post(url + '/stat/0').json()
            s0, s1, s2 = resp['stat']
            self.assertAlmostEqual(s0, g_stat.s0, 3, "PS, s0 error")
            self.assertAlmostEqual(s1, g_stat.s1, 3, "PS, s1 error")
            self.assertAlmostEqual(s2, g_stat.s2, 3, "PS, s2 error")



if __name__ == '__main__':
    unittest.main()