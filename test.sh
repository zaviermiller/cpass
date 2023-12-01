#!/bin/bash

INPUT_DIR="./inputs"
NUM_CORR=0
NUM_TOTAL=0
CORRECT_ERR="correct.err"
TEST_ERR="test.err"

for FILE in $INPUT_DIR/*
do
  ((NUM_TOTAL++))
  testname=`basename $FILE`
  testname=${testname%.*}
  ./run_opt.sh TEST --opt $testname 2> $TEST_ERR
  ./run_opt.sh REF --opt $testname 2> $CORRECT_ERR
  CORRECT_OUTPUT="llvm_ir/ref/$testname.ll"
  TEST_OUTPUT="llvm_ir/test/$testname.ll"
  DIFF=`diff $CORRECT_OUTPUT $TEST_OUTPUT`
  DIFF_ERR=`diff $CORRECT_ERR $TEST_ERR`
  if [ "$DIFF" != "" ]; then
    echo "Output for $testname differs, run diff -y $CORRECT_OUTPUT $TEST_OUTPUT to see how they differ"
    echo "$NUM_CORR/$NUM_TOTAL correct"
    # exit 1
  elif [ "$DIFF_ERR" != "" ]; then
    echo "Error for $testname differs, run diff -y $CORRECT_ERR $TEST_ERR to see how they differ"
    echo "$NUM_CORR/$NUM_TOTAL correct"
    exit 1
  else
    echo "$testname is correct"
    ((NUM_CORR++))
  fi
done

echo "$NUM_CORR/$NUM_TOTAL correct"
echo "cleaning up..."
rm $CORRECT_ERR $TEST_ERR
