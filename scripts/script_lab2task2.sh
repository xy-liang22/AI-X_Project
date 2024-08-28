bash -c '> plot/latency_link_width_bits.txt'

link_width_bits=8

while (( $(echo "$link_width_bits <= 512" | bc -l) ))
do
    injection_rate=.01
    while (( $(echo "$injection_rate <= 0.5" | bc -l) ))
    do
        echo "Running script with injection_rate = $injection_rate"
        ./build/NULL/gem5.opt \
        configs/example/garnet_synth_traffic.py \
        --network=garnet --num-cpus=64 --num-dirs=64 \
        --topology=Mesh_XY --mesh-rows=8 \
        --inj-vnet=0 --synthetic=neighbor \
        --sim-cycles=10000 --injectionrate=$injection_rate --link-width-bits=$link_width_bits
        grep "average_packet_latency" m5out/stats.txt | sed "s/system.ruby.network.average_packet_latency\s*/link_width_bits = $link_width_bits  injection_rate = $injection_rate  average_packet_latency = /" >> plot/latency_link_width_bits.txt
        injection_rate=$(echo "$injection_rate + 0.01" | bc)
    done
    link_width_bits=$(echo "$link_width_bits * 2" | bc)
done