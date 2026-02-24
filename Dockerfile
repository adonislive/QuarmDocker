FROM debian:12-slim

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && apt install -y --no-install-recommends build-essential libtool cmake curl debconf-utils git libluabind-dev libsodium-dev liblua5.2-0 liblua5.2-dev libmariadb-dev libssl-dev minizip make mariadb-client locales nano unzip uuid-dev iputils-ping wget libcurl4-openssl-dev gdb libyaml-cpp-dev ccache ninja-build pv mariadb-server libperl-dev libjson-perl libio-stringy-perl liblua5.1-dev libluabind-dev libboost-dev mariadb-server valgrind telnet ca-certificates

RUN git clone https://github.com/SecretsOTheP/EQMacEmu.git src

# Build Arg to circumvent client file checksums on server.
# Required for a Quarm client to connect.
ARG DISABLE_CHECKSUM
RUN if [ "$DISABLE_CHECKSUM" = "true" ]; then \
        sed -i '/bool Client::HandleChecksumPacket(const EQApplicationPacket \*app)/,/^{/!b;/^{/a\    return true;' src/world/client.cpp; \
    fi

# Comment out GetWorldPreferredStatus (mirrors TAKP — doesn't work atm)
RUN sed -i '/GetWorldPreferredStatus/,+2 s|^|//|' src/loginserver/server_manager.cpp

RUN mkdir src/build && \
    cd src/build && \
    cmake -DEQEMU_BUILD_LOGIN=ON -DEQEMU_BUILD_TESTS=ON -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -G Ninja ..
RUN cd src/build && \
    cmake --build . --config Release --target all -- -j1

RUN git clone https://github.com/SecretsOTheP/quests.git quests

RUN git clone https://github.com/EQMacEmu/Maps.git Maps

RUN mkdir data && \
    cd data && \
    mv /src/utils/sql/database_full/*.tar.gz .

COPY init.sh .
RUN chmod +x init.sh && ./init.sh

COPY entrypoint.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/entrypoint.sh
ENTRYPOINT [ "entrypoint.sh" ]
