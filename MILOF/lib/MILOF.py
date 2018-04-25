import numpy as np
import scipy.io as sio
from sklearn.preprocessing import MinMaxScaler
from sklearn.neighbors import NearestNeighbors
from sklearn.neighbors import LocalOutlierFactor
from sklearn.cluster import KMeans

class Point:
	def __init__(self):
		self.kdist = []
		self.knn = []
		self.lrd = []
		self.LOF = []
		# self.rdist = {}

class Cluster:
	def __init__(self):
		self.center = []
		self.LS = []

def LOF(datastream, kpar):
	#not sure to use euclidean or minkowski
	Points = Point()
	clf = LocalOutlierFactor(n_neighbors=kpar, algorithm="kd_tree", leaf_size=30, metric='euclidean', n_jobs=-1)
	clf.fit(datastream)
	Points.LOF = [-x for x in clf.negative_outlier_factor_.tolist()]
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

def setdiff(list1, list2):
	set1 = set(list1)
	set2 = set(list2)
	list3 = list(set1 - set2)
	return list3

def IncrementalLOF_Fixed(Points, datastream, PointsC, Clusters, kpar, buck, width):
	i = datastream.shape[0]
	# print("******************* Processing Data Point", i-1, "*******************")
	
	nbrs = NearestNeighbors(n_neighbors=kpar, algorithm='kd_tree', leaf_size=50, metric='euclidean', n_jobs=-1)
	nbrs.fit(datastream[0:i-1, :])
	dist, ind = nbrs.kneighbors(datastream[i-1, :].reshape(1, -1))
	Points.kdist = Points.kdist + dist.tolist()
	Points.knn = Points.knn + ind.tolist()
	# print("Points.kdist = ", Points.kdist)
	# print("Points.knn = ", Points.knn)

	#check the distances of the point i with the cluster centers and pick the nearest cluster
	dist = []
	for j in range(0, len(Clusters.center)):
		distCI = ComputeDist(Clusters.center[j], datastream[i-1, :]) - width;
		if distCI <=0:
			distCI=PointsC.kdist[j]
		dist = dist + [distCI]
	# print("dist = ", dist)

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
		# print ("distance between point", k, "and point", i-1, "=", distance)
		if (Points.kdist[k][-1] >= distance):
			for kk in range(0, len(Points.knn[k])):
				if distance <= Points.kdist[k][kk]:
					if kk == 0:
						Points.knn[k]   = [i-1] + Points.knn[k][kk:]
						Points.kdist[k] = [distance] + Points.kdist[k][kk:]
					else:
						Points.knn[k]   = Points.knn[k][0:kk] + [i-1] + Points.knn[k][kk:]
						Points.kdist[k] = Points.kdist[k][0:kk]+ [distance] + Points.kdist[k][kk:]
					break
			for kk in range(kpar, len(Points.knn[k])):
				# print(Points.kdist[k][kk], Points.kdist[k][kpar-1])
				if Points.kdist[k][kk] != Points.kdist[k][kpar-1]:
					del Points.kdist[k][kk:]
					del Points.knn[k][kk:] 
					break
			rNN = rNN + [k]
	# print("rNN = ", rNN)
	# print("Points.kdist = ", Points.kdist)
	# print("Points.knn = ", Points.knn)

	# update the updatelrd set
	updatelrd = rNN
	if len(rNN) > 0:
		for j in rNN:
			for ii in Points.knn[j]:
				if ii < len(Points.knn) and ii != i-1 and j in Points.knn[ii]:
					updatelrd = union(updatelrd, [ii])
	# print("updatelrd = ", updatelrd)

	# lrd-i
	rdist = 0
	for p in Points.knn[i-1]:
		if p > buck-1:
			rdist = rdist + max(ComputeDist(datastream[i-1:], Clusters.center[p-buck])-width, PointsC.kdist[p-buck][-1])
		else:
			rdist = rdist + max(ComputeDist(datastream[i-1:], datastream[p,:]), Points.kdist[p][-1])
	Points.lrd = Points.lrd + [1/(rdist/len(Points.knn[i-1]))]
	# print("Points.lrd = ", Points.lrd)

	# lrd neighbours
	updateLOF = updatelrd
	if len(updatelrd) > 0:
		for m in updatelrd:
			rdist = 0
			for p in Points.knn[m]:
				if p > buck-1:
					rdist = rdist + max(ComputeDist(datastream[m, :], Clusters.center[p-buck])-width, PointsC.kdist[p-buck][-1])
				else:
					rdist = rdist + max(ComputeDist(datastream[m, :], datastream[p, :]), Points.kdist[p][-1])
			Points.lrd[m] = 1/(rdist/len(Points.knn[m]))
			for k in range(0, len(Points.knn)):
				if k != m and Points.knn[k].count(m) > 0:
					updateLOF = union(updateLOF, [k])
	# print("Points.lrd =", Points.lrd)
	# print("updateLOF =", updateLOF)

	# LOF neighbours
	if len(updateLOF) > 0:
		for l in updateLOF:
			lof = 0
			for ll in Points.knn[l]:
				if ll > buck-1:
					lof = lof + PointsC.lrd[ll-buck]/Points.lrd[l]
				else:
					lof = lof + Points.lrd[ll]/Points.lrd[l]
			if l == len(Points.LOF):
				Points.LOF = Points.LOF + [lof/len(Points.knn[l])]
			else:
				Points.LOF[l] = lof/len(Points.knn[l])
	# print("Points.LOF =", Points.LOF)

	# LOF-i
	lof = 0
	for ll in Points.knn[i-1]:
		if ll > buck-1:
			lof = lof + PointsC.lrd[ll-buck]/Points.lrd[i-1]
		else:
			lof = lof + Points.lrd[ll]/Points.lrd[i-1]
	if i-1 == len(Points.LOF):
		Points.LOF = Points.LOF + [lof/len(Points.knn[i-1])]
	else:
		Points.LOF[i-1] = lof/len(Points.knn[i-1])
	# print("Points.kdist =", Points.kdist)
	# print("Points.knn =", Points.knn)
	# print("Points.LOF =", Points.LOF)
	# print("Points.lrd =", Points.lrd)
	# print("rNN = ", rNN)
	# print("updatelrd = ", updatelrd)
	# print("updateLOF =", updateLOF)
	
	return Points

def MILOF_Kmeans_Merge(kpar, dimension, buck, filepath, num_k, width):
	datastream = sio.loadmat(filepath)
	datastream = np.array(datastream['DataStream'])
	datastream = datastream[:, 0:dimension]
	datastream = np.unique(datastream, axis=0)

	scaler = MinMaxScaler()
	scaler.fit(datastream)
	datastream = scaler.transform(datastream)
	PointNum = datastream.shape[0]
	print ("number of data points =", PointNum)
	print ("normalized data = ", datastream)

	hbuck = int(buck/2) # half of buck
	kdist = []
	Scores = []
	clusterLog = []
	Clusters = Cluster()
	PointsC= Point()
	Points = LOF(datastream[0:kpar+1, :], kpar)
	Scores = Scores + Points.LOF
	
	for mm in range(0, kpar+1):
		kdist = kdist + [Points.kdist[mm][-1]]

	print("Scores =", Scores)	
	print("kdist =", kdist)

	# using direct Incremental LOF for the first bucket/2
	for i in range(kpar+2, hbuck+1):
		Points = IncrementalLOF_Fixed(Points, datastream[0:i, :], PointsC, Clusters, kpar, buck, width)
		Scores = Scores + [Points.LOF[i-1]]
		kdist  = kdist + [Points.kdist[i-1][-1]]
	# print("Scores =", Scores)	
	# print("kdist =", kdist)

	exit = False
	step = 0
	while not exit:
		for i in range(hbuck+1, buck+1):
			if (i > PointNum):
				exit = True
				break
			Points = IncrementalLOF_Fixed(Points, datastream[0:i, :], PointsC, Clusters, kpar, buck, width)
			Scores = Scores + [Points.LOF[i-1]]
			kdist  = kdist + [Points.kdist[i-1][-1]]
		# print("Scores =", Scores)
		# print("kdist =", kdist)

		if not exit:
			step = step + 1
			indexNormal = list(range(0, hbuck))
			kmeans = KMeans(n_clusters=num_k, n_jobs=-1)  # Considering precompute_distances for faster but more memory
			kmeans.fit(datastream[indexNormal, :]) # need to check how to configure to match result of matlab code 
			center = kmeans.cluster_centers_
			clusterindex = kmeans.labels_
			# print("label =", clusterindex)
			# print("center =", center)
			remClustLbl = list(range(0, num_k))
			lof_scores = []
			for itr in range(0, hbuck):
				lof_scores = lof_scores + [Points.kdist[itr][-1]] # need to optimize later, same as line 201
			lof_scores = np.array(lof_scores)
			lof_threshold = np.mean(lof_scores) + 3 * np.std(lof_scores) # Not sure if calcuating for each i is necessary
			# print("lof_scores=", lof_scores)
			# print("lof_threshold=", lof_threshold)
			for kk in range(0, num_k):
				clusterMembers = np.where(clusterindex==kk)
				clusterMembersList = np.asarray(clusterMembers).flatten().tolist()
				if np.sum(lof_scores[clusterMembers]>lof_threshold) > 0.5*len(clusterMembersList):
					indexNormal = setdiff(indexNormal, clusterMembersList)
					remClustLbl = setdiff(remClustLbl, [kk])
			clusterindex = clusterindex[indexNormal]
			center = center[remClustLbl,:]
		
			# make summerization of clusters
			for j in range(0, len(remClustLbl)):
				# PointsC.rdist = PointsC.rdist + []
				num = np.sum(clusterindex == remClustLbl[j])
				PointsC.knn = PointsC.knn + [num]
				Ckdist = Clrd = CLOF = 0
				for k in np.array(indexNormal)[np.where(clusterindex==remClustLbl[j])]:
					Ckdist = Ckdist + Points.kdist[k][-1]
					Clrd   = Clrd   + Points.lrd[k]
					CLOF   = CLOF   + Points.LOF[k]
				PointsC.kdist = PointsC.kdist + [Ckdist/num]
				PointsC.lrd   = PointsC.lrd   + [Clrd/num]
				PointsC.LOF   = PointsC.LOF   + [CLOF/num]

				print(PointsC.kdist)
				print(PointsC.lrd)
				print(PointsC.LOF)

			np.delete(datastream, range(0,hbuck), axis=0)
			del Points.kdist[0:hbuck]
			del Points.knn[0:hbuck]
			del Points.lrd[0:hbuck]
			del Points.LOF[0:hbuck]

		# exit = True