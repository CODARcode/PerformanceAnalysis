#Generate a Makefile for the 3rdparty include directory (run from inside the directory)

echo 'thirdpartydir = $(includedir)/chimbuko/3rdparty' > Makefile.am
echo -n 'nobase_thirdparty_HEADERS = ' >> Makefile.am

for i in $(find . -name '*.hpp' | sed 's/^\.\///'); do
    echo -n "$i " >> Makefile.am
done
