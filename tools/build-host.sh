set -ex

PAR=1
while [ $# -gt 0 ]; do
  case "$1" in
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

# MAKR: - gRPC
GRPC_PATH=${PROJECT_PATH}/grpc
cd ${GRPC_PATH}

cd third_party/protobuf
./autogen.sh && ./configure && make -j$PAR
make install
ldconfig

export LDFLAGS="$LDFLAGS -lm"
cd ${GRPC_PATH}
make clean
make -j$PAR
make install
ldconfig

# MAKR: - GoogleAPIs
cd ${PROJECT_PATH}
cd googleapis/
make LANGUAGE=cpp -j$PAR
