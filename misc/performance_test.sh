#!/bin/bash

set -u

RUN_DURATION_NORMAL=600	# 10 mins run duration for each test
RUN_DURATION_QUICK=30	# 30 seconds run duration for each test

NEED_ISOLCPUS="1-3"

PARSE_ONLY=false
QUICK_TEST=false
RUN_DURATION=${RUN_DURATION_NORMAL}

print_help() {
	echo "Usage: $0 [OPTION] NAME SSH-HOST"
	echo "Run performance tests on selected target via ssh."
	echo ""
	echo "Use of ssh keys for SSH-HOST is recommended to avoid prompt during execution."
	echo "Target isolcpus must be set to ${NEED_ISOLCPUS}." 
	echo "The following binaries are needed in default path of the target:"
	echo "  load_test"
	echo "  stress-ng"
	echo ""
	echo "NAME     Give a name to the test batch. All the files will be stored in a folder with the same name."
	echo ""
	echo "SSH-HOST user@host parameter to be used for ssh connection."
	echo ""
	echo "OPTIONS:"
	echo ""
	echo "  -h | --help        Print this help."
	echo "  -q | --quick       Perform a quicker test."
	echo "  -p | --parse-only  Parse log files only without running any test on the target."
	echo "                     Raspa log files must already be inside OUTPUT-DIR."
	echo "                     When using this option the SSH-HOST parameter could be omitted."
	echo ""
}

error() {
	rm -rf ${OUTPUT_DIR} &> /dev/null
	exit 1
}

run_on_target() {
	ssh ${SSH_HOST} $@
}

# Check options
while [ $# -gt 0 ]; do
	case ${1} in
		-h | --help)
			print_help
			exit 0
			;;
		-q | --quick)
			RUN_DURATION=${RUN_DURATION_QUICK}
			;;
		-p | --parse-only)
			PARSE_ONLY=true
			;;
		*)
			break
			;;
	esac
	shift
done
NAME=$1

RUN_STRESS_NG="stress-ng --memthrash 1"
SLEEP="sleep $RUN_DURATION"
CLEAN_RASPA_LOG="rm /tmp/raspa.log"
KILL_LOAD_TEST="while pgrep -x load_test; do killall -s SIGINT load_test; sleep 1; done"
KILL_STRESS_NG="while pgrep -x stress-ng; do killall -s SIGKILL stress-ng; sleep 1; done"
INITIAL_CLEANUP="$KILL_STRESS_NG; $KILL_LOAD_TEST; sleep 1; $CLEAN_RASPA_LOG"

# Array containing the test commands
# TEST_NAME array should be updated accordingly
TEST_COMMANDS=(
"$INITIAL_CLEANUP; load_test -c3 -l -f512 -d0 & $SLEEP; $KILL_LOAD_TEST"								# cpu intensive test
"$INITIAL_CLEANUP; load_test -c3 -l -f0 -d32 -s262144 -x0 -t16 -y8192 & $SLEEP; $KILL_LOAD_TEST"					# memory intensive test, L1 thrashing
"$INITIAL_CLEANUP; load_test -c3 -l -f0 -d32 -s4194304 -x0 -t16 -y262144 & $SLEEP; $KILL_LOAD_TEST"					# memory intensive test, L2 thrashing
"$INITIAL_CLEANUP; load_test -c3 -l -f0 -d32 -s262144 -x71 -t16 -y13 & $SLEEP; $KILL_LOAD_TEST"						# memory intensive test, sparse pattern
"$INITIAL_CLEANUP; $RUN_STRESS_NG & load_test -c3 -l -f512 -d0 & $SLEEP; $KILL_LOAD_TEST; $KILL_STRESS_NG"				# cpu intensive test + stress-ng
"$INITIAL_CLEANUP; $RUN_STRESS_NG & load_test -c3 -l -f0 -d32 -s262144 -x0 -t16 -y8192 & $SLEEP; $KILL_LOAD_TEST; $KILL_STRESS_NG"	# memory intensive test, L1 thrashing + stress-ng
"$INITIAL_CLEANUP; $RUN_STRESS_NG & load_test -c3 -l -f0 -d32 -s4194304 -x0 -t16 -y262144 & $SLEEP; $KILL_LOAD_TEST; $KILL_STRESS_NG"	# memory intensive test, L2 thrashing + stress-ng
"$INITIAL_CLEANUP; $RUN_STRESS_NG & load_test -c3 -l -f0 -d32 -s262144 -x71 -t16 -y13 & $SLEEP; $KILL_LOAD_TEST; $KILL_STRESS_NG"	# memory intensive test, sparse pattern + stress-ng
)

# This array contains the names for the above tests. Avoid using spaces in the test names!
TEST_NAME=(
"cpu"
"mem_l1_thrash"
"mem_l2_thrash"
"mem_sparse"
"stress-ng+cpu"
"stress-ng+mem_l1_thrash"
"stress-ng+mem_l2_thrash"
"stress-ng+mem_sparse"
)

# Create output directory
OUTPUT_DIR=$NAME
mkdir -p $OUTPUT_DIR

# Verify target and get info
if ! $PARSE_ONLY; then

	SSH_HOST=$2

	# Check if we can ssh to the target
	run_on_target exit &> /dev/null
	if [ $? -ne 0 ]; then
		echo "Error accessing ${SSH_HOST}"
		error
	fi

	# Check if host is configured properly
	if [ ! -f run_log_parser.py ]; then
		echo "run_log_parser.py not found on current directory"
		error
	fi

	# Check target configuration
	run_on_target "which load_test" &> /dev/null
	if [ $? -ne 0 ]; then
		echo "load_test was not found in $SSH_HOST"
		error
	fi

	run_on_target "which stress-ng" &> /dev/null
	if [ $? -ne 0 ]; then
		echo "stress-ng was not found in $SSH_HOST."
		error
	fi

	# Get info from target
	run_on_target "uname -a" > ${OUTPUT_DIR}/uname.txt
	if [ $? -ne 0 ]; then
		echo "Error retrieving uname info from target"
		error
	fi

	run_on_target "cat /proc/cmdline" > ${OUTPUT_DIR}/cmdline.txt
	if [ $? -ne 0 ]; then
		echo "Error retrieving cmdline from target"
		error
	fi

	# Check isolcpu settings
	if ! grep ${NEED_ISOLCPUS} ${OUTPUT_DIR}/cmdline.txt &> /dev/null; then
		echo "Target isolcpus must be set to ${NEED_ISOLCPUS}, check target cmdline settings."
		error
	fi

	# Save test info
	info_file=${OUTPUT_DIR}/info.txt
	echo "Test info" > ${info_file}
	echo "target uname   : $(cat ${OUTPUT_DIR}/uname.txt)" >> ${info_file}
	echo "target cmdline : $(cat ${OUTPUT_DIR}/cmdline.txt)" >> ${info_file}
	echo "run duration   : ${RUN_DURATION}" >> ${info_file}

	# Save a copy of the test script used
	cp $0 ${OUTPUT_DIR}/

	# Remove temporary files
	rm ${OUTPUT_DIR}/uname.txt
	rm ${OUTPUT_DIR}/cmdline.txt
fi

# Run tests on target
index=0
for command in "${TEST_COMMANDS[@]}"; do
	testname=${TEST_NAME[$index]}
	raspa_log=${OUTPUT_DIR}/raspa_${testname}.log

	if ! $PARSE_ONLY; then
		echo "[$(date +'%D %T')] Running test $((index + 1)) of ${#TEST_COMMANDS[@]}: ${testname}"

		run_on_target "${command}" &> /dev/null
		if [ $? -ne 0 ]; then
			echo "Error executing test ${testname}"
			error
		fi
	
		scp ${SSH_HOST}:/tmp/raspa.log ${raspa_log} &> /dev/null
		if [ $? -ne 0 ]; then
			echo "Raspa log file for test ${testname} not found on target"
			error
		fi
	fi

	csv_file=${OUTPUT_DIR}/raspa_${testname}.csv
	python run_log_parser.py ${raspa_log} --csv ${csv_file} --pdf "${NAME}/${testname}.pdf" --label "${NAME}/${testname}"

	index=$((index+1))
done
