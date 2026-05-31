#!/usr/bin/env bash

usage_string="usage: treespile.sh <wip_dir> <input_fham> -b <backend name> -c <cost_fn> -e <exhaustive> -o <optimize>"
usage() {
    echo "$usage_string"
    exit 1
}
help() {
    echo "$usage_string"
    exit 0
}

backend_name=fake_torino
optimize=0

while [[ $# -gt 0 ]]; do
    case $1 in
        -b) backend_name="$2"; shift 2;;
        -o) optimize=1; shift;;
        -h) help;;
        -*) usage;;
        *)  if [ -z "$wip_dir" ]; then
                wip_dir="$1"; shift
            fi
            if [ -z "$fham_file" ]; then
                fham_file="$1"; shift
            fi
            ;;
    esac
done

if [ -z "$wip_dir" ] || [ -z "$fham_file" ]; then
    usage
fi

fham_file_name_no_ext=$(basename "$fham_file" .fham)
# mangle the options for use in file names (e.g. path/to/fn -> path_to_fn)
base_out=$wip_dir/$fham_file_name_no_ext-${backend_name}
if [ $optimize -eq 1 ]; then
    base_out+="-optimize"
fi
log_file=${base_out}.log


exec > >(tee "$log_file") 2>&1

qsyn_commands=(
    "device fetch -f ${backend_name}"
    "fham read ${fham_file}"
)
qsyn_commands+=("fham qubitize -s ternary_tree")


if [ $optimize -eq 1 ]; then
    qsyn_commands[-1]+=" --optimize"
fi

qsyn_commands+=("qbham print")

qsyn_commands_str=$(IFS='; '; echo "${qsyn_commands[*]}")

# use qsyn to treespile the circuit
./qsyn -c "${qsyn_commands_str}"
# translate and optimize the circuit
# uv run python ./scripts/translate_and_optimize_qasm.py "${base_out}.qasm" --backend "$backend_name" -o "${base_out}.opt.qasm"
# simulate the circuit
# uv run python ./scripts/simulate_ideal_vs_noisy_fidelity.py "${base_out}.opt.qasm" --backend "$backend_name" --input-all-zero