main: main.C
	tau_cxx.sh -DCPT_IDX=1 main.C -o main1 -g
	tau_cxx.sh -DCPT_IDX=2 main.C -o main2 -g
main_notau: main.C
	mpic++ -DCPT_IDX=1 main.C -o main1 -g
	mpic++ -DCPT_IDX=2 main.C -o main2 -g
clean:
	rm main1 main2
