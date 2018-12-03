g++ -Iinclude/ main.cpp src/wallet.cpp src/blockchain.cpp $(pkg-config --cflags --libs libbitcoin-database libczmq) -lsqlpp11-connector-sqlite3 -lsqlcipher -DSQLPP_USE_SQLCIPHER -DSQLITE_HAS_CODEC -DSQLITE_TEMP_STORE=2 -lcrypto -o darktech -g
#g++ -Iinclude/ write.cpp src/blockchain.cpp $(pkg-config --cflags --libs libbitcoin-database) -o write -g
