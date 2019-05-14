rm -rf src/tools/install # delete the GA libraries
for f in adjust.c collisions.c fapi.c onesided.c capi.c armci.c comex.c util_md.F util_cpusec.f;
do
  find . -name ${f}
  touch `find . -name ${f}`
  find . -name "${f%.c}.o"
  objs=`find . -name "${f%.c}.o"`
  if [ "$objs" != "" ]; then
    rm `find . -name "${f%.c}.o"`
  fi
done
rm bin/*/nwchem
./contrib/distro-tools/build_nwchem
