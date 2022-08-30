import math
import copy

verbose = False
def verbosePrint(*args):
    if(verbose == True):
        print(*args)

class HistogramVBW:
    class Bin:
        def __init__(self,l,u,c):
            self.l = l #lower edge, float
            self.u = u #upper edge, float
            self.c = c #count, int
            self.left = None
            self.right = None
            self.is_end = False

        def __str__(self):
            if(self.is_end == True):
                return "(END)"
            else:
                return "(%f:%f;%d)" % (self.l,self.u,self.c)

        __repr__ = __str__

        @staticmethod
        def create_end():
            end = HistogramVBW.Bin(0,0,0)  
            end.is_end = True
            return end

        @staticmethod
        def insertLeft(of, toins):
            if(of == None):
                raise Exception("Nullptr exception")        
            toins.left = of.left
            if(toins.left != None):
                toins.left.right = toins
            toins.right = of
            of.left = toins;
            return toins;

        @staticmethod
        def insertRight(of, toins):
            if(of == None):
                raise Exception("Nullptr exception")
            if(of.is_end):
                raise Exception("Cannot insert right of end")

            toins.right = of.right;
            toins.right.left = toins;

            toins.left = of;
            of.right = toins;
            return toins

        @staticmethod
        def insertFirst(toins):
            if(toins == None):
                raise Exception("Nullptr exception")
            end = HistogramVBW.Bin.create_end()
            HistogramVBW.Bin.insertLeft(end, toins)
            return (toins,end)

        @staticmethod
        def getBin(start, v):
            if(start == None):
                raise Exception("Nullptr exception")
            if(start.is_end or v <= start.l):
                return None;
  
            cur = start;
            next = cur.right;
            while(cur.is_end == False):
                if(v <= cur.u):
                    return cur
                cur = next
                next = cur.right
            return None

        @staticmethod
        def split(_bin, about):
            if(_bin == None or _bin.is_end):
                raise Exception("Invalid bin")
            if(about < _bin.l or about > _bin.u):
                raise Exception("Split location not inside bin")
            verbosePrint("Splitting bin ",_bin,"about",about)
            if(about == _bin.u):
                verbosePrint("Split point matches upper edge, doing nothing")
                return (_bin,_bin.right); #nothing needed
  
            bw = _bin.u - _bin.l;
            o = [0,0]
            fracs = [ (about - _bin.l)/bw, 0 ]
            fracs[1] = 1. - fracs[0]
            count = float(_bin.c)
            lrg = 0
            debt = count
            for i in range(2):
                o[i] = math.floor(fracs[i] * count + 0.5);
                debt -= o[i];
                if(fracs[i] > fracs[lrg]):
                    lrg = i;
        
            #Assign debt to largest fraction with preference from left
            o[lrg] += debt; 
            HistogramVBW.Bin.insertRight(_bin, HistogramVBW.Bin(about, _bin.u, o[1]))
            
            _bin.u = about;
            _bin.c = int(o[0]);
            verbosePrint("Split bin into ",_bin,"and",_bin.right)
            return (_bin, _bin.right)

        @staticmethod
        def size(first):
            if(first == None):
                return 0
  
            cur = first
            sz = 0
            while(cur.is_end == False):
                sz = sz+1;
                cur = cur.right  
            return sz

        @staticmethod
        def printChain(first):
            cur = first
            out=""
            if(first == None):
                return ""
            while(cur.is_end == False):
                out = out + str(cur) + " "
                cur = cur.right
            return out


    def __init__(self):
        self.m_min = None
        self.m_max = None
        self.first = None
        self.end = None

    def __deepcopy__(self,memo):
        out = HistogramVBW()
        out.m_min = self.m_min
        out.m_max = self.m_max
        if(self.first == None):
            out.first = None
            out.end = None
        else:
            out.first, out.end = HistogramVBW.Bin.insertFirst( HistogramVBW.Bin(self.first.l, self.first.u, self.first.c) )
            if(self.first.right.is_end == False):
                in_cur = self.first.right
                dest = out.first
                while(in_cur.is_end == False):
                    HistogramVBW.Bin.insertRight(dest, HistogramVBW.Bin(in_cur.l, in_cur.u, in_cur.c) )
                    dest = dest.right
                    in_cur = in_cur.right
        return out

    def setup(self, edges, counts, minval=None, maxval=None):
        if len(edges) != len(counts) + 1:
            raise Exception("Invalid array sizes")
        Nbin = len(counts)
        if(Nbin == 0):
            return

        self.first, self.end = self.Bin.insertFirst( self.Bin(edges[0],edges[1], counts[0]) )

        hp = self.first
        for b in range(1,Nbin):            
            hp = self.Bin.insertRight(hp, self.Bin(edges[b],edges[b+1],counts[b]) )

        if minval == None:
            self.m_min = edges[0]
        else:
            self.m_min = minval

        if maxval == None:
            self.m_max = edges[-1]
        else:
            self.m_max = maxval

    def Nbin(self):
        return self.Bin.size(self.first)

    def getBinByIndex(self, idx):
        if(self.first == None):
            return None
        curidx = 0
        cur = self.first
  
        while(cur.is_end == False):
            if(idx == curidx):
                return cur
            cur = cur.right
            curidx += 1

        return None

    def totalCount(self):
        if(self.first == None):
            return 0
  
        cur = self.first
        out = 0.
        while(cur.is_end == False):
            out += cur.c
            cur = cur.right
        return out

    def getBin(self, v):
        return self.Bin.getBin(self.first, v)


    def extractUniformCountInRangeInt(self,l,u):
        verbosePrint("Extracting count in range",l,":",u)
        if(u<=l):
            raise Exception("Invalid range, require u>l but got l=%f u=%f" % (l,u))
        if(self.first == None):
            raise Exception("Histogram is empty")

        if(self.m_max == self.m_min):
            #Ignore the bin edges, the data set is a delta function
            v = self.m_max;
            _bin = self.getBin(v);
            count = _bin.c
            verbosePrint("extractUniformCountInRangeInt range",l,":",u,"evaluating for max=min=",v,": data are in bin with count",count,": l<v?",int(l<v)," u>=v?",int(u>=v))
            if(l < v and u>= v):
                _bin.c = 0;
                verbosePrint("returning",count)
                return count
            else:
                verbosePrint("returning",0)
                return 0


        last = self.end.left
        bl = self.Bin.getBin(self.first, l)

        if(bl != None): #left edge is inside histogram
            verbosePrint("Lower edge",l,"in bin",bl)
            bl = self.Bin.split(bl,l)[1];
            if(bl.is_end == True):
                verbosePrint("Right of split point is end")
                #If the split point matches the upper edge of the last bin, the .second pointer is END
                return 0
    
        elif(l <= self.first.l): #left edge is left of histogram
            bl = self.first;
            verbosePrint("Lower edge is left of histogram")
        elif(l > last.u): #left edge is right of histogram
            return 0
        else: #should not happen
            assert(False)


        last = self.end.left #update last in case it changed

        bu = self.Bin.getBin(bl, u)
        if(bu != None): #right edge is inside histogram
            bu = self.Bin.split(bu,u)[0];
        elif(u <= self.first.l): #right edge is left of histogram
            return 0
        elif(u > last.u): #right edge is right of histogram
            verbosePrint("Upper edge is right of histogram")
            bu = last
        else:
            assert(False)


        verbosePrint("Zeroing bins between",bl,"and",bu)
        h = bl
        out = 0
        while(h is not bu):
            out += h.c
            h.c = 0;
            h = h.right
  
        out += h.c;
        h.c = 0;
        return out

    #Run the function f on the object 'action' for every bin
    def binAction(self, action):
        if(self.first == None):
            return
        cur = self.first
        while(cur.is_end == False):
            action.f(cur)
            cur = cur.right

    #Return a tuple containing lists of the edges and counts
    def getEdgesAndCounts(self):
        class action:
            def __init__(self):
                self.edges = []
                self.counts = []
            def f(self, b):
                if(len(self.edges) == 0):
                    self.edges.append(b.l)
                self.edges.append(b.u)
                self.counts.append(b.c)
        a = action()
        self.binAction(a)
        return (a.edges,a.counts)

    #Return a histogram that has bin rebinned so that its edges match those provided
    #counts outside of that range are ignored
    def rebin(self, new_edges):
        new_counts = []
        cp = copy.deepcopy(self)
        for i in range(1, len(new_edges)):
            l = new_edges[i-1]
            u = new_edges[i]        
            new_counts.append( cp.extractUniformCountInRangeInt(l,u) )
        out = HistogramVBW()
        out.setup(new_edges,new_counts)
        return out
        

    #Difference of two histograms
    #If return_rebinned == True the result will be a tuple of the rebinned histograms of a,b followed by the histogram of the diff
    #Otherwise it will just return the diffed histogram
    @staticmethod
    def diff(a,b,return_rebinned=False):
        #Choose the range from the lowest low and highest high, and the bin width from the smallest
        class action:
            def __init__(self):
                self.bw = None
                self.lowest = None
                self.highest = None
            def f(self, b):
                bw = b.u - b.l
                if self.bw == None or bw < self.bw:
                    self.bw = bw
                if self.lowest == None or b.l < self.lowest:
                    self.lowest = b.l
                if self.highest == None or b.u > self.highest:
                    self.highest = b.u

        action_a = action()
        a.binAction(action_a)
        action_b = action()
        b.binAction(action_b)

        lowest = min(action_a.lowest, action_b.lowest)
        highest = max(action_a.highest, action_b.highest)
        bw = min(action_a.bw, action_b.bw)
        
        verbosePrint("Diff range",lowest,highest,"bin width",bw)
        
        #Get new edges
        edges = [lowest]
        u = lowest
        while(u < highest):
            u += bw
            edges.append(u)

        #Rebin both histograms onto the new range
        arb = a.rebin(edges)
        brb = b.rebin(edges)
        ae,ac = arb.getEdgesAndCounts()
        be,bc = brb.getEdgesAndCounts()
        assert(len(ae) == len(edges))
        assert(len(be) == len(edges))
        for i in range(len(ac)):
            ac[i] -= bc[i]
        out = HistogramVBW()
        out.setup(edges,ac)

        if(return_rebinned == True):
            return (arb, brb, out)
        else:
            return out



def testHistogramVBW_Basics():
  hv = HistogramVBW()
  assert(hv.Nbin() == 0)
  
  hv.setup([0,1,2,3], [1,2,3], 1e-12, 3);

  bins = [None for i in range(3)]

  assert(hv.Nbin() == 3)
  b = hv.getBinByIndex(0);
  assert(b != None)
  assert(b.l == 0)
  assert(b.u == 1);
  assert(b.c == 1)
  assert(b.left == None)

  bins[0] = b;
  bp = b;
  bn = b.right;

  b = hv.getBinByIndex(1);
  assert(b != None)
  assert(b is bn);
  assert(b.l == 1);
  assert(b.u == 2);
  assert(b.c == 2);
  assert(b.left is bp);
 
  bins[1] = b;
  bp = b
  bn = b.right

  b = hv.getBinByIndex(2);
  assert(b != None)
  assert(b is bn);
  assert(b.l == 2);
  assert(b.u == 3);
  assert(b.c == 3);
  assert(b.left is bp);
  assert(b.right != None)
  assert(b.right.is_end == True);
  
  bins[2] = b

  #Test getBin
  assert(hv.getBin(0.5) is bins[0])
  assert(hv.getBin(1.5) is bins[1])
  assert(hv.getBin(2.5) is bins[2])

  assert(hv.getBin(0.0) == None)
  assert(hv.getBin(3.01) == None)

  assert( abs(hv.totalCount() - 6.0) < 1e-8 )

  #Test revert to lists
  e,c = hv.getEdgesAndCounts()
  assert( e == [0,1,2,3] )
  assert( c == [1,2,3] )

  

  #Test copy
  #hv.setup([0,1,2,3], [1,2,3], 1e-12, 3);
  hcp = copy.deepcopy(hv)
  assert(hcp.Nbin() == 3)
  assert(hcp.first is not hv.first)
  assert(hcp.end is not hv.end)
  for i in range(3):
      aa = hv.getBinByIndex(i)
      bb = hcp.getBinByIndex(i)
      assert(aa is not bb)
      assert(aa.l == bb.l)
      assert(aa.u == bb.u)
      assert(aa.c == bb.c)

  print("Passed basic tests")


def testHistogramVBW_extractUniformCountInRangeInt():
    #Check for a normal histogram

    #total sum
    h = HistogramVBW()
    h.setup([0,1,2,3], [1,2,3], 1e-12, 3);
    assert(h.extractUniformCountInRangeInt(0,3) == 6)
    assert(h.totalCount() == 0.)


    #first bin
    h = HistogramVBW()
    h.setup([0,1,2,3], [1,2,3], 1e-12, 3);
    assert(h.extractUniformCountInRangeInt(0,1) == 1.)
    assert(h.Nbin() == 3)
    assert(h.getBinByIndex(0).c == 0.)

    
    #second bin
    h = HistogramVBW()
    h.setup([0,1,2,3], [1,2,3], 1e-12, 3);
    assert(h.extractUniformCountInRangeInt(1,2) == 2.)
    assert(h.Nbin() == 3)
    assert(h.getBinByIndex(1).c == 0.)


    #last bin
    h = HistogramVBW()
    h.setup([0,1,2,3], [1,2,3], 1e-12, 3);
    assert(h.extractUniformCountInRangeInt(2,3) == 3.)
    assert(h.Nbin() == 3)
    assert(h.getBinByIndex(2).c == 0.)
   

    #range left of histogram
    h = HistogramVBW()
    h.setup([0,1,2,3], [1,2,3], 1e-12, 3);
    assert(h.extractUniformCountInRangeInt(-1,0) == 0.)
    assert(h.Nbin() == 3)

    #range right of histogram
    h = HistogramVBW()
    h.setup([0,1,2,3], [1,2,3], 1e-12, 3);
    assert(h.extractUniformCountInRangeInt(3.01,4) == 0.)
    assert(h.Nbin() == 3)

    #range beyond but includes histogram
    h = HistogramVBW()
    h.setup([0,1,2,3], [1,2,3], 1e-12, 3);
    assert(h.extractUniformCountInRangeInt(-1,4) == 6.)
    assert(h.totalCount() == 0.)

    #lower bound matches upper edge of histogram
    h = HistogramVBW()
    h.setup([0,1,2,3], [1,2,3], 1e-12, 3);
    assert(h.extractUniformCountInRangeInt(3,4) == 0.)
    assert(h.totalCount() == 6.);

    #partial bin
    h = HistogramVBW()
    h.setup([0,1,2,3], [1,2,3], 1e-12, 3);
    assert(h.Nbin() == 3)
    assert(h.extractUniformCountInRangeInt(2.5,3) == 2.0)
    assert(h.Nbin() == 4)

    lsplit = h.getBinByIndex(2)
    assert(lsplit.c == 1.0)
    assert(lsplit.l == 2.0)
    assert(lsplit.u == 2.5)

    rsplit = h.getBinByIndex(3)
    assert(rsplit.c == 0.0)
    assert(rsplit.l == 2.5)
    assert(rsplit.u == 3.0)

    assert(lsplit.right is rsplit)
    assert(rsplit.left is lsplit)

    
    #multiple partial bins
    h = HistogramVBW()
    h.setup([0,1,2,3], [1,2,3], 1e-12, 3)
    assert(h.Nbin() ==3)	    
    assert(h.extractUniformCountInRangeInt(0.5,2.5) == 1+2+1)
    assert(h.Nbin() ==5)

    lsplitl = h.getBinByIndex(0)
    assert(lsplitl.c == 0.0)
    assert(lsplitl.l == 0.0)
    assert(lsplitl.u == 0.5)

    lsplitr = h.getBinByIndex(1)
    assert(lsplitr.c == 0.0)
    assert(lsplitr.l == 0.5)
    assert(lsplitr.u == 1.0)
      
    assert(lsplitl.right is lsplitr)
    assert(lsplitr.left is lsplitl)

    b = h.getBinByIndex(2)
    assert(b.c == 0.)

    rsplitl = h.getBinByIndex(3)
    assert(rsplitl.c == 0.0)
    assert(rsplitl.l == 2.0)
    assert(rsplitl.u == 2.5)

    rsplitr = h.getBinByIndex(4)
    assert(rsplitr.c == 2.0)
    assert(rsplitr.l == 2.5)
    assert(rsplitr.u == 3.0)

    assert(rsplitl.right is rsplitr)
    assert(rsplitr.left is rsplitl)

    
    #Check special treatment for a histogram with min=max
    h = HistogramVBW()
    h.setup([0,1], [3], 0.5, 0.5)
    assert(h.Nbin() ==1)
    assert(h.extractUniformCountInRangeInt(0.499,0.5) ==3.)
    assert(h.Nbin() ==1)
    assert(h.getBinByIndex(0).c ==0.)

    h = HistogramVBW()
    h.setup([0,1], [3], 0.5, 0.5)
    assert(h.Nbin() == 1) 
    assert(h.extractUniformCountInRangeInt(0.5,0.501) == 0.)
    assert(h.Nbin() == 1)
    assert(h.getBinByIndex(0).c == 3.) 

    print("extractUniformCountInRangeInt test passed")

def testHistogramVBW_rebin():
    hv = HistogramVBW()
  
    hv.setup([0,1,2,3], [4,2,6], 1e-12, 3);

    #Check a slice that aligns with existing bin edges
    hsliced = hv.rebin( [1,2] )
    b = hsliced.getBinByIndex(0)
    assert(b.l == 1)
    assert(b.u == 2)
    assert(b.c == 2)

    #Check a slice that doesn't 
    hsliced = hv.rebin( [0.5,2.5] )
    b = hsliced.getBinByIndex(0)
    assert(b.l == 0.5)
    assert(b.u == 2.5)
    assert(b.c == 7)

    print("rebin test passed")
  
    
def testHistogramVBW_Diff():
    #Histograms with same range
    g = HistogramVBW()
    g.setup([0,1,2,3], [2,3,4], 1e-12, 3)

    h = HistogramVBW()
    h.setup([0,1,2,3], [3,4,5], 1e-12, 3)

    edges, counts = HistogramVBW.diff(g,h).getEdgesAndCounts()
    print(edges,counts)
    assert(len(counts) == 3)
    assert(len(edges) == 4)
    assert(edges == [0,1,2,3])
    assert(counts == [-1,-1,-1])

    #Histogram with same total range but bin width of second with smaller bins
    g = HistogramVBW()
    g.setup([0,1,2,3], [2,6,4], 1e-12, 3)

    h = HistogramVBW()
    h.setup([0,0.5,1,1.5,2,2.5,3], [1,1,3,3,2,2], 1e-12, 3)

    edges, counts = HistogramVBW.diff(g,h).getEdgesAndCounts()
    print(edges,counts)
    assert(len(counts) == 6)
    assert(len(edges) == 7)
    assert(edges == [0,0.5,1,1.5,2,2.5,3])
    assert(counts == [0,0,0,0,0,0])


    #Histogram with different total range and bin width of second with smaller bins
    g = HistogramVBW()
    g.setup([-1,0,1,2,3,4], [8,2,6,4,8], 1e-12, 3)

    h = HistogramVBW()
    h.setup([0,0.5,1,1.5,2,2.5,3], [1,1,3,3,2,2], 1e-12, 3)

    edges, counts = HistogramVBW.diff(g,h).getEdgesAndCounts()
    print(edges,counts)
    assert(edges == [-1.0,-0.5,0,0.5,1,1.5,2,2.5,3,3.5,4])
    assert(counts == [4,4,0,0,0,0,0,0,4,4])




    print("diff test passed")
    



    
if __name__ == '__main__':
    verbose = True
    testHistogramVBW_Basics()
    testHistogramVBW_extractUniformCountInRangeInt()
    testHistogramVBW_rebin()
    testHistogramVBW_Diff()
