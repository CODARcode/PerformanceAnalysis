"""
Outlier detection class
Authors: Shinjae Yoo (sjyoo@bnl.gov), Gyorgy Matyasfalvi (gmatyasfalvi@bnl.gov)
Create: August, 2018
"""

import configparser
import numpy as np
from runstats import Statistics
from sklearn.neighbors import LocalOutlierFactor

class Outlier():
    def __init__(self, configFile):
        self.config = configparser.ConfigParser()
        self.config.read(configFile)
        
        # Outlier detection algorithm
        self.algorithm = self.config['Outlier']['Algorithm']
        
        # Local outlier factor
        if self.algorithm == 'Lof':
            self.runLof = self.config['Lof']['RunLof']
            self.numNeighbors = int(self.config['Lof']['n_neighbors'])
            self.leafSize = int(self.config['Lof']['leaf_size'])
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
            self.stats = Statistics()
            self.sigma = int(self.config['Sstd']['Sigma'])
            self.numPoints = 0
        
        
        # Outliers and scores
        self.outl = None
        self.score = None
        


    def maxTimeDiff(self, data): # determine which function has the biggest difference between min and max execution time
        # TODO rewrite to consitently expect numpy array
        maxDiffExecTime = 0
        maxFunId = None
        funtime = data.getFunExecTime()
        for ii in funtime:
            ll = funtime[ii]
            maxEvent = max(ll, key=lambda ll: ll[4]) 
            minEvent = min(ll, key=lambda ll: ll[4])
            diffExecTime = maxEvent[4] - minEvent[4]
            if(diffExecTime > maxDiffExecTime):
                maxDiffExecTime = diffExecTime
                maxFunId = ii
        return maxFunId
    
    
    
    def lofComp(self, data):
        self.clf = LocalOutlierFactor(self.numNeighbors, self.algorithm, self.leafSize, self.metric, self.p, self.metricParams, self.contamination, self.numJobs)
        self.outl = self.clf.fit_predict(data)
        self.score = -1.0 * self.clf.negative_outlier_factor_
        
    
    def sstdComp(self, data):
        if self.outl is None and self.score is None:
            self.outl = []
            self.score = []
        else:
            self.outl.clear()
            self.score.clear()
        for i in data[:]:
            self.stats.push(i)
        sigma = self.stats.mean() + self.sigma*self.stats.stddev() 
        for i in range(0, len(data[:])):
            if data[i] >= sigma:
                self.outl.append(-1)
                self.score.append(abs(data[i] - self.stats.stddev()))
            else:
                 self.outl.append(1)
                 self.score.append(abs(data[i] - self.stats.stddev()))
    
    
    def compOutlier(self, data):
        if self.algorithm == 'Sstd':
            self.sstdComp(data[:,5])
            
        if self.algorithm == 'Lof':
            self.lofComp(data[:,4:6])
    
    
    def getClf(self):
        return self.clf
    
        
    def getScore(self):
        if self.score is None:
            raise Exception("No scores computed ...")
        else:
            return self.score
        
        
    def getOutlier(self):
        if self.outl is None:
            raise Exception("No outliers computed ...")
        else:
            return self.outl
        
        
    def getContamination(self):
        return self.contamination
   
    