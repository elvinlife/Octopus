# Octopus
![Status](https://img.shields.io/badge/Version-Experimental-green.svg)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Octopus is the first real-time 2D&3D video streaming system that leverages in-network content adaptation to achieve both high throughput and low latency. It's built upon UDT protocol. For more details, please check our paper published in SEC'2023.

## Build and deploy the Octopus endhosts
```
cd src && make
cd ../app && make
# make a soft link in the dynamic load library dir, such as /usr/lib 
sudo ln -s /usr/lib/libudt.so ./lib/libudt.so
```

## Build the Octopus router 
Since it's easier and more stable to run mahimahi compared with srsRAN, we only provide the instructions to reproduce the experiment results of Octopus with mahimahi(an emulated cellular router). For a more realistic case using srsRAN, please email the author to get the code and instructions.

Check https://github.com/elvinlife/octopus-mahimahi to build and install the mahimahi with Octopus router logic

## Experiments 

Check `./scripts_mahimahi` for experiments reproducibility
