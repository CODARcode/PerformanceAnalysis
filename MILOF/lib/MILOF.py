import numpy as np
import scipy.io as sio
from sklearn.preprocessing import MinMaxScaler
from sklearn.neighbors import KDTree
from sklearn.neighbors import LocalOutlierFactor

class Point:
	def __init__(self):
		self.LOF = {}

def MILOF_Kmeans_Merge(kpar, dimension, buck, filepath, num_k, width):
	kdist = []
	clusterLog = []
	datastream = sio.loadmat(filepath)
	datastream = np.array(datastream['DataStream'])
	datastream = datastream[:, 0:dimension]
	print (datastream)

	scaler = MinMaxScaler()
	scaler.fit(datastream)
	datastream = scaler.transform(datastream)
#	datastream = (datastream - datastream.min(axis=0)) / (datastream.max(axis=0) - datastream.min(axis=0)
	print (datastream)

	PointsC=[]
	Scores = []
	Points = LOF(datastream[0:kpar+1, :], kpar)
	Scores.extend(Points.LOF)
	print(Scores)


def LOF(datastream, kpar):
	#not sure to use euclidean or minkowski
	clf = LocalOutlierFactor(n_neighbors=kpar+1, algorithm="kd_tree", leaf_size=30, metric="euclidean", p=2, metric_params=None, n_jobs=-1)
	clf.fit(datastream)
	Points = Point()
	Points.LOF = clf.negative_outlier_factor_.tolist()
	return Points