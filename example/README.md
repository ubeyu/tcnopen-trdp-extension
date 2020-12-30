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
