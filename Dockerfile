FROM enfipy/ubuntu-boost:latest

WORKDIR /app

ARG BOOST_ROOT=$HOME/opt/boost_1_58_0

ADD . /app

RUN git submodule update --init --recursive  && \
    cmake -DBOOST_ROOT="${BOOST_ROOT}" -DCMAKE_BUILD_TYPE=Debug . && \
    make -j$(nproc)

ENV NODE_TO_RUN witness_node
ENV ARGUMENTS --data-dir=data --rpc-endpoint=0.0.0.0:5929 --genesis-json=genesis.json

CMD ./programs/${NODE_TO_RUN}/${NODE_TO_RUN} ${ARGUMENTS}
