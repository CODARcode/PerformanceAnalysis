"""
Outlier detection class
Authors: Shinjae Yoo (sjyoo@bnl.gov), Gyorgy Matyasfalvi (gmatyasfalvi@bnl.gov)
Create: August, 2018
"""

import event

class Outlier():
    def __init__(self, data):
      self.data = data # For now data will be an event object   


    def maxTimeDiff(): # determine which function has the biggest difference between min and max execution time
        maxDiffExecTime = 0
        maxFunId = None
        funtime = self.data.getFunExecTime()
        for ii in funtime:
            ll = funtime[ii]
            maxEvent = max(ll, key=lambda ll: ll[4]) 
            minEvent = min(ll, key=lambda ll: ll[4])
            diffExecTime = maxEvent[4] - minEvent[4]
            if(diffExecTime > maxDiffExecTime):
                maxDiffExecTime = diffExecTime
                maxFunId = ii
        return maxFunId
    
    