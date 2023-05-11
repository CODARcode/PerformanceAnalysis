SUBDIRS = include src app test sphinx 3rdparty scripts sim
nobase_include_HEADERS = chimbuko_config.h

install-provdb-python:
	python3 -m pip install --user $(top_srcdir)/scripts/provdb_python/
#python3 -m pip install --prefix $(prefix) $(top_srcdir)/scripts/provdb_python/

uninstall-provdb-python:
	python3 -m pip uninstall -y provdb-python
#Below are needed if installing into prefixc becase pip doesn't allow uninstall of packages installed with --prefix?????!?!?	 (I HATE PYTHON)
#	rm -rf $(prefix)/lib/python3.6
#	rm $(prefix)/bin/provdb-python
