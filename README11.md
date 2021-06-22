编译：
make LINUX_config
make


运行：
cd bld/output/linux-rel/

./sendHello -o 127.0.0.1 -t 127.0.0.1
./receiveHello -o 127.0.0.1

./sendTRDPMessage -o 127.0.0.1 -t 127.0.0.1
./sendTRDPMessage -o 127.0.0.1 -t 127.0.0.1 -d "1111111111111111"
./receiveTRDPMessage -o 127.0.0.1


./trdp-pd-test 127.0.0.1 127.0.0.1 239.2.24.1 log.txt
./trdpMulticast 127.0.0.1 127.0.0.1 239.2.24.1 log.txt
./trdpMulticast 127.0.0.1 127.0.0.1 239.1.1.100 log.txt


