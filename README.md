# AI_plus_X_Project

### Dependencies

```bash
sudo apt install build-essential git m4 scons zlib1g zlib1g-dev \
    libprotobuf-dev protobuf-compiler libprotoc-dev libgoogle-perftools-dev \
    python3-dev libboost-all-dev pkg-config
pip install -r requirements.txt
```

### Building

```bash
scons build/NULL/gem5.opt PROTOCOL=Garnet_standalone -j $(nproc)
```

### Run

```bash
./build/NULL/gem5.opt configs/example/garnet_synth_traffic.py --network=garnet --num-cpus=36 --num-dest=72 --num-dirs=128 --topology=Dragonfly --routers-per-group=4 --global-channels-per-router=2 --routing-algorithm=4 --inj-vnet=0 --synthetic=Dragonfly_WC --garnet-deadlock-threshold=50000 --sim-cycles=100000000 --injectionrate=0.1
```

### Get Network Statistics

```bash
bash scripts/get_stats.sh
```
Then you can see statistics results in `./network_stats.txt`.

### Test Results

```bash
# To get statistics in ./plot/dragonfly_latency.txt and ./plot/dragonfly_reception_rate.txt
bash scripts/test.sh
python3 plot/plot_latency.py
```
