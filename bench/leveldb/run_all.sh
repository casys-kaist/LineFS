#!/bin/bash
{
	source ../../../scripts/global.sh

	buildLineFS

	./run_bench_histo.sh -t linefs    # Run LineFS alone.
	./run_bench_histo.sh -t linefs -c # Run LineFS with streamcluster.

	buildAssise

	./run_bench_histo.sh -t assise    # Run Assise alone.
	./run_bench_histo.sh -t assise -c # Run Assise with streamcluster.

	exit
}
