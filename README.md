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

### Test the result

```bash
bash scripts/experiment.sh
```
