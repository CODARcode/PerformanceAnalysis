#!/bin/bash

mkdir -p ../doc 
pydoc -w `find ../lib/ -name '*.py'`
mv *.html ../doc/