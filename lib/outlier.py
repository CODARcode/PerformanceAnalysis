"""@package Chimbuko
This module detects outliers in trace data 
Author(s): 
    Gyorgy Matyasfalvi (gmatyasfalvi@bnl.gov)
    Sungsoo Ha (sungsooha@bnl.gov)

Created: 
    August, 2018

Modified:
    March 8, 2019

    pydoc -w outlier
    
"""
import numpy as np
import requests as req
from sklearn.neighbors import LocalOutlierFactor
from collections import defaultdict
from algorithm.runstats import RunStats

def check_ps(url, log=None):
    try:
        r = req.get(url)
        r.raise_for_status()
    except req.exceptions.HTTPError as e:
        if log is not None: log.debug("Http Error: ", e)
        return False
    except req.exceptions.ConnectionError as e:
        if log is not None: log.debug("Connection Error: ", e)
        return False
    except req.exceptions.Timeout as e:
        if log is not None: log.debug("Timeout Error: ", e)
        return False
    except req.exceptions.RequestException as e:
        if log is not None: log.debug("OOps: something else: ", e)
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
    
    def __init__(self, config, log=None):
        """
        This is the constructor for the Outlier class.
        Using the provided configuration file it determines which algorithm to use
        to detect outliers in the data.
        
        Args:
            configFile (string): The configuration files's name.
        """
        self.log = log

        # check parameter server
        self.ps_url = config['Outlier']['PSUrl']
        self.use_ps = check_ps(self.ps_url, self.log)
        if self.log is not None:
            self.log.info("Use parameter server: {:s} @ {:s}".format(
                'YES' if self.use_ps else 'NO', self.ps_url))

        # Outlier detection algorithm
        self.algorithm = config['Outlier']['Algorithm']
        
        # Local outlier factor
        if self.algorithm == 'Lof':
            self.numNeighbors = int(config['Lof']['n_neighbors'])
            self.leafSize = int(config['Lof']['leaf_size'])
            self.lofAlgorithm = config['Lof']['algorithm']
            self.metric = config['Lof']['metric']
            self.p = int(config['Lof']['p'])
            if config['Lof']['metric_params'] == "None":
                self.metricParams = None
            self.contamination = float(config['Lof']['contamination'])
            self.numJobs = int(config['Lof']['n_jobs'])
            
            # Lof objects
            self.clf = None
                  
        # Streaming standard deviation     
        if self.algorithm == 'Sstd':
            #self.stats = dict()
            self.stats = defaultdict(lambda: RunStats())
            self.sigma = int(config['Sstd']['Sigma'])
            self.numPoints = 0
        
        # Outliers and scores
        self.outl = []
        self.score = []

    def maxTimeDiff(self, data): 
        """It computes min and max execution times for each function and determines
        which function has the biggest difference between min and max execution time. 
        
        Args:
            data (numpy array): Holding function execution time table as received from Event class.
          
        Returns:
            Function id (int) of the function that had the biggest difference between min and max execution times.
        """
        maxDiffExecTime = 0
        maxFunId = None
        for ii in data:
            ll = data[ii]
            maxEvent = max(ll, key=lambda ll: ll[4]) 
            minEvent = min(ll, key=lambda ll: ll[4])
            diffExecTime = maxEvent[4] - minEvent[4]
            if diffExecTime > maxDiffExecTime:
                maxDiffExecTime = diffExecTime
                maxFunId = ii
        return maxFunId

    def lofComp(self, data):
        """ Run Local Outlier Factor algorithm on the data. 
        
        Args:
            data (numpy array): Holding function execution time table as received from Event class.
          
        Returns:
            No return value.
        """
        self.clf = LocalOutlierFactor(self.numNeighbors, self.lofAlgorithm, self.leafSize, self.metric, self.p, self.metricParams, self.contamination, self.numJobs)
        self.outl = self.clf.fit_predict(data)
        self.score = -1.0 * self.clf.negative_outlier_factor_

    def sstdComp(self, data, funid):
        """ Run Streaming Standard Deviation algorithm on the data. 
        
        Args:
            data (numpy array): Holding function execution time table as received from Event class.
            id (int): Specifying the function id which indicates which function the data belongs to.
        """
        funid = int(funid)
        self.outl, self.score = [], []

        for d in data:
            self.stats[funid].add(d)

        if self.use_ps:
            resp = req.post(self.ps_url + '/update',
                            json={'id': funid, 'stat': self.stats[funid].stat()})
            resp = resp.json()
            self.stats[funid].reset(*resp['stat'])

        if self.stats[funid].stat()[0] > 1.0:
            mean, std = self.stats[funid].mean(), self.stats[funid].std()
            threshold = mean + self.sigma*std
            for d in data:
                self.outl.append(-1 if d > threshold else 1)
                self.score.append(abs(d - mean))

    def compOutlier_v1(self, data, funid):
        """
        Generic function call to compute outliers in function execution time data
        for a specific function (specified by id). Depending on the configuration file
        it will either run LOF or Stream Standard Deviation  algorithm.
        
        Args:
            data (numpy array): Holding function execution time table as received from Event class.
                - n_function x 7
                - [PID, RID, TID, FID, start time, execution time, event id]

            id (int): Specifying the function id which indicates which function the data belongs to.
        """
        if self.algorithm == 'Sstd':
            self.sstdComp(data[:,5], funid)

        elif self.algorithm == 'Lof':
            self.lofComp(data[:,4:6])
        else:
            raise ValueError("Unsupported outlier detection algorithm: %s" % self.algorithm)

    def compOutlier_v2(self, data, funid):
        """
        Generic function call to compute outliers in function execution time data
        for a specific function (specified by id). Depending on the configuration file
        it will either run LOF or Stream Standard Deviation  algorithm.

        Args:
            data (numpy array): Holding function execution time table as received from Event class.
                - n_function x 7
                - [PID, RID, TID, FID, start time, execution time, event id]

            id (int): Specifying the function id which indicates which function the data belongs to.
        """
        if self.algorithm == 'Sstd':
            data = np.array([d.runtime for d in data])
            self.sstdComp(data, funid)

        elif self.algorithm == 'Lof':
            data = np.array([[d.entry, d.runtime] for d in data])
            self.lofComp(data)
        else:
            raise ValueError("Unsupported outlier detection algorithm: %s" % self.algorithm)

    def getClf(self):
        """Get method to access the object that implements Lof.
        
        Args:
            No arguments.
        
        Returns:
            self.clf, which is an object of the class LocalOutlierFactor().
        """
        return self.clf

    def getScore(self):
        """
        Get method to access the scores generated by the outlier detection algorithms.
        """
        return np.array(self.score)

    def checkNumPoints(self):
        """Get method to access how many data points we are dealing with.
        
        Args:
            No arguments.
        
        Returns:
            self.numPoints (int).
        """
        return self.numPoints

    def getOutlier(self):
        """
        Get method to access the outl list, which contains values of 1 or -1 indicating
        inlier or outlier for each data point.
        """
        return np.array(self.outl, dtype=np.int32)

    def getContamination(self):
        """Get method to access contamination parameter.
        
        Args:
            No arguments.
        
        Returns:
            self.contamination (float).
        """
        return self.contamination
    
    def getLofNumNeighbors(self):
        """Get method to access number of neighbors parameter.
        
        Args:
            No arguments.
        
        Returns:
            self.numNeighbors (int).
        """
        return self.numNeighbors
    
    def getAlgorithm(self):
        """Get method to access algorithm parameter.
        
        Args:
            No arguments.
        
        Returns:
            self.algorithm (string).
        """
        return self.algorithm
    
    def getSigma(self):
        """Get method to access sigma parameter.
        
        Args:
            No arguments.
        
        Returns:
            self.sigma (float).
        """
        return self.sigma
    
    def getMean(self, id):
        """Get method to access rolling mean of function execution time associated with
        function: id.
        
        Args:
            id (int): function id
        
        Returns:
            self.stats[id].mean() (float).
        """
        return self.stats[id].mean()

    def getStat(self, id):
        return self.stats[id].stat()
   
    