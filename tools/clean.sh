set -x

PROJECT_PATH=$(pwd)

# MAKR: - gRPC
GRPC_PATH=${PROJECT_PATH}/grpc
cd ${GRPC_PATH}

cd third_party/protobuf
make clean

cd ${GRPC_PATH}
make clean

# MAKR: - GoogleAPIs
cd ${PROJECT_PATH}
cd googleapis/
make clean
