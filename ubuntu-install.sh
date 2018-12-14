#!/bin/bash -x

apt -y install build-essential libboost-all-dev libsqlcipher-dev git libcurl4-openssl-dev qt5-defaults cmake libsqlite3-dev libssl-dev

sed -i "s/Cflags: -I\${includedir}\/sqlcipher/Cflags: -I\${includedir}\/sqlcipher -DSQLITE_HAS_CODEC -DSQLITE_TEMP_STORE=2/" 

cd
git clone https://github.com/HowardHinnant/date
cd date
mkdir build
cd build
cmake ..
make -j2 install

cd
git clone https://github.com/rbock/sqlpp11
cd sqlpp11
mkdir build
cd build
cmake ..
make -j2 install

cd
git clone https://github.com/rbock/sqlpp11-connector-sqlite3
cd sqlpp11-connector-sqlite3
mkdir build
cd build
cmake ..
make -j2 install

cd
git clone https://github.com/nlohmann/json
cd json
mkdir build
cd build
cmake ..
make -j2 install


cd
git clone https://github.com/jarro2783/cxxopts
cd cxxopts
mkdir build
cd build
cmake ..
make -j2 install

cd
git clone https://github.com/zeromq/libzmq
cd libzmq
./autogen.sh
./configure
make -j2 install

cd
git clone https://github.com/zeromq/czmq
cd czmq
./autogen.sh
./configure
make -j2 install

cd
git clone https://github.com/libbitcoin/secp256k1
cd secp256k1
./autogen.sh
./configure --enable-module-recovery
make -j2 install

cd
git clone https://github.com/narodnik/libbitcoin
cd libbitcoin
./autogen.sh
./configure
make -j2 install

cd
git clone https://github.com/libbitcoin/libbitcoin-database
cd libbitcoin-database
./autogen.sh
./configure
make -j2 install

cd
git clone https://github.com/narodnik/dtek/
cd dtek
qmake darktech.pro
make -j2

