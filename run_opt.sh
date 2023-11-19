#! /usr/bin/env bash

EXECUTABLE=""
IRDIR=""

if [[ $1 == "REF" ]]
then
    EXECUTABLE="ref_builds/lib_ref_copy_prop.so"
    IRDIR="ref"
elif [[ $1 == "TEST" ]]
then
    EXECUTABLE="build/copy_prop/libcopy_prop.so"
    IRDIR="test"
else
    echo "Need REF or TEST for first arg"
    exit
fi

if [[ $2 == "--opt" ]]
then
    clang -O0 -S -emit-llvm ./inputs/"$3".c -o ./llvm_ir/unoptimized/"$3".ll
    opt -O0 -enable-new-pm=0 -load "$EXECUTABLE" -copy_prop -verbose < ./llvm_ir/unoptimized/"$3".ll | llvm-dis -o ./llvm_ir/"$IRDIR"/"$3".ll
    # turn off verbose output
    # opt -O0 -enable-new-pm=0 -load "$EXECUTABLE" -copy_prop < ./llvm_ir/unoptimized/"$3".ll | llvm-dis -o ./llvm_ir/"$IRDIR"/"$3".ll

elif [[ $2 == "--compile" ]]
then
    llc -filetype=obj ./llvm_ir/"$IRDIR"/"$3".ll -o ./objs/"$3".o
    clang ./objs/"$3".o -o "$3"
else
    echo "usage: ./run_opt.sh [REF TEST] --[flag] input"
    echo "flags: opt, compile"
fi
