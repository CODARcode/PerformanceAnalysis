"""@package Chimbuko
This module detects outliers in trace data 
Author(s): 
    Gyorgy Matyasfalvi (gmatyasfalvi@bnl.gov)
Created: 
    August, 2018
    
    pydoc -w outlier
    
"""

import configparser
import numpy as np
from runstats import Statistics
from sklearn.neighbors import LocalOutlierFactor

class Outlier():
    """Outlier class computes outliers by assigning scores to each data point. 
    The higher the score the more likely the data point is an outlier.
    Depending on the configuration settings it will then create a vector of 1(s) and -1(s),
    where each entry of the vector corresponds to a data point and -1 indicates that
    the associated data point is an outlier, whereas 1 indicates it is an inlier.
    At present the only algorithm that works for both streaming and batch is Streaming Standard Deviation (Sstd).   
    """
    
    def __init__(self, configFile):
        """This is the constructor for the Outlier class. 
        Using the provided configuration file it determines which algorithm to use to detect outliers in the data.
        
        Args:
            configFile (string): The configuration files's name.
          
        Returns:
            No return value.
        """
        
        self.config = configparser.ConfigParser()
        self.config.read(configFile)
        
        # Outlier detection algorithm
        self.algorithm = self.config['Outlier']['Algorithm']
        
        # Local outlier factor
        if self.algorithm == 'Lof':
            self.numNeighbors = int(self.config['Lof']['n_neighbors'])
            self.leafSize = int(self.config['Lof']['leaf_size'])
            self.lofAlgorithm = self.config['Lof']['algorithm']
            self.metric = self.config['Lof']['metric']
            self.p = int(self.config['Lof']['p'])
            if self.config['Lof']['metric_params'] == "None":
                self.metricParams = None
            self.contamination = float(self.config['Lof']['contamination'])
            self.numJobs = int(self.config['Lof']['n_jobs'])
            
            # Lof objects
            self.clf = None
                  
        # Streaming standard deviation     
        if self.algorithm == 'Sstd':
            self.stats = {}
            self.sigma = int(self.config['Sstd']['Sigma'])
            self.numPoints = 0
        
        # Outliers and scores
        self.outl = None
        self.score = None
        


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
            if(diffExecTime > maxDiffExecTime):
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
        
    
    def sstdComp(self, data, id):
        """ Run Streaming Standard Deviation algorithm on the data. 
        
        Args:
            data (numpy array): Holding function execution time table as received from Event class.
            id (int): Specifying the function id which indicates which function the data belongs to.
          
        Returns:
            No return value.
        """
        if self.outl is None and self.score is None:
            self.outl = []
            self.score = []
        else:
            self.outl.clear()
            self.score.clear()
        if id not in self.stats:
            self.stats[id] = Statistics()
        for i in data[:]:
            self.stats[id].push(i)
        if self.stats[id].get_state()[0] > 1.0:
            sigma = self.stats[id].mean() + self.sigma*self.stats[id].stddev() 
            for i in range(0, len(data[:])):
                if data[i] > sigma:
                    self.outl.append(-1)
                    self.score.append(abs(data[i] - self.stats[id].mean()))
                else:
                    self.outl.append(1)
                    self.score.append(abs(data[i] - self.stats[id].mean()))
    
    
    def compOutlier(self, data, id):
        """Generic function call to compute outliers in function execution time data 
        for a specific function (specified by id). Depending on the configuration file
        it will either run LOF or Stream Standard Deviation  algorithm.
        
        Args:
            data (numpy array): Holding function execution time table as received from Event class.
            id (int): Specifying the function id which indicates which function the data belongs to.
        
        Returns:
            No return value.
        """
        if self.algorithm == 'Sstd':
            self.sstdComp(data[:,5], id)
            return
            
        if self.algorithm == 'Lof':
            self.lofComp(data[:,4:6])
            return
    
    
    def getClf(self):
        """Get method to access the object that implements Lof.
        
        Args:
            No arguments.
        
        Returns:
            self.clf, which is an object of the class LocalOutlierFactor().
        """
        return self.clf
    
        
    def getScore(self):
        """Get method to access the scores generated by the outlier detection algorithms.
        
        Args:
            No arguments.
        
        Returns:
            self.score (numpy array).
        """
        if self.score is None:
            raise Exception("No scores computed ...")
        elif type(self.score) is list:
            return np.array(self.score)
        else:
            return self.score
    
    def checkNumPoints(self):
        """Get method to access how many data points we are dealing with.
        
        Args:
            No arguments.
        
        Returns:
            self.numPoints (int).
        """
        return self.numPoints
        
        
    def getOutlier(self):
        """Get method to access the outl list, which contains values of 1 or -1 indicating 
        inlier or outlier for each data point.
        
        Args:
            No arguments.
        
        Returns:
            self.outl (list).
        """
        if self.outl is None:
            raise Exception("No outliers computed ...")
        else:
            return self.outl
        
        
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
   
    