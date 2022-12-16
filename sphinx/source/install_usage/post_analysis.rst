********************************************
Analysing the results of a run with Chimbuko
********************************************

The output of Chimbuko is stored in the provenance database. The database is sharded over multiple files of the form **provdb.${SHARD}.unqlite** that are by default output into the :file:`chimbuko/provdb` directory in the run path. We provide several tools for analyzing the contents of the provenance database:

1. **provdb_query**, a command-line tool for filtering and querying the database

2. A **Python module** for connecting to the database, filtering and querying, for use in custom analysis tools

3. **provdb-python**, a Python-based command-line tool for analyzing the database.   

Using the post-analysis **Python module**
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The module

.. code:: bash
	  
	  scripts/provdb_python/src/provdb_python/provdb_interact.py

in the PerformanceAnalysis source provides an interface for connecting and querying the database. Futher documentation is forthcoming.
	  

Using **provdb-python**
~~~~~~~~~~~~~~~~~~~~~~~

This tool can also be installed via Spack

.. code:: bash
	  
	  spack repo add /src/develop/PerformanceAnalysis/spack/repo/chimbuko
          spack install chimbuko-provdb-python


Or using **pip** from the Chimbuko source (note the **py-mochi-sonata** Spack module is expected to be loaded):

.. code:: bash
	  
	  git clone -b ckelly_develop https://github.com/CODARcode/PerformanceAnalysis.git && \
	  python3.6 -m pip install PerformanceAnalysis/scripts/provdb_python/


Once installed the tool can be used as a regular command line function, executed from the directory containing the provenance database UnQLite files:
	  
.. code:: bash
	  
          cd chimbuko/provdb
	  provdb-python 

Several components are available, with further documentation forthcoming.
	  
   
Using **provdb_query**
~~~~~~~~~~~~~~~~~~~~~~

The provenance database is stored in a single file, **provdb.${SHARD}.unqlite** in the job's run directory. From this directory the user can interact with the provenance database via the visualization module. A more general command line interface to the database is also provided via the **provdb_query** tool that allows the user to execute arbitrary jx9 queries on the database.

The **provdb_query** tool has two modes of operation: **filter** and **execute**.

Filter mode
-----------

**filter** mode allows the user to provide a jx9 filter function that is applied to filter out entries in a particular collection. The result is displayed in JSON format and can be piped to disk. It can be used as follows:

.. code:: bash

	  provdb_query filter ${COLLECTION} ${QUERY}

Where the variables are as follows:

- **COLLECTION** : One of the three collections in the database, **anomalies**, **normalexecs**, **metadata** (cf :ref:`introduction/provdb:Provenance Database`).
- **QUERY**: The query, format described below.

The **QUERY** argument should be a jx9 function returning a bool and enclosed in quotation marks. It should be of the format

.. code:: bash

	  QUERY="function(\$entry){ return \$entry['some_field'] == ${SOME_VALUE}; }"


Alternatively the query can be set to "DUMP", which will output all entries.

The function is applied sequentially to each element of the collection. Inside the function the entry is described by the variable **$entry**. Note that the backslash-dollar (\\$) is necessary to prevent the shell from trying to expand the variable. Fields of **$entry** can be queried using the square-bracket notation with the field name inside. In the sketch above the field "some_field" is compared to a value **${SOME_VALUE}** (here representing a numerical value or a value expanded by the shell, *not* a jx9 variable!).

Some examples:

- Find every anomaly whose function contains the substring "Kokkos":

.. code:: bash

	  provdb_query filter anomalies "function(\$a){ return substr_count(\$a['func'],'Kokkos') > 0; }"

- Find all events that occured on a GPU:

.. code:: bash

	  provdb_query filter anomalies "function(\$a){ return \$a['is_gpu_event']; }"

Filter-global mode
------------------

If the pserver is connected to the provenance database, at the end of the run the aggregated function profile data and global averages of counters will be stored in a "global" database "provdb.global.unqlite". This database can be queried using the **filter-global** mode, which like the above allows the user to provide a jx9 filter function that is applied to filter out entries in a particular collection. The result is displayed in JSON format and can be piped to disk. It can be used as follows:

.. code:: bash

	  provdb_query filter-global ${COLLECTION} ${QUERY}

Where the variables are as follows:

- **COLLECTION** : One of the two collections in the database, **func_stats**, **counter_stats**.
- **QUERY**: The query, format described below.

The formatting of the **QUERY** argument is described above.

Execute mode
------------

**execute** mode allows running a complete jx9 script on the database as a whole, allowing for more complex queries that collect different outputs and span collections.

.. code:: bash

	  provdb_query execute ${CODE} ${VARIABLES} ${OPTIONS}

Where the variables are as follows:

- **CODE** : The jx9 script
- **VARIABLES** : a comma-separated list (without spaces) of the variables assigned by the script

The **CODE** argument is a complete jx9 script. As above, backslashes ('\') must be placed before internal '$' and '"' characters to prevent shell expansion.

If the option **-from_file** is specified the **${CODE}** variable above will be treated as a filename from which to obtain the script. Note that in this case the backslashes before the special characters are not necessary.

