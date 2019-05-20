### Build & run

System requirements:
- ubuntu 18.04 LTS 64bit;
- 50-100 GB SSD;
- 30 GB of RAM for full transaction history or 15 GB for partial;

First of all you need to install gcc-7/g++-7 compilers and dependencies:

```bash
user@user:~$ sudo apt-get install gcc-7 g++-7 make cmake zlib1g-dev libbz2-dev libdb++-dev libdb-dev libssl-dev openssl libreadline-dev autoconf libtool libncurses5-dev automake python-dev
```

Then build these libraries (using compilers above):

- boost 1.63.0:

```bash
user@user:~$ wget 'https://sourceforge.net/projects/boost/files/boost/1.63.0/boost_1_63_0.tar.gz'
user@user:~$ cd <boost-root-directory-after-unpacking>
user@user:~$ ./bootstrap.sh
user@user:~$ ./b2 toolset=gcc-7 variant=release -j6 && sudo ldconfig
```
- openssl 1.1.0j:

```bash
user@user:~$ wget 'https://www.openssl.org/source/openssl-1.1.0j.tar.gz'
user@user:~$ cd <openssl-directory-after-unpacking>
user@user:~$ ./config
user@user:~$ make CC=gcc-7
user@user:~$ sudo ldconfig
```

If your dependencies are already built then (with replacing the paths with yours, of course):

```bash
user@user:~$ git clone git@github.com:EDCBlockchain/blockchain.git
user@user:~$ cd blockchain
user@user:~$ cmake -DCMAKE_C_COMPILER="/usr/bin/gcc-7" -DCMAKE_CXX_COMPILER="/usr/bin/g++-7" -DBOOST_ROOT="/home/devuser/work/boost_1_63_0" -DOPENSSL_ROOT_DIR="/home/devuser/work/openssl-1.1.0j" -DOPENSSL_INCLUDE_DIR="/home/devuser/work/openssl-1.1.0j/include" -DOPENSSL_LIBRARIES="/home/devuser/work/openssl-1.1.0j" -DCMAKE_BUILD_TYPE=RelWithDebInfo
user@user:~$ make -j6
```

It is very comfortably to use 'screen' or 'tmux' utility for starting witness_node and cli_wallet in separate system windows.

### witness_node
<b>Important info</b>

If you need to stop witness_node do that via 'ctrl+c' combination (or sending SIGINT to the process) - only ONCE(!) and wait until it is finished. Otherwise, on the next starting process you will wait for several hours (database reindexing).

Running:

```bash
user@user:~/blockchain$ ./programs/witness_node/witness_node --data-dir=data --rpc-endpoint=127.0.0.1:5909 --genesis-json=genesis.json
```

A list of witness_node commands is available here:

   https://github.com/EDCBlockchain/blockchain/blob/master/libraries/app/include/graphene/app/database_api.hpp#L659
   
   and here:
   
   https://github.com/EDCBlockchain/blockchain/blob/master/libraries/app/include/graphene/app/api.hpp#L362

### cli_wallet

Then, in a separate terminal window, start the command-line wallet `cli_wallet`:
```bash
user@user:~/blockchain$ ./programs/cli_wallet/cli_wallet --server-rpc-endpoint=ws://127.0.0.1:5909 --rpc-endpoint=127.0.0.1:8085 --rpc-http-endpoint=127.0.0.1:8086 --chain-id=979b29912e5546dbf47604692aafc94519f486c56221a5705f0c7f5f294df126 --wallet-file=wallet.json --no-backups
```

A list of CLI wallet commands is available here:

   https://github.com/EDCBlockchain/blockchain/blob/master/libraries/wallet/include/graphene/wallet/wallet.hpp#L1645
   
