./build/NULL/gem5.opt configs/example/garnet_synth_traffic.py --network=garnet --num-cpus=36 --num-dirs=64 --topology=Mesh_XY --mesh-rows=6 --routing-algorithm=1  --inj-vnet=0 --synthetic=uniform_random --garnet-deadlock-threshold=50000 --sim-cycles=100000000 --injectionrate=.9
# ./build/NULL/gem5.opt configs/example/garnet_synth_traffic.py --network=garnet --num-cpus=64 --num-dirs=64 --topology=Mesh_XY --mesh-rows=8 --routing-algorithm=2  --inj-vnet=0 --synthetic=uniform_random --garnet-deadlock-threshold=50000 --sim-cycles=100000000 --injectionrate=.1
bash -c '> network_stats.txt'
grep "packets_injected::total" m5out/stats.txt | sed 's/system.ruby.network.packets_injected::total\s*/packets_injected = /' >> network_stats.txt
grep "packets_received::total" m5out/stats.txt | sed 's/system.ruby.network.packets_received::total\s*/packets_received = /' >> network_stats.txt
echo >> network_stats.txt
grep "average_packet_queueing_latency" m5out/stats.txt | sed 's/system.ruby.network.average_packet_queueing_latency\s*/average_packet_queueing_latency = /' >> network_stats.txt
grep "average_packet_network_latency" m5out/stats.txt | sed 's/system.ruby.network.average_packet_network_latency\s*/average_packet_network_latency = /' >> network_stats.txt
grep "average_packet_latency" m5out/stats.txt | sed 's/system.ruby.network.average_packet_latency\s*/average_packet_latency = /' >> network_stats.txt
echo >> network_stats.txt
echo "average_packet_queueing_latency = $(echo "scale=6; $(grep "average_packet_queueing_latency" m5out/stats.txt | awk '{ print $2 }') / 1000" | bc)                       (Cycle/Packet)" >> network_stats.txt
echo "average_packet_network_latency = $(echo "scale=6; $(grep "average_packet_network_latency" m5out/stats.txt | awk '{ print $2 }') / 1000" | bc)                       (Cycle/Packet)" >> network_stats.txt
echo "average_packet_latency = $(echo "scale=6; $(grep "average_packet_latency" m5out/stats.txt | awk '{ print $2 }') / 1000" | bc)                       (Cycle/Packet)" >> network_stats.txt
grep "average_hops" m5out/stats.txt | sed 's/system.ruby.network.average_hops\s*/average_hops = /' >> network_stats.txt
total_packets_received=$(grep "packets_received::total" m5out/stats.txt | awk '{ print $2 }')
num_cpus=$(echo "$(grep '"path": "system.cpu' m5out/config.json | grep -oE '[0-9]{2}' | sort -nr | head -n 1) + 1" | bc)
sim_cycles=$(echo "$(grep -o '"sim_cycles": [0-9]*' m5out/config.json | sed 's/"sim_cycles": //' | head -n 1) / 1000" | bc)
reception_rate=$(echo "scale=6; $total_packets_received / $num_cpus / $sim_cycles" | bc)
echo -e "reception_rate = $reception_rate                       (Packets/Node/Cycle)" >> network_stats.txt
