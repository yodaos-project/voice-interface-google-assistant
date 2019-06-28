set -ex

while [ $# -gt 0 ]; do
  case "$1" in
    --host)
      HOST=$2
      shift
      ;;
    --sysroot)
      SYSROOT=$2
      shift
      ;;
    --toolchain)
      TOOLCHAIN=$2
      shift
      ;;
    -s)
      STRIP_EXEC="true"
      ;;
    -j)
      PAR=$2
      shift
      ;;
    -h)
      printf "$help"
      exit
      ;;
    --*)
      echo "Illegal option $1"
      exit
      ;;
  esac
  shift $(( $# > 0 ? 1 : 0 ))
done

PROJECT_PATH=$(pwd)
STAGING_DIR=$(pwd)/staging_dir

mkdir -p $STAGING_DIR

if [ ! $TOOLCHAIN ]; then
  CC=`echo $(which gcc)`
  CXX=`echo $(which g++)`
  STRIP=`echo $(which strip)`
  if [ ! $HOST ]; then
    HOST=`echo $(uname -m)`
  fi
else
  echo "toolchain: ${TOOLCHAIN}"
  TOOLCHAIN=`echo "$(cd ${TOOLCHAIN}; pwd)"`
  if [ ! $HOST ]; then
    echo "Please define host for toolchain."
    exit 1
  fi
  CC=$TOOLCHAIN/bin/$HOST-gcc
  CXX=$TOOLCHAIN/bin/$HOST-g++
  STRIP=$TOOLCHAIN/bin/$HOST-strip
  echo "CC: $CC"
  echo "CXX: $XX"
  echo "strip: $STRIP"
fi

if [ ! $SYSROOT ]; then
  SYSROOT=/
else
  SYSROOT=`echo "$(cd ${SYSROOT}; pwd)"`
fi
echo $SYSROOT

if [ ! $PAR ]; then
  PAR=1
fi

MAKE_OPTS="ARCH=aarch64 \
  AR=aarch64-linux-gnu-ar \
  AS=aarch64-linux-gnu-as \
  NM=aarch64-linux-gnu-nm \
  CC=aarch64-linux-gnu-gcc \
  GCC=aarch64-linux-gnu-gcc \
  CXX=aarch64-linux-gnu-g++ \
  RANLIB=aarch64-linux-gnu-ranlib \
  STRIP=aarch64-linux-gnu-strip "

# MAKR: - gRPC
GRPC_PATH=${PROJECT_PATH}/grpc

cd ${GRPC_PATH}/third_party/protobuf
./autogen.sh
./configure --host=aarch64-linux-gnu \
  CC=$CC \
  CXX=$CXX \
  --with-protoc=/usr/local/bin/protoc
  --prefix=/data/protobuf/build_aarch64

make $MAKE_OPTS -j$PAR
make DESTDIR=$STAGING_DIR install
ldconfig

export CFLAGS="-Os -pipe -fPIC -fno-caller-saves -fno-plt -Wno-error -Wformat -Wl,-z,now -Wl,-z,relro -I$STAGING_DIR/usr/local/include -I$SYSROOT/usr/include -I$TOOLCHAIN/include --sysroot=$SYSROOT "
export CXXFLAGS="-std=c++11 -Os -pipe -fPIC -fno-caller-saves -Wno-error -fno-plt -Wformat -Wl,-z,now -Wl,-z,relro -I$STAGING_DIR/usr/local/include -I$SYSROOT/usr/include -I$TOOLCHAIN/include --sysroot=$SYSROOT "
export CPPFLAGS="$CXXFLAGS"
export LDFLAGS="-L$STAGING_DIR/usr/local/lib -L$TOOLCHAIN/lib "
export PATH=$SYSROOT/bin:$TOOLCHAIN/bin:$PATH


export LDFLAGS="$LDFLAGS -lm"
MAKE_OPTS="$MAKE_OPTS \
  LD=aarch64-linux-gnu-gcc \
  LDXX=aarch64-linux-gnu-g++ \
  AROPTS='-r'"
cd ${GRPC_PATH}
make $MAKE_OPTS -j$PAR \
  GRPC_CROSS_COMPILE=true \
  PROTOC=/usr/local/bin/protoc \
  PROTOC_PLUGINS_DIR=/usr/local/bin \
  PROTOBUF_CONFIG_OPTS="--host=aarch64-linux --with-protoc=/usr/local/bin/protoc"
make DESTDIR=$STAGING_DIR install
ldconfig
