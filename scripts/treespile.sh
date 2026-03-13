#!/usr/bin/env bash

# usage: treespile.sh <input_qasm> <backend name> <cost_fn>

wip_dir=/home/zwischen/qsyn/wip/treespile/

mkdir -p $wip_dir

qasm_file=$1
backend_name=$2
cost_fn=$3
qasm_file_name_no_ext=$(basename $qasm_file .qasm)
# mangle cost_fn for use in file names (e.g. path/to/fn -> path_to_fn)
cost_mangled=${cost_fn//\//_}

base_out=$wip_dir/$qasm_file_name_no_ext-$cost_mangled
log_file=${base_out}.log

exec > >(tee "$log_file") 2>&1

# use qsyn to treespile the circuit
./qsyn scripts/treespile.qsyn $qasm_file ${base_out}.qasm $backend_name $cost_fn
# nativize and optimize the circuit
uv run python ./scripts/nativize_and_optimize_qasm.py ${base_out}.qasm --backend $backend_name -o ${base_out}.opt.qasm
# simulate the circuit
uv run python ./scripts/simulate_ideal_vs_noisy_fidelity.py ${base_out}.opt.qasm --backend $backend_name --input-all-zero