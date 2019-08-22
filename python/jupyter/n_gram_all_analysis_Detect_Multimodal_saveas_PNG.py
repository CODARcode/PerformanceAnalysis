# The aim of this program is 
#
# 1 - To create historgram and detect multi-modal functions
# 2 - To save plots of such detected functions as PNG to disk
#
# This will enable testing of algorithms on multi-modal functions


# In[1]:

__date__   = 4/4/2018
__author__ = "Alok Singh"


# In[2]:

# Paper: https://arxiv.org/pdf/0710.3742v1.pdf


# In[3]:

import pickle
import matplotlib
matplotlib.use('agg')
import matplotlib.pyplot as plt
import seaborn as sns
import os
import glob
import graphlab as gl


# In[4]:

import numpy as np


# In[25]:

os.getcwd()
os.chdir("/datavol2/bnl_dataframes/csvs")
os.getcwd()

init  = True
sf    = None
itera = 0

for file in glob.glob("ee*.json.df.csv"):
    itera += 1
    #
    if init:
        #first call
        sf = gl.SFrame.read_csv(file, verbose=False)
        init = False
    else:
        sf = sf.append(gl.SFrame.read_csv(file, verbose=False))
    #
    print(itera, " File : ", file, "| Size > ", sf.num_cols(), sf.num_rows())


# In[26]:

import graphlab.aggregate as agg
ngram_count = sf.groupby(key_columns='kl', operations={'numberofcalls': agg.COUNT()})


# In[27]:

# ngram_count = ngram_count.sort('numberofcalls', ascending = False)


# In[28]:

def plotsf(sf, thetitle):
    import numpy as np
    import matplotlib.pyplot as plt
    
    fig = matplotlib.pyplot.gcf()
    fig.set_size_inches(15.5, 10.5)
    
    #Prune extreme outliers
    prune   = 100000
    howmany = 1
    
    while (len(sf[sf['time_diff']>prune]) > 10):
        prune   += 50000
        howmany += 1
        if(howmany > 100):
            print('Break')
            break
    
    sf = sf[sf['time_diff']<prune]
    
    plt.title("Plot of function: " + str(thetitle))
    
    #print('Checkpoint 4')
    df_normals   = sf[:]
    time_normals = sf[:]['time_by_lasttime']
    
    # Plot
    #print('Checkpoint 5')
    g = plt.scatter(time_normals ,  df_normals['time_diff'], c='green', s=65, edgecolor='k')
    
    # Adjust Plot
    minx=sf['time_by_lasttime'].min()
    maxx=sf['time_by_lasttime'].max()
    miny=sf['time_diff'].min()
    maxy=sf['time_diff'].max()
    
    plt.axis('tight')
    plt.xlim((minx, maxx))
    plt.ylim((miny, maxy))
    plt.legend([g],["Normal"],loc="upper right")
    
    #plt.show()
    
    filename = 'bimodal'+thetitle+'_plotted_'+str(len(sf))+'.png'
    print('Saving to disk: %s | #points: %d' % (filename, len(sf)))
    plt.savefig(filename)


# In[29]:

# Define max and min threshold
minn = 0
maxx = float("inf")
subset = ngram_count[(ngram_count['numberofcalls'] > minn) & (ngram_count['numberofcalls'] < maxx)]
freq   = subset
# In[32]:

def numberofpeaks(sf):
    x0  = sf['time_diff'].to_numpy()
    x, ed  = np.histogram(x0, bins=5)
    x = 1.0*x / sum(x)

    length = len(x)
    ret    = []
    thres  = 0.10
    
    while( len(ret) == 0):
        for i in range(length):
              ispeak = True
              ispeak &= (x[i] > thres)
              if ispeak:
                  ret.append(i)

        thres -= .01
          
    #print(len(ret))
    #print(ret)
    #print('*****')
    return ret


# In[35]:

sns.set_context("notebook", font_scale=1.2, rc={"lines.linewidth": 2.5})

print('Total functions: ', len(freq))

for i in range(len(freq)):
    pick_func = freq[i]['kl']
    count     = freq[i]['numberofcalls']
    lbl       = str(pick_func)[:] +'_count_'+ str(count)
    
    if count > 10000/2:
        n = numberofpeaks(sf[sf['kl'] == pick_func])
        
        if len(n) > 1:
            lbl = 'peaks_'+str(len(n))+'_'+lbl
            print(lbl)
            plotsf(sf[sf['kl'] == pick_func].sample(.5), thetitle=lbl)

print('End of program')
