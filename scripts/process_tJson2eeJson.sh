for i in `ls test-for-ml-lammps-*.json | sed "s/.json//"`; do 
  echo $i; 
  python3 /home/sjyoo/codar/pa/perf_anom/src/trace2ee.py $i;
done
