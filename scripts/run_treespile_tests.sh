#!/usr/bin/env bash
date=$(date +%Y%m%d-%H%M%S)
echo "Running treespile tests on $date"
wip_dir="/home/zwischen/qsyn/wip/treespile/$date"
mkdir -p "$wip_dir"

# tee the output of the treespile tests to a file
tee_file="$wip_dir/treespile_tests.log"
exec > >(tee "$tee_file") 2>&1

for fham_file in benchmark/fham/*.fham; do
    echo "Running treespile test on $fham_file"

    for option in "default" "default -o" "log -e" "log -e -o"; do
        time ./scripts/treespile.sh "$wip_dir" "$fham_file" -b fake_torino -c $option
        if [ $? -ne 0 ]; then
            echo "error: treespile test on $fham_file with $option failed with exit code $?"
            exit 1
        fi
        echo "--------------------------------"
    done
done
