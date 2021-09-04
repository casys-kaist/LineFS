#!/bin/bash
{
	source ../../scripts/global.sh

	TPUT_BENCH_SH="run_iobench_tput.sh"
	MEMCPY_OUT_DIR="/tmp/out_memcpy"
	CPU_MEMCPY="cpu_memcpy"
	DMA_POLLING="dma_polling"
	DMA_POLLING_BATCHING="dma_polling_batching"
	DMA_INTERRUPT_BATCHING="dma_interrupt_batching"
	NO_COPY="no_copy"

	killStreamcluster() {
		echo "Killing streamcluster."
		sudo scripts/parsec_ctl.sh -k &>/dev/null
	}

	runInterferenceExp() {
		memcpy_type="$1"
		out_file="$MEMCPY_OUT_DIR/${memcpy_type}/output.txt"
		config_dir="$MEMCPY_OUT_DIR/${memcpy_type}"
		mkdir -p "$config_dir"
		dumpConfigs "$config_dir"

		echo "******************** RUN INTERFERENCE EXP *************************"
		echo "Memcopy type     	: $memcpy_type"
		echo "Output file       : $out_file"
		echo "Dump configs into : $config_dir"
		echo "*******************************************************************"

		sudo scripts/${TPUT_BENCH_SH} -t nic -a | tee "$out_file"
	}

	printInterferenceExpResult() {
		memcpy_type="$1"
		out_file="$MEMCPY_OUT_DIR/${memcpy_type}/output.txt"

		echo "$memcpy_type"
		grep -A1 "Primary" "$out_file"
	}

	setIOATMemcpy() {
		val=$1
		echo "Set IOAT Memcpy config to $val."
		(
			cd "$PROJ_DIR" || exit
			if [ "$val" = 0 ]; then
				sed -i 's/export IOAT_MEMCPY_OFFLOAD=1/export IOAT_MEMCPY_OFFLOAD=0/g' mlfs_config.sh
			else
				sed -i 's/export IOAT_MEMCPY_OFFLOAD=0/export IOAT_MEMCPY_OFFLOAD=1/g' mlfs_config.sh
			fi
		)
	}

	setMemcpyBatching() {
		val=$1
		echo "Set Memcpy Batching config to $val."
		(
			cd "$PROJ_DIR" || exit
			if [ "$val" = 0 ]; then
				sed -i 's/WITH_SNIC_COMMON_FLAGS += -DBATCH_MEMCPY_LIST/# WITH_SNIC_COMMON_FLAGS += -DBATCH_MEMCPY_LIST/g' kernfs/Makefile
				sed -i 's/WITH_SNIC_FLAGS += -DBATCH_MEMCPY_LIST/# WITH_SNIC_FLAGS += -DBATCH_MEMCPY_LIST/g' libfs/Makefile
			else
				sed -i 's/# WITH_SNIC_COMMON_FLAGS += -DBATCH_MEMCPY_LIST/WITH_SNIC_COMMON_FLAGS += -DBATCH_MEMCPY_LIST/g' kernfs/Makefile
				sed -i 's/#WITH_SNIC_COMMON_FLAGS += -DBATCH_MEMCPY_LIST/WITH_SNIC_COMMON_FLAGS += -DBATCH_MEMCPY_LIST/g' kernfs/Makefile
				sed -i 's/# WITH_SNIC_FLAGS += -DBATCH_MEMCPY_LIST/WITH_SNIC_FLAGS += -DBATCH_MEMCPY_LIST/g' libfs/Makefile
				sed -i 's/#WITH_SNIC_FLAGS += -DBATCH_MEMCPY_LIST/WITH_SNIC_FLAGS += -DBATCH_MEMCPY_LIST/g' libfs/Makefile
			fi
		)
	}

	setIOATInterruptConfig() {
		val=$1
		echo "Set IOAT Interrupt Kernel Module config to $val."
		(
			cd "$PROJ_DIR" || exit
			if [ "$val" = 0 ]; then
				sed -i 's/WITH_SNIC_HOST_FLAGS += -DIOAT_INTERRUPT_KERNEL_MODULE/# WITH_SNIC_HOST_FLAGS += -DIOAT_INTERRUPT_KERNEL_MODULE/g' kernfs/Makefile
			else
				sed -i 's/# WITH_SNIC_HOST_FLAGS += -DIOAT_INTERRUPT_KERNEL_MODULE/WITH_SNIC_HOST_FLAGS += -DIOAT_INTERRUPT_KERNEL_MODULE/g' kernfs/Makefile
				sed -i 's/#WITH_SNIC_HOST_FLAGS += -DIOAT_INTERRUPT_KERNEL_MODULE/WITH_SNIC_HOST_FLAGS += -DIOAT_INTERRUPT_KERNEL_MODULE/g' kernfs/Makefile
			fi
		)
	}

	setNoCopyConfig() {
		val=$1
		echo "Set No Memcopy to $val."
		(
			cd "$PROJ_DIR" || exit
			if [ "$val" = 0 ]; then
				sed -i 's/DBG_FLAGS += -DDIGEST_MEMCPY_NO_COPY/# DBG_FLAGS += -DDIGEST_MEMCPY_NO_COPY/g' kernfs/Makefile
			else
				sed -i 's/# DBG_FLAGS += -DDIGEST_MEMCPY_NO_COPY/DBG_FLAGS += -DDIGEST_MEMCPY_NO_COPY/g' kernfs/Makefile
				sed -i 's/#DBG_FLAGS += -DDIGEST_MEMCPY_NO_COPY/DBG_FLAGS += -DDIGEST_MEMCPY_NO_COPY/g' kernfs/Makefile
			fi
		)
	}

	insIOATKernelModule() {
		(
			cd $PROJ_DIR/kernfs/lib/ioat-dma-kernel-module || exit
			sudo insmod ioat-dma.ko
		)
		$SSH_HOST_2 "(cd ${PROJ_DIR}/kernfs/lib/ioat-dma-kernel-module || exit; sudo insmod ioat-dma.ko &> /dev/pts/${HOST_2_TTY})"
		$SSH_HOST_3 "(cd ${PROJ_DIR}/kernfs/lib/ioat-dma-kernel-module || exit; sudo insmod ioat-dma.ko &> /dev/pts/${HOST_3_TTY})"
	}

	rmIOATKernelModule() {
		sudo rmmod ioat-dma
		$SSH_HOST_2 "sudo rmmod ioat_dma &> /dev/pts/${HOST_2_TTY}"
		$SSH_HOST_3 "sudo rmmod ioat_dma &> /dev/pts/${HOST_3_TTY}"
	}

	setupSPDKIOAT() {
		sudo "${PROJ_DIR}/kernfs/lib/spdk/scripts/setup.sh"
		$SSH_HOST_2 "sudo ${PROJ_DIR}/kernfs/lib/spdk/scripts/setup.sh &> /dev/pts/${HOST_2_TTY}"
		$SSH_HOST_3 "sudo ${PROJ_DIR}/kernfs/lib/spdk/scripts/setup.sh &> /dev/pts/${HOST_3_TTY}"
	}

	resetSPDKIOAT() {
		sudo "${PROJ_DIR}/kernfs/lib/spdk/scripts/setup.sh" reset
		$SSH_HOST_2 "sudo ${PROJ_DIR}/kernfs/lib/spdk/scripts/setup.sh reset &> /dev/pts/${HOST_2_TTY}"
		$SSH_HOST_3 "sudo ${PROJ_DIR}/kernfs/lib/spdk/scripts/setup.sh reset &> /dev/pts/${HOST_3_TTY}"
	}

	setInterruptIOATMemcpy() {
		val=$1

		if [ "$val" = 0 ]; then
			## Restore
			rmIOATKernelModule
			setupSPDKIOAT
			setIOATInterruptConfig 0
		else
			resetSPDKIOAT
			insIOATKernelModule
			setIOATInterruptConfig 1
		fi
	}

	# Main.
	if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then # script is executed directly.

		# Kill all running processes.
		sudo pkill -9 iobench
		killStreamcluster
		rm -rf $MEMCPY_OUT_DIR
		mkdir -p $MEMCPY_OUT_DIR

		## To make sure that Async Replication is turned off.
		setAsyncReplicationOff

		## Run only 1 libfs process case.
		sed -i '/^NPROCS=/c\NPROCS="1"' scripts/$TPUT_BENCH_SH

		## CPU MEMCPY
		buildLineFS
		setIOATMemcpy 0 # Set run-time config.
		runInterferenceExp $CPU_MEMCPY
		setIOATMemcpy 1 # Restore run-time config.

		## DMA POLLING
		setIOATMemcpy 1 # Set run-time config.
		runInterferenceExp $DMA_POLLING
		
		## DMA POLLING with BATCHING
		setMemcpyBatching 1
		buildLineFS
		runInterferenceExp $DMA_POLLING_BATCHING

		## DMA INTERRUPT with BATCHING
		setInterruptIOATMemcpy 1
		buildLineFS
		runInterferenceExp $DMA_INTERRUPT_BATCHING
		setInterruptIOATMemcpy 0 # Restore to SPDK driver.
		
		## NO COPY
		setNoCopyConfig 1
		buildLineFS
		runInterferenceExp $NO_COPY
		setNoCopyConfig 0 # Restore

		## Restore modified line.
		sed -i '/^NPROCS=/c\NPROCS="1 2 4 8"' scripts/$TPUT_BENCH_SH

		## Print results.
		printInterferenceExpResult $CPU_MEMCPY
		printInterferenceExpResult $DMA_POLLING
		printInterferenceExpResult $DMA_POLLING_BATCHING
		printInterferenceExpResult $DMA_INTERRUPT_BATCHING
		printInterferenceExpResult $NO_COPY
	fi

	exit
}
