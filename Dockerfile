FROM ubuntu:trusty
MAINTAINER Lukas Martinelli <me@lukasmartinelli.ch>

# install dependencies for building facebook libraries
RUN apt-get update && apt-get install -y \
    g++ \
    gdc \
    automake \
    autoconf \
    autoconf-archive \
    libtool \
    libboost-all-dev \
    libevent-dev \
    libdouble-conversion-dev \
    libgoogle-glog-dev \
    libgflags-dev \
    liblz4-dev \
    liblzma-dev \
    libsnappy-dev \
    make \
    zlib1g-dev \
    binutils-dev \
    libjemalloc-dev \
    libssl-dev \
    libiberty-dev \
    scons \
    git \
    wget \
    unzip

# build and install folly
WORKDIR /tmp/build
RUN git clone https://github.com/facebook/folly
WORKDIR folly/folly
RUN wget https://googletest.googlecode.com/files/gtest-1.6.0.zip && \
    unzip gtest-1.6.0.zip -d test
RUN autoreconf -ivf && \
    ./configure && \
    make check && \
    make install

# build and install double-conversion
WORKDIR /tmp/build
RUN git clone https://github.com/floitsch/double-conversion.git
WORKDIR double-conversion
RUN scons install

# build and install flint
WORKDIR /tmp/build
RUN git clone https://github.com/facebook/flint
WORKDIR flint
RUN wget https://googletest.googlecode.com/files/gtest-1.6.0.zip && \
    unzip gtest-1.6.0.zip -d cxx
ENV LDFLAGS -L/root/double-conversion
ENV CPPFLAGS -I/root/double-conversion/src
RUN autoreconf --install && \
    ./configure --with-boost-libdir=/usr/lib/x86_64-linux-gnu && \
    make && \
#   make check && \
    make install

# cleanup build
WORKDIR /root
RUN rm -r /tmp/build

ENV LD_LIBRARY_PATH /usr/local/lib
ENTRYPOINT ["/usr/local/bin/flint"]
