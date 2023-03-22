#pragma once

#include<vector>
#include <nlohmann/json.hpp>

namespace chimbuko{

  class Histogram;

  /**
   * @brief Base class for functional that determines the histogram bin width
   */
  struct binWidthSpecifier{
    /**
     * @brief Determine the bin with for a data set
     * @param runtimes The data
     * @param min The min value in the data array
     * @param max The max value in the data array
     */
    virtual double bin_width(const std::vector<double> &runtimes, const double min, const double max) const = 0;

    /**
     * @brief Determine the bin width for the merger of two histograms
     */
    virtual double bin_width(const Histogram &a, const Histogram &b, const double min, const double max) const = 0;
    virtual ~binWidthSpecifier(){}
  };
  /**
   * @brief Use a fixed bin width
   */
  struct binWidthFixed: public binWidthSpecifier{
    double width;
    binWidthFixed(double width): width(width){}

    double bin_width(const std::vector<double> &runtimes, const double min, const double max) const override{ return width; }
    double bin_width(const Histogram &a, const Histogram &b, const double min, const double max) const{ return width; }
  };
  /**
   * @brief Determine the bin width using Scott's rule
   */
  struct binWidthScott: public binWidthSpecifier{
    double bin_width(const std::vector<double> &runtimes, const double min, const double max) const override;
    double bin_width(const Histogram &a, const Histogram &b, const double min, const double max) const override;
  };
  /**
   * @brief Determine the bin width from the range assuming a fixed number of bins
   */
  struct binWidthFixedNbin: public binWidthSpecifier{
    int nbin;
    binWidthFixedNbin(int nbin): nbin(nbin){}
    double bin_width(const std::vector<double> &runtimes, const double min, const double max) const override;
    double bin_width(const Histogram &a, const Histogram &b, const double min, const double max) const override;
  };
  /**
   * @brief Base class for binWidthSpecifier derivatives that impose a cap on the number of bins
   */
  struct binWidthMaxNbinBase{
    int maxbins;
    binWidthMaxNbinBase(int maxbins): maxbins(maxbins){}

    /**
     * @brief Compute the current #bins and adjust the bin width accordingly if too large
     */
    double correct_bin_width(double bin_width, const double min, const double max) const;
  };

  /**
   * @brief Use a fixed bin width but adjust if the number of bins is larger than some maximum
   */
  struct binWidthFixedMaxNbin: public binWidthMaxNbinBase, public binWidthSpecifier{
    int maxbins;
    double width;
    binWidthFixedMaxNbin(double width, int maxbins): width(width),binWidthMaxNbinBase(maxbins){}

    double bin_width(const std::vector<double> &runtimes, const double min, const double max) const override{ return correct_bin_width(width,min,max); }
    double bin_width(const Histogram &a, const Histogram &b, const double min, const double max) const{ return correct_bin_width(width,min,max); }
  };
  /**
   * @brief Determine the bin width using Scott's rule but adjust if the number of bins is larger than some maximum
   */
  struct binWidthScottMaxNbin: public binWidthMaxNbinBase, public binWidthScott{
    binWidthScottMaxNbin(int maxbins): binWidthMaxNbinBase(maxbins){}

    double bin_width(const std::vector<double> &runtimes, const double min, const double max) const override{ return correct_bin_width(this->binWidthScott::bin_width(runtimes,min,max),min,max); }
    double bin_width(const Histogram &a, const Histogram &b, const double min, const double max) const override{ return correct_bin_width(this->binWidthScott::bin_width(a,b,min,max),min,max); }
  };




  /**
   * @brief Fixed bin width histogram Implementation
   *
   * Note: lower bin edges are exclusive and upper edges inclusive,  i.e. for bin b with edges  (l,u), all data v in the bin have  l < v <= u
   */
  class Histogram {

  public:

    /**
     * @brief Construct a Histogram object
     */
    Histogram();

    /**
     * @brief Destroy Histogram object
     */
    ~Histogram();


    /**
     * @brief Construct a Histogram object from the data provided
     * @param data a vector<double> of data
     * @param bwspec a functional that specifies the bin width. Default to using Scott's rule
     */
    Histogram(const std::vector<double> &data, const binWidthSpecifier &bwspec = binWidthScott());

    /**
     * @brief Serialize using cereal
     */
    template<class Archive>
    void serialize(Archive & archive){
      archive(m_counts, m_bin_edges, m_min, m_max);
    }

    void clear();

    /**
     * @brief Create new histogram with the bin width chosen according to the functional provided
     * @param r_times a vector<double> of function run times
     * @param bwspec a functional that specifies the bin width. Default to using Scott's rule
     */
    void create_histogram(const std::vector<double>& r_times, const binWidthSpecifier &bwspec = binWidthScott());

    /**
     * @brief Create new histogram using the bin edges and min/max provided
     * @param data the data to be included in the histogram
     * @param min The minimum data value ever encountered
     * @param edges The bin edges. The first bin edge should be very slightly below min such that the value at min always falls in the first bin
     *
     * All data should lie within min and the last edge!
     */
    void create_histogram(const std::vector<double>& data, const double min, const std::vector<double> &edges);

    /**
     * @brief merges a Histogram with function runtimes
     * @param g: Histogram to merge
     * @param runtimes: Function runtimes
     * @param bwspec a functional that specifies the bin width of the merged histogram. Default to using Scott's rule
     * @return The merged histogram
     */
    static Histogram merge_histograms(const Histogram& g, const std::vector<double>& runtimes, const binWidthSpecifier &bwspec = binWidthScott());

    /**
     * @brief Combine two Histogram instances such that the resulting statistics are the union of the two Histograms
     * @param g: Histogram to merge into
     * @param l: Histogram to merge
     * @return result: Merged Histogram
     */
    static Histogram merge_histograms(const Histogram& g, const Histogram& l, const binWidthSpecifier &bwspec = binWidthScott());

    /*
     * @brief set bin counts in Histogram
     * @param c: vector of bin counts
     */
    void set_counts(const std::vector<double> & c) { m_counts = c; }

    /**
     * @brief set bin edges in Histogram
     * @param be: vector of bin edges
     */
    void set_bin_edges(const std::vector<double> &be){ m_bin_edges = be; }
    
    /**
    * @brief Set the minimum and maximum values of data represented by the histogram
    *
    * both min and max values should be inside the histogram bounds!
    */
    void set_min_max(double min, double max);
    
    /*
     * @brief New bin counts in Histogram
     * @param count: bin count value
     */
    void add2counts(const double count) {m_counts.push_back(count);}

    /*
     * @brief Update counts for a given index of bin in histogram
     * @param id: index of bin in Histogram
     * @param count: bin count value to update
     */
    void add2counts(const int id, const double count) {m_counts[id] += count;}

    /*
     * @brief New bin edges in histogram
     * @param bin_edge: vector of bin edges of histogram
     */
    void add2binedges(const double bin_edge) {m_bin_edges.push_back(bin_edge);}

    /**
     * @brief Get current vector of bin counts of Histogram
     * @return vector of bin counts
     */
    const std::vector<double>& counts() const {return m_counts;}

    /**
     * @brief Get current vector of bin edges of histogram
     * @return vector of bin edges
     */
    const std::vector<double>& bin_edges() const {return m_bin_edges;}

    /**
     * @brief Get the lower and upper edges of the given bin
     */
    inline std::pair<double,double> binEdges(const int bin) const{ return {m_bin_edges[bin],m_bin_edges[bin+1]}; }
    
    /**
     * @brief Get the count of a given bin
     */
    inline double binCount(const int bin) const{ return m_counts[bin]; }

    /**
     * @brief Get the bin width
     */
    double binWidth() const;

    /**
     * @brief Get the current statistics as a JSON object
     */
    nlohmann::json get_json() const;

    /**
     * @brief Return the number of bins
     */
    size_t Nbin() const{ return m_counts.size(); }

    /**
     * @brief Enums for reporting bin index outside of histogram
     */
    enum { LeftOfHistogram = -1, RightOfHistogram = -2 };
    
    /**
     * @brief Get the bin containing value 'v'
     * @param v The value
     * @param tol The tolerance, as a fraction of the closest bin width,  that we allow values to be below the smallest bin or above the largest bin and still consider the value within that bin
     * @return Bin index if within the histogram, otherwise either LeftOfHistogram or RightOfHistogram
     */
    int getBin(const double v, const double tol = 0.) const;


    /**
     * @brief Generate a representative data set from the histogram assuming a bin is represented by its midpoint value
     *
     * @return Vector of values of a size equal to the sum of the bin counts
     */
    std::vector<double> unpack() const;


    /**
     * @brief Get the value represented by a bin; we use the midpoint
     */
    inline double binValue(const size_t i) const{ return binValue(i, bin_edges()); }


    /**
     * @brief Workspace for empiricalCDF function allows repeated calls to avoid recomputing the total sum
     */
    struct empiricalCDFworkspace{
      bool set;
      double sum;
      empiricalCDFworkspace(): set(false){}

      double getSum(const Histogram &h);
    };
 
    /**
     * @brief Compute the empirical CDF for a given value assuming data in bins are uniformly distributed
     * @param value The value at to which the CDF is computed
     * @param workspace If non-null, workspace allows the preservation of the bin count sum between successive calls, reducing the cost
     */
    double empiricalCDF(const double value, empiricalCDFworkspace *workspace = nullptr) const;

    /**
     * @brief Return the sum of all the bin counts
     */
    double totalCount() const;

    /**
     * @brief Comparison operator
     */
    inline bool operator==(const Histogram &r) const{ return m_counts == r.m_counts && m_bin_edges == r.m_bin_edges && m_min == r.m_min && m_max == r.m_max; }
    inline bool operator!=(const Histogram &r) const{ return !(*this == r); }

    /**
     * @brief Return a copy of the histogram but whose bin edges are negated and the bin order reversed
     */
    Histogram operator-() const;

    /**
     * @brief Compute the skewness of the distribution, assuming the bins are represented by their midpoint value
     */
    double skewness() const;

    /**
     * @brief Obtain the count of values falling between the given bounds assuming a uniform distribution of points within a bin
     * @param l The lower bound
     * @param h The upper bound
     *
     * The result is non-integer if the bounds do not align with bin boundaries
     * Requires u>l
     */
    double uniformCountInRange(double l, double u) const;

    /**
     * @brief Compute bin width based on Scott's rule
     * @param vals: vector of runtimes
     * @return computed bin width
     */
    static double scottBinWidth(const std::vector<double> & vals);

    /**
     * @brief Compute bin width based on Scott's rule
     * @param global_counts: bin counts in global histogram on pserver
     * @param global_edges: bin edges in global histogram on pserver
     * @param local_counts: bin counts in local histogram in AD module
     * @param local_edges: bin edges in local histogram in AD module
     * @return computed bin width
     */
    static double scottBinWidth(const std::vector<double> & global_counts, const std::vector<double> & global_edges, const std::vector<double> & local_counts, const std::vector<double> & local_edges);


    /**
     * @brief Compute bin width based on Scott's rule
     * @param global_counts: bin counts in global histogram on pserver
     * @param global_edges: bin edges in global histogram on pserver
     * @param local_vals: local data values
     * @return computed bin width
     */
    static double scottBinWidth(const std::vector<double> & global_counts, const std::vector<double> & global_edges, const std::vector<double> & local_vals);


    /**
     * @brief Return the smallest value encountered
     */
    double getMin() const{ return m_min; }

    /**
     * @brief Return the largest value encountered
     */
    double getMax() const{ return m_max; }

    /**
     * @brief The lower bound of the histogram will be placed  getLowerBoundShiftMul()*bin_width below the minimum value
     */
    static double getLowerBoundShiftMul();


    /**
     * @brief Uniformly shift the bin edges by x
     */
    void shiftBinEdges(const double x);
    
  private:
    std::vector<double> m_counts; /**< Bin counts in Histogram*/
    std::vector<double> m_bin_edges; /**< Bin edges in Histogram*/
    double m_min; /**< Minimum value ever encountered*/
    double m_max; /**< Maximum value ever encountered*/

  protected:
    /**
     * @brief Get the value represented by a bin; we use the midpoint
     */
    static double binValue(const size_t i, const std::vector<double> & edges);

    /**
     * @brief Merge two histograms into a third assuming that the histogram bins are represented by bin_count copies of the bin's midpoint
     */
    static void merge_histograms_central_value(Histogram &combined, const Histogram& g, const Histogram& l);

    /**
     * @brief Merge two histograms into a third assuming that the data are uniformly distributed across the histogram bins
     */
    static void merge_histograms_uniform(Histogram &combined, const Histogram& g, const Histogram& l);
    
    static void merge_histograms_uniform_int(Histogram &combined, const Histogram& g, const Histogram& l);
  
  };


  Histogram operator+(const Histogram& g, const Histogram& l);

  std::ostream & operator<<(std::ostream &os, const Histogram &h);


  
  //A histogram implementation with variable bin widths
  class HistogramVBW{
  public:
    struct Bin{
      double l; /**< Lower edge*/
      double u; /**< Upper edge*/
      double c; /**< Bin count*/
      
      Bin* left;
      Bin* right;

      bool is_end;
      
      Bin(double l, double u, double c): l(l), u(u), c(c), left(nullptr), right(nullptr), is_end(false){}
  
      static std::pair<Bin*,Bin*> insertFirst(Bin* toins);
      static Bin* insertRight(Bin* of, Bin* toins);
      static Bin* insertLeft(Bin* of, Bin* toins);
      static void deleteChain(Bin* leftmost);
      static Bin* getBin(Bin* leftmost, double v);    
      static std::pair<Bin*,Bin*> split(Bin* bin, double about);
      static size_t size(Bin* leftmost);
    };

    HistogramVBW(): first(nullptr), end(nullptr), m_min(0), m_max(0){}

    ~HistogramVBW();

    void import(const Histogram &h);

    /**
     * @brief Construct from a fixed binwidth histogram
     */
    explicit HistogramVBW(const Histogram &h): HistogramVBW(){ import(h); }

    /**
     * @brief Get the bin containing value 'v'
     * @param v The value
     * @return Bin index if within the histogram, otherwise either Histogram::LeftOfHistogram or Histogram::RightOfHistogram
     */
    Bin const* getBin(const double v) const;

    /**
    * @brief Set the minimum and maximum values of data represented by the histogram
    *
    * both min and max values should be inside the histogram bounds!
    */
    void set_min_max(double min, double max);
    
    /**
     * @brief Return the number of bins
     */
    size_t Nbin() const;

    Bin const* getBinByIndex(const int idx) const;

    /**
     * @brief Return the sum of all the bin counts
     */
    double totalCount() const;

    /**
     * @brief Obtain the count of values falling between the given bounds assuming a uniform distribution of points within a bin. 
     * The number is rounded to the nearest integer and returned. The data within the range in the histogram is zeroed, creating new edges appropriately
     * @param l The lower bound
     * @param h The upper bound
     *
     * Requires u>l
     */
    double extractUniformCountInRangeInt(double l, double u);

    Bin const *getFirst() const{ return first; }
    Bin const *getEnd() const{ return end; }
  private:
    Bin *first;
    Bin *end;
    double m_min;
    double m_max;
  };

  std::ostream & operator<<(std::ostream &os, const HistogramVBW::Bin &b);
  std::ostream & operator<<(std::ostream &os, const HistogramVBW &h);


}
