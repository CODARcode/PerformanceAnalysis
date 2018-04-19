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
		self.lrd = {}

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
	Points.lrd = clf._lrd.tolist()
	dist, ind = clf.kneighbors()
	Points.kdist = dist.tolist()
	Points.knn = ind.tolist()
	return Points

def ComputeDist(A, B):
	dist = np.linalg.norm(A-B)
	return dist

def union(list1, list2):
	set1 = set(list1)
	set2 = set(list2)
	list3 = list1 + list(set2 - set1)
	return list3

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
			if minval < Points.kdist[i-1][j]:
				Points.kdist[i-1][j] = minval
				Points.knn[j] = buck+ind
				if j < len(Points.kdist[i-1])-1:
					Points.kdist[i-1][j+1:] = [minval] * (len(Points.kdist[i-1]) - j - 1)
					Points.knn[i-1][j+1:] = [buck+ind] * (len(Points.kdist[i-1]) - j - 1)
				break

	rNN = []
	for k in range(0, i-1):
		distance = ComputeDist(datastream[k, :], datastream[i-1, :])
		if (Points.kdist[k][-1] >= distance):
			for kk in range(0, len(Points.knn[k])):
				if distance <= Points.kdist[k][kk]:
					if kk == 0:
						Points.knn[k]   = [i-1] + Points.knn[k][kk:]
						Points.kdist[k] = [distance] + Points.kdist[k][kk:]
					else:
						Points.knn[k]   = Points.knn[k][0:kk-1] + [i-1] + Points.knn[k][kk:]
						Points.kdist[k] = Points.kdist[k][0:k-1]+ [distance] + Points.kdist[k][kk:]
					break
			for kk in range(kpar, len(Points.knn[k])):
				if Points.kdist[k][kk] != Points.kdist[k][kpar-1]:
					del Points.kdist[k][kk:]
					del Points.knn[k][kk:] 
					break
			rNN = rNN + [k]

	# update the updatelrd set
	updatedlrd = rNN
	if len(rNN) > 0:
		for j in rNN:
			for ii in Points.knn[j]:
				if ii < len(Points.knn) and ii != i-1 and j in Points.knn[ii]:
					updatelrd = union(updatelrd, [ii])


	# lrd-i
	rdist = 0
	for p in Points.knn[i-1]:
		if p > buck-1:
			rdist = rdist + max(ComputeDist(datastream[i-1:], Clusters.center[p-buck])-width, PointsC.kdist[p-buck][-1])
		else:
			rdist = rdist + max(ComputeDist(datastream[i-1:], datastream[p,:]), Poists.kdist[p][-1])
	Points.lrd[i-1] = 1/(rdist/len(Points.knn[i-1]))

	# lrd neighbours
	updatedLOF = updatedlrd
	if len(updatedlrd) > 0:
		for m in updatedlrd:
			rdist = 0
			for p in Points.knn[m]:
				if p > buck-1:
					rdist = rdist + max(ComputeDist(datastream[m, :], Clusters.center[p-buck])-width, PointsC.kdist[p-buck][-1])
				else:
					rdist = rdist + max(ComputeDist(datastream[m, :], datastream[p, :]), Points.kdist[p][-1])
			Points.lrd[m] = 1/(rdist/len(Points.knn[m]))
			for k in range(0, len(Points)):
				if k != m and Points.knn[k].count(m) > 0
				updateLOF = unior(updateLOF, [k])

	# LOF neighbours
	if len(updateLOF) > 0:
		for l in updateLOF:
			lof = 0
			for ll in Points.knn[l]:
				if ll > buck-1:
					lof = lof + PointsC.lrd[ll-buck]/Points.lrd[l]
				else:
					lof = lof + Points.lrd[ll]/Points.lrd[l]
			Points.LOF[l] = lof/len(Points.knn[l])

	# LOF-i
	for ll in Points.knn[i-1]:
		if ll > buck-1:
			lof = lof + PointsC.lrd[ll-buck]/Points.lrd[i-1]
		else:
			lof = lof + Points.lrd[ll]/Points.lrd[i-1]
	Points.LOF[i-1] = lof/len(Points.knn[i])
	
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
	print("Points.lrd = ", Points.lrd)

	for mm in range(0, kpar+1):
		kdist.append(Points.kdist[mm][-1])
	print("kdist = ", kdist)

	# using direct Incremental LOF for the first bucket/2
#	for i in range(kpar+2, int(buck/2)):
	for i in range(kpar+2, kpar+3):
		Points = IncrementalLOF_Fixed(Points, datastream[0:i, :], PointsC, Clusters, kpar, buck, width)
		Scores.extend(Points.LOF)
#		kdist.append(Points.kdist[i][-1])
