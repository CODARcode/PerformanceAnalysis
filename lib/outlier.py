"""@package Chimbuko
This module detects outliers in trace data 
Author(s): 
    Gyorgy Matyasfalvi (gmatyasfalvi@bnl.gov)
    Sungsoo Ha (sungsooha@bnl.gov)

Created: 
    August, 2018

Modified:
    April 11, 2019

    pydoc -w outlier
    
"""
import numpy as np
import requests as req
import json
from sklearn.neighbors import LocalOutlierFactor
from collections import defaultdict
from algorithm.runstats import RunStats

def check_ps(url, log=None):
    """Check parameter server connection"""
    try:
        r = req.get(url)
        r.raise_for_status()
    except req.exceptions.HTTPError as e:
        if log is not None: log.debug("Http Error: {}".format(e))
        return False
    except req.exceptions.ConnectionError as e:
        if log is not None: log.debug("Connection Error: {}".format(e))
        return False
    except req.exceptions.Timeout as e:
        if log is not None: log.debug("Timeout Error: {}".format(e))
        return False
    except req.exceptions.RequestException as e:
        if log is not None: log.debug("OOps: something else: {}".format(e))
        return False
    except Exception as e:
        if log is not None: log.debug("OOps: really unknwon: {}".format(e))
        return False
    return True

class Outlier(object):
    """
    : Outlier class computes outliers by assigning scores to each data point.

    : The higher the score the more likely the data point is an outlier.

    : Depending on the configuration settings it will then create a vector of 1(s) and -1(s),
      where each entry of the vector corresponds to a data point and -1 indicates that
      the associated data point is an outlier, whereas 1 indicates it is an inlier.

    : At present the only algorithm that works for both streaming and batch is
      Streaming Standard Deviation (Sstd).
    """
    scale_factor = 1000000
    
    def __init__(self, config, log=None, force_no_ps=False):
        """
        This is the constructor for the Outlier class.
        Using the provided configuration file it determines which algorithm to use
        to detect outliers in the data.
        
        Args:
            config: config object
            log: logging object
            force_no_ps: force not to use parameter server if True regardless of its existance
        """
        self.log = log

        # check parameter server
        self.ps_url = config['Outlier']['PSUrl']
        self.use_ps = check_ps(self.ps_url, self.log) if not force_no_ps else False
        if self.log is not None:
            self.log.info("Use parameter server: {:s} @ {:s}".format(
                'YES' if self.use_ps else 'NO', self.ps_url))

        # Outlier detection algorithm
        self.algorithm = config['Outlier']['Algorithm']
        
        # Local outlier factor
        # if self.algorithm == 'Lof':
        #     self.numNeighbors = int(config['Lof']['n_neighbors'])
        #     self.leafSize = int(config['Lof']['leaf_size'])
        #     self.lofAlgorithm = config['Lof']['algorithm']
        #     self.metric = config['Lof']['metric']
        #     self.p = int(config['Lof']['p'])
        #     if config['Lof']['metric_params'] == "None":
        #         self.metricParams = None
        #     self.contamination = float(config['Lof']['contamination'])
        #     self.numJobs = int(config['Lof']['n_jobs'])
        #
        #     # Lof objects
        #     self.clf = None
                  
        # Streaming standard deviation     
        if self.algorithm == 'Sstd':
            self.stats = defaultdict(lambda: RunStats())
            self.sigma = float(config['Sstd']['Sigma'])
            self.local_stats = dict()

        else:
            raise ValueError("Unknown outlier detection algorithm: %s" % self.algorithm)

        # Outliers and scores
        self.outl = []
        self.score = []

    def sstdComp(self, data, funid):
        """ Run Streaming Standard Deviation algorithm on the data. 
        
        Args:
            data (numpy array): Holding function execution time table as received from Event class.
            funid (int): Specifying the function id which indicates which function the data belongs to.
        """
        funid = int(funid)
        self.outl, self.score = [], []

        data = data / self.scale_factor
        if self.stats[funid].stat()[0] > 1./RunStats.factor:
            mean, std = self.stats[funid].mean(), self.stats[funid].std()
            thr_hi = mean + self.sigma*std
            thr_lo = mean - self.sigma*std
            for d in data:
                self.outl.append(-1 if d > thr_hi or d < thr_lo else 1)
                self.score.append(abs(d - mean))

    def initLocalStat(self):
        """clear local statistics"""
        self.local_stats.clear()

    def addLocalStat(self, data, funid):
        """compute local statistics"""
        data = np.array([d.runtime for d in data])
        data = data / self.scale_factor
        s0, s1, s2 = len(data) / RunStats.factor, 0, 0
        for d in data:
            s1 += d
            s2 += d * d
        self.local_stats[int(funid)] = [s0, s1, s2]

    def pushLocalStat(self):
        """push the local statistics to either parameter server or local accumulated one"""
        if self.use_ps:
            try:
                resp = req.post(self.ps_url + '/update_all', json={'stats': self.local_stats})
                resp = resp.json()
                for funid, (s0, s1, s2) in resp['stats'].items():
                    self.stats[int(funid)].reset(s0, s1, s2)
            except Exception as e:
                self.log.info("Error: {}".format(e))
                exit(1)
        else:
            for funid, (s0, s1, s2) in self.local_stats.items():
                self.stats[funid].update(s0, s1, s2)

    def compOutlier(self, data, funid):
        """
        Generic function call to compute outliers in function execution time data
        for a specific function (specified by id). Depending on the configuration file
        it will either run LOF or Stream Standard Deviation  algorithm.

        Args:
            data (list): list of ExecData (see definition.py for details)
            funid (int): Specifying the function id which indicates which function the data belongs to.
        """
        if self.algorithm == 'Sstd':
            data = np.array([d.runtime for d in data])
            self.sstdComp(data, funid)

        # elif self.algorithm == 'Lof':
        #     data = np.array([[d.entry, d.runtime] for d in data])
        #     self.lofComp(data)

        else:
            raise ValueError("Unsupported outlier detection algorithm: %s" % self.algorithm)

    def getScore(self):
        """Get method to access the scores generated by the outlier detection algorithms."""
        return np.array(self.score)

    def getOutlier(self):
        """
        Get method to access the outl list, which contains values of 1 or -1 indicating
        inlier or outlier for each data point.
        """
        return np.array(self.outl, dtype=np.int32)

    def getAlgorithm(self):
        """Get method to access algorithm parameter."""
        return self.algorithm
    
    def getSigma(self):
        """Get method to access sigma parameter."""
        return self.sigma
    
    def getMean(self, funid):
        """Get method to access rolling mean of function execution time associated with
        function: id"""
        return self.stats[funid].mean()

    def getStat(self, funid):
        """Get running statistics"""
        return self.stats[funid].stat()

    def getStatViz(self, funid):
        """Get running statistics for VIS module"""
        n_total, n_abnormal = self.stats[funid].count()
        mean = self.stats[funid].mean()
        std = self.stats[funid].std()
        return n_total, n_abnormal, mean, std, self.stats[funid].factor

    def addAbnormal(self, funid, n_abnormals):
        """Add the number of anbormal trace data to running statistics"""
        if self.use_ps:
            resp = req.post(self.ps_url + '/add_abnormal',
                            json={'id': int(funid), 'abnormal': int(n_abnormals)})
            resp = resp.json()
            self.stats[funid].reset_abnormal(resp['abnormal'])
        else:
            self.stats[funid].add_abnormal(n_abnormals)

    def getCount(self, funid):
        """Get trace data count (total and abnormal)"""
        return self.stats[funid].count()

    def dump_stat(self, path='stat.json'):
        """Dump running statistics on json file (only for debug)"""
        data = {}
        for funid, stat in self.stats.items():
            data[funid] = [stat.mean(), stat.std(), *stat.stat()]
        with open(path, 'w') as f:
            json.dump(data, f, indent=4, sort_keys=True)

    def shutdown_parameter_server(self):
        if self.use_ps:
            req.post(self.ps_url + '/shutdown')


    # @staticmethod
    # def maxTimeDiff(data):
    #     """ (not fully tested on Summit)
    #     It computes min and max execution times for each function and determines
    #     which function has the biggest difference between min and max execution time.
    #
    #     Args:
    #         data (numpy array): Holding function execution time table as received from Event class.
    #
    #     Returns:
    #         Function id (int) of the function that had the biggest difference between min and max execution times.
    #     """
    #     maxDiffExecTime = 0
    #     maxFunId = None
    #     for ii in data:
    #         ll = data[ii]
    #         maxEvent = max(ll, key=lambda ll: ll[4])
    #         minEvent = min(ll, key=lambda ll: ll[4])
    #         diffExecTime = maxEvent[4] - minEvent[4]
    #         if diffExecTime > maxDiffExecTime:
    #             maxDiffExecTime = diffExecTime
    #             maxFunId = ii
    #     return maxFunId
    #
    # def lofComp(self, data):
    #     """ (not fully tested on Summit)
    #     Run Local Outlier Factor algorithm on the data.
    #
    #     Args:
    #         data (numpy array): Holding function execution time table as received from Event class.
    #
    #     Returns:
    #         No return value.
    #     """
    #     self.clf = LocalOutlierFactor(self.numNeighbors, self.lofAlgorithm, self.leafSize,
    #                                   self.metric, self.p, self.metricParams, self.contamination,
    #                                   self.numJobs)
    #     self.outl = self.clf.fit_predict(data)
    #     self.score = -1.0 * self.clf.negative_outlier_factor_
    #
    # def getClf(self):
    #     """Get method to access the object that implements Lof."""
    #     return self.clf
    #
    # def getContamination(self):
    #     """Get method to access contamination parameter"""
    #     return self.contamination
    #
    # def getLofNumNeighbors(self):
    #     """Get method to access number of neighbors parameter."""
    #     return self.numNeighbors

    