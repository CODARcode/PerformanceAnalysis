import numpy as np
import scipy.io as sio
from sklearn.preprocessing import MinMaxScaler
from sklearn.neighbors import KDTree
from sklearn.neighbors import LocalOutlierFactor

class Point:
	def __init__(self):
		self.LOF = {}
		self.kdist = {}

def MILOF_Kmeans_Merge(kpar, dimension, buck, filepath, num_k, width):
	kdist = []
	clusterLog = []
	datastream = sio.loadmat(filepath)
	datastream = np.array(datastream['DataStream'])
	datastream = datastream[:, 0:dimension]

	scaler = MinMaxScaler()
	scaler.fit(datastream)
	datastream = scaler.transform(datastream)
#	datastream = (datastream - datastream.min(axis=0)) / (datastream.max(axis=0) - datastream.min(axis=0)
	print ("data = ", datastream)

	PointsC=[]
	Scores = []
	Points = LOF(datastream[0:kpar+1, :], kpar)
	Scores.extend(Points.LOF)
	print("Scores = ", Scores)

	for mm in range(0, kpar+1):
		kdist.append(Points.kdist[mm][-1])
	print("kdist = ", kdist)

def LOF(datastream, kpar):
	#not sure to use euclidean or minkowski
	clf = LocalOutlierFactor(n_neighbors=kpar, algorithm="kd_tree", leaf_size=30, metric="euclidean", p=2, metric_params=None, n_jobs=-1)
	clf.fit(datastream)
	Points = Point()
	Points.LOF = clf.negative_outlier_factor_.tolist()
	dist, ind = clf.kneighbors()
	Points.kdist = dist.tolist()
	return Points