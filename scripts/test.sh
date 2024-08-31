bash -c '> plot/mesh_latency.txt'
bash -c '> plot/mesh_reception_rate.txt'

injection_rate=.1

while (( $(echo "$injection_rate <= 1" | bc -l) ))
do
  echo "Running script with injection_rate = $injection_rate"
  # ./build/NULL/gem5.opt configs/example/garnet_synth_traffic.py --network=garnet --num-cpus=36 --num-dest=72 --num-dirs=128 --topology=Dragonfly --routers-per-group=4 --global-channels-per-router=2 --routing-algorithm=5 --inj-vnet=0 --synthetic=Dragonfly_WC --garnet-deadlock-threshold=50000 --sim-cycles=100000000 --injectionrate=$injection_rate
  ./build/NULL/gem5.opt configs/example/garnet_synth_traffic.py --network=garnet --num-cpus=64 --num-dirs=64 --topology=Mesh_westfirst --mesh-rows=8 --routing-algorithm=3  --inj-vnet=0 --synthetic=uniform_random --garnet-deadlock-threshold=50000 --sim-cycles=100000000 --injectionrate=$injection_rate
  echo "injection_rate = $injection_rate  average_packet_latency = $(echo "scale=6; $(grep "average_packet_latency" m5out/stats.txt | awk '{ print $2 }') / 1000" | bc)  (Cycle/Packet)" >> plot/mesh_latency.txt
  total_packets_received=$(grep "packets_received::total" m5out/stats.txt | awk '{ print $2 }')
  num_cpus=$(echo "$(grep '"path": "system.cpu' m5out/config.json | grep -oE '[0-9]{2}' | sort -nr | head -n 1) + 1" | bc)
  sim_cycles=$(echo "$(grep -o '"sim_cycles": [0-9]*' m5out/config.json | grep -oE '[0-9]*' | head -n 1) / 1000" | bc)
  reception_rate=$(echo "scale=6; $total_packets_received / $num_cpus / $sim_cycles" | bc)
  echo -e "injection_rate = $injection_rate  reception_rate = $reception_rate  (Packets/Node/Cycle)" >> plot/mesh_reception_rate.txt
  injection_rate=$(echo "$injection_rate + 0.1" | bc)
done
