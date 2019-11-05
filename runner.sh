#/bin/bash
# By default starts the servers and run the integration tests.
# Use 'server_only' to start the servers only without running the integration
# tests.
#   ./runner.sh server_only
# User 'benchmark' to start the servers and run the integration tests.
#   ./runner.sh benchmark

PID_LIST=
MODE=$1

if [[ ! -f build/flowmq_integration_test ]]; then
  echo "Make sure ./build/flowmq_integration_test exists."
  exit 1
fi

CUR_TIME=$(date +%s)
TEST_DIR=temp_test_$CUR_TIME
echo "Creating a new test dir $TEST_DIR"
mkdir $TEST_DIR

cd $TEST_DIR

for node in {0,1,2,3,4}; do {
  echo "Starting node $node"
  ../build/flowmq_node ../build/node.config $node & PID_LIST+=" $!"
} done


if [[ $MODE = server_only ]]; then
  echo "Starting server only."
elif [[ $MODE = benchmark ]]; then
  echo "All nodes started, waiting for 10 seconds to run benchmark"
  sleep 10s
  ../build/flowmq_benchmark_test localhost 9001 0 & PID_LIST+=" $!"
else
  echo "All nodes started, waiting for 10 seconds to run tests"
  sleep 10s
  ../build/flowmq_integration_test localhost 9001 0 & PID_LIST+=" $!"
fi

echo $PID_LIST
trap "kill $PID_LIST" SIGINT

echo "Use Ctrl-C to kill all processes."
wait $PID_LIST


