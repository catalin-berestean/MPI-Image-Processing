build:
	mpicc tema3.c -o tema3 -lm
run:
	mpirun -n 4 ./tema3 input/PNM/landscape.pnm out.pnm blur smooth sharpen emboss mean blur smooth sharpen emboss mean
clean:
	rm tema3