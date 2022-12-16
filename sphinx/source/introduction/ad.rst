*****************
On-node AD Module
*****************

The on-node anomaly detection (AD) module (per application process) takes streamed trace data provided by the TAU-instrumented binary. Each AD instance parses the streamed trace data and maintains a function call stack along with any communication events (if available) and counters. The anomaly detection algorithm compares the function execution to those it has seen previously, from which a decision is made as to whether the execution is anomalous. Detailed "provenance" information is collected for every anomaly as well as a limited number of "normal" executions (kept for comparison purposes) and stored in a database. Any remaining trace data is periodically discarded. By focusing primarily on anomalous events, Chimbuko significantly reduces the trace data volume while keeping those events that are important for understanding performance issues.

Below we provide a brief overview of the steps performed by the AD in processing the data.

Parser
------

Currently, the trace data is streamed from a TAU-instrumented binary via `ADIOS2 <https://github.com/ornladios/ADIOS2>`_. The streaming is performed in a "stepped" fashion, whereby trace data is collected over some short time period and then communicated to the AD instances, after which the next step begins. We will refer to these steps as "I/O steps" or "I/O frames" throughout this document. We provide a class, `ADParser <../api/api_code.html#adparser>`__ to connect to an ADIOS2 writer side and
fetch necessary data for the performance analysis. The parser performs some rudimentary validation on the data, and, depending on the context might augment the data with additional tags; for example, when running a workflow with multiple distinct binaries, the parser appends a 'program index' to the data to allow the programs to be differentiated within Chimbuko.

Pre-processing
--------------

In the pre-processing step, the AD takes the data for the present I/O step and generates a call stack tree for each program index, rank and thread (See class `ADEvent <../api/api_code.html#adevent>`__). Communication events, GPU kernel launches and counters that occurred during the function execution are associated with the calls and the inclusive (including child calls) and exclusive (excluding child calls) runtimes are computed.

Anomaly detection
-----------------

With the preprocessed data the AD instance applies its anomaly detection algorithm to filter the data. In order to provide the most complete and robust information to the algorithm, function execution statistics computed locally are aggregated globally with the data on the **Parameter Server** (See `ADOutlier <../api/api_code.html#adoutlier>`__) prior to performing the anomaly detection.

Anomaly detection algorithm
~~~~~~~~~~~~~~~~~~~~~~~~~~~

At present, Chimbuko offers three different algorithms for anomaly detection, that we describe below. All approaches dynamically generate models of the function execution time, which are synchronized through the parameter server. By default the exclusive execution time (excluding child calls) is used but the inclusive time can be used with the appropriate command line option to the AD.

1. **Histogram Based Outlier Score (HBOS)** is a deterministic and non-parametric statistical anomaly detection algorithm. HBOS is an unsupervised anomaly detection algorithm which scores data in linear time. It supports dynamic bin widths which ensures long-tail distributions of function executions are captured and global anomalies are detected better. HBOS normalizes the histogram and calculates the anomaly scores by taking inverse of estimated densities of function executions. The score is a multiplication of the inverse of the estimated densities given by the following Equation

   .. math::
      HBOS_{i} = \log_{2} (1 / density_{i} + \alpha)

   where :math:`i` is the index of a particular a function execution and :math:`density_{i}` is function execution probability. The offset :math:`\alpha` is chosen such that the scores lie in the range :math:`0\to 100`. HBOS works in :math:`O(nlogn)` using dynamic bin-width or in linear time :math:`O(n)` using fixed bin width. After scoring, the top 1% of scores are filtered as anomalous function executions. This filter value can be set at runtime to adjust the density of detected anomalies.

   This algorithm is quite robust and is able to cope with functions of arbitrary distribution, although the lack of foreknowledge of the appropriate bin width and data range for any given function makes it somewhat susceptible to "compression artefacts" early in the run, although the effects of these can be expected to diminish with time as the model settles and the true peaks become dominant. We recommend this algorithm as the primary, default option.

2. **Sample Standard Deviation (SSTD)** defines anomalous function calls as those that have a longer (or shorter) execution time than a upper (or a lower) threshold. The thresholds are defined as follows:

   .. math::
      threshold_{upper} = \mu_{i} + \alpha * \sigma_{i} \\
      threshold_{lower} = \mu_{i} - \alpha * \sigma_{i}

  where :math:`\mu_{i}` and :math:`\sigma_{i}` are the mean and standard deviation of the execution time of a function :math:`i`, respectively, and :math:`\alpha` is a control parameter (smaller values increase the number of anomalies detected while potentially increasing the number of false-positives).

  This algorithm is very simple and its results easy to interpret, but it is susceptible to the poisoning of the model by extreme anomalous events, which when included in the model dramatically increase the standard deviation to the point that no further anomalies are detected. Also, this model implicitly assumes a unimodal distribution of approximately normal distribution, and as such does not adequately describe functions with varying runtimes (perhaps due to being run with different parameters) whose distributions are multimodal.

3. **COPula based Outlier Detection (COPOD)** is a deterministic, parameter-free anomaly detection algorithm. It computes empirical copulas for each sample in the dataset. A copula defines the dependence structure between random variables. For this single-dimensional dataset it is equivalent to the cumulative distribution function of the sample distribution. For each sample in the dataset, the COPOD algorithm computes left-tail empirical copula from the left-tail empirical cumulative distribution function, the right-tail copula from the right-tail empirical cumulative distribution function, and a skewness-corrected empirical copula using a skewness coefficient calculated from left-tail and right-tail empirical cumulative distribution functions. These three computed values are interpreted as left-tail, right-tail, and skewness-corrected probabilities, respectively. The lowest probability value results in largest negative-log value, which is the score assigned to the sample in the dataset. Samples with the highest scores in the dataset are tagged as anomalous.

   This approach is also histogram-based under-the-hood, but the use of the copula makes it less susceptible to artefacts introduced by the binning procedure. However this algorithm also suffers from an assumption of unimodal data; only data points far above or far below all peaks of the histogram will be labeled as outliers, and those in-between peaks will not. 

(See `ADOutlier <../api/api_code.html#adoutlier>`__ and `RunStats <../api/api_code.html#runstats>`__, `HbosParam <../api/api_code.html#hbosparam>`__ and `CopodParam <../api/api_code.html#copodparam>`__).

Provenance data collection
--------------------------

Once anomalous and non-anomalous functions are tagged, Chimbuko walks the call stack and generates detailed provenance information of the anomalous executions and a select number of normal executions (for comparison). (See `ADAnomalyProvenance <../api/api_code.html#adanomalyprovenance>`__) These data are sent to the centralized "provenance database" for later analysis.

Stream local vizualisation data
-------------------------------

The visualization module displays various real-time statistics such as the number of anomalies per rank. This information is collected by the AD (cf. `ADLocalFuncStatistics <../api/api_code.html#adlocalfuncstatistics>`__ and `ADLocalCounterStatistics <../api/api_code.html#adlocalcounterstatistics>`__) and is aggregated on the parameter server, from which it is sent periodically (via curl) to the visualization module. The visualization module is capable also of interacting with the provenance database to obtain detailed information on specific anomalies per user request.


Post-processing
---------------

Once the data have been processed the call stack for the present I/O step is discarded and Chimbuko moves onto processing the next step. In this way the amount of trace data maintained is dramatically reduced to just the provenance data and any statistics that we maintain.
