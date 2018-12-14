#!/bin/bash

mkdir -p ../doc 
pydoc3 -w `find ../lib/ -name '*.py'`
mv *.html ../doc/