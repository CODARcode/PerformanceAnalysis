#Generate a Makefile for the sim/include directory (run from inside the directory)
echo 'simincdir = $(prefix)/sim/include' > Makefile.am

echo -n 'nobase_siminc_HEADERS = ' >> Makefile.am

for i in $(find . -name '*.hpp' | sed 's/^\.\///'); do
    echo -n "$i " >> Makefile.am
done
