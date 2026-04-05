#!/usr/bin/env bash
date=$(date +%Y%m%d-%H%M%S)
echo "Running tree rotation tests on $date"
wip_dir="/home/zwischen/qsyn/wip/treespile/$date"
mkdir -p "$wip_dir"

# tee the output of the treespile tests to a file
tee_file="$wip_dir/tree_rotation_tests.log"
exec > >(tee "$tee_file") 2>&1

for fham_file in benchmark/fham/*.fham; do

    for backend in fake_manila fake_oslo fake_algiers fake_torino; do
        for option in "" "-o"; do
            echo "Running tree rotation test on $fham_file with device $backend and option $option"
            time ./scripts/tree_rotation.sh "$wip_dir" "$fham_file" -b $backend $option
            if [ $? -ne 0 ]; then
                echo "error: tree rotation test on $fham_file with $option failed with exit code $?"
            fi
            echo "--------------------------------"
        done
    done
done
