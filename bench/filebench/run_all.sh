#!/bin/bash
{
	source ../../scripts/global.sh

	buildLineFS

	./run_bench.sh -t nic    # Run with LineFS
	./run_bench.sh -t nic -c # Run with LineFS and a cpu-intensive job.

	buildAssise

	./run_bench.sh -t hostonly    # Run with Assise
	./run_bench.sh -t hostonly -c # Run with Assise and a cpu-intensive job.

	exit
}
