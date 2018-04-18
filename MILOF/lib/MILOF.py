import numpy as np
import scipy.io as sio
from sklearn.preprocessing import MinMaxScaler
from sklearn.neighbors import NearestNeighbors
from sklearn.neighbors import LocalOutlierFactor

class Point:
	def __init__(self):
		self.LOF = {}
		self.kdist = {}
		self.knn = {}

class Cluster:
	def __init__(self):
		self.center = {}
		self.LS = {}

def LOF(datastream, kpar):
	#not sure to use euclidean or minkowski
	clf = LocalOutlierFactor(n_neighbors=kpar, algorithm="kd_tree", leaf_size=30, metric='euclidean', n_jobs=-1)
	clf.fit(datastream)
	Points = Point()
	Points.LOF = clf.negative_outlier_factor_.tolist()
	dist, ind = clf.kneighbors()
	Points.kdist = dist.tolist()
	Points.knn = ind.tolist()
	return Points

def ComputeDist(A, B):
	dist = np.linalg.norm(A-B)
	return dist

def IncrementalLOF_Fixed(Points, datastream, PointsC, Clusters, kpar, buck, width):
	i = datastream.shape[0]
	nbrs = NearestNeighbors(n_neighbors=kpar, algorithm='kd_tree', leaf_size=30, metric='euclidean', n_jobs=-1)
	nbrs.fit(datastream[0:i-1, :])
	dist, ind = nbrs.kneighbors(datastream[i-1, :].reshape(1, -1))
	Points.kdist.extend(dist.tolist())
	Points.knn.extend(ind.tolist())
	print("Points.kdist = ", Points.kdist)
	print("Points.knn = ", Points.knn)

	#check the distances of the point i with the cluster centers and pick the nearest cluster
	dist = []
	for j in range(0, len(Clusters.center)):
		distCI = ComputeDist(Clusters.center[j], datastream[i-1, :]) - width;
		if distCI <=0:
			distCI=PointsC.kdist[j]
		dist.append(distCI)

	if (len(dist)):
		minval, ind =  min((dist[j], j) for j in xrange(len(dist)))
		for j in range(0, len(Points.kdist[i-1])):
			if (minval < Points.kdist[i-1][j]):
				Points.kdist[i-1][j] = minval
				Points.knn[j] = buck+ind
				if (j < len(Points.kdist[i-1])-1):
					Points.kdist[i-1][j+1:] = [minval] * (len(Points.kdist[i-1]) - j - 1)
					Points.knn[i-1][j+1:] = [buck+ind] * (len(Points.kdist[i-1]) - j - 1)
				break
	return Points

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
	print ("normalized data = ", datastream)

	PointsC=[]
	Clusters = Cluster()
	Scores = []
	Points = LOF(datastream[0:kpar+1, :], kpar)
	Scores.extend(Points.LOF)
	print("Scores = ", Scores)

	for mm in range(0, kpar+1):
		kdist.append(Points.kdist[mm][-1])
	print("kdist = ", kdist)

	# using direct Incremental LOF for the first bucket/2
#	for i in range(kpar+2, int(buck/2)):
	for i in range(kpar+2, kpar+3):
		Points = IncrementalLOF_Fixed(Points, datastream[0:i, :], PointsC, Clusters, kpar, buck, width)
#		Scores.extend(Points.LOF)
#		kdist.append(Points.kdist[i][-1])
