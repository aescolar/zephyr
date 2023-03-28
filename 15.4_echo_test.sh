#!/usr/bin/env bash

# Build the applications as
# echo client: 
#   mkdir build ; cd build ;  
#   cmake ../samples/net/sockets/echo_client/ -DBOARD=nrf52_bsim -GNinja -DOVERLAY_CONFIG="overlay-ot.conf"
#   ninja
# echo server: 
#   mkdir build2 ; cd build2 ;  
#   cmake ../samples/net/sockets/echo_server/ -DBOARD=nrf52_bsim -GNinja -DOVERLAY_CONFIG="overlay-ot.conf"
#   ninja
# Then you can run them together with this silly script:

cp build/zephyr/zephyr.exe ${BSIM_OUT_PATH}/bin/echo_client.exe
cp build2/zephyr/zephyr.exe ${BSIM_OUT_PATH}/bin/echo_server.exe

cd ${BSIM_OUT_PATH}/bin

./echo_client.exe -s=Hola -d=0 -RealEncryption=1 & #-start_offset=10e6 
./echo_server.exe -s=Hola -d=1 -RealEncryption=1 &
./bs_2G4_phy_v1 -s=Hola -D=2 -sim_length=35e6 -v=2 -argschannel -at=40 

# You can get a pcap trace of the radio activity with: 
# ${BSIM_COMPONENTS_PATH}/ext_2G4_phy_v1/dump_post_process/csv2pcap_15.4.py ${BSIM_OUT_PATH}/results/Hola/d_2G4_0*.Tx.csv -o bsim_trace.pcap
