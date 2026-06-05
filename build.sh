#!/bin/bash
set -e

# GET Config
PROJECT_ROOT=$(pwd)
SERVER_CONFIG_FILE="${PROJECT_ROOT}/config/server_config.ini"

if [ ! -f "$SERVER_CONFIG_FILE" ]; then
    echo "❌ Config file '$SERVER_CONFIG_FILE' not found!"
    exit 1
fi

while IFS='=' read -r key value; do
    value="${value%$'\r'}"
    key="${key%$'\r'}"
    
    if [[ -z "$key" || "$key" =~ ^[#\;] || "$key" =~ ^\[.*\]$ ]]; then
        continue
    fi
    
    declare "$key"="$value"
done < "config/server_config.ini"

if [ "$1" == "clean" ]; then
    echo "🧹 Cleaning build directory..."
    rm -rf build
    rm -rf client
    echo "✨ Clean complete!"
    exit 0
fi

echo "==============================================="
echo "🔍 Checking Environment on Raspberry Pi4..."
echo "==============================================="

WPI_CHECK=$(ssh ${RPI_USER}@${RPI_IP} "which gpio || echo 'NOT_FOUND'")

if [ "$WPI_CHECK" == "NOT_FOUND" ]; then
    echo "⚠️ WARNING: wiringPi is NOT installed on your Raspberry Pi!"
    echo "빌드 및 파일 전송은 완료되지만, 서버가 정상 동작하지 않을 수 있습니다."
    echo "--------------------------------------------------------"
    echo "📋 [조치 방법]"
    echo "라즈베리 파이 OS 버전에 맞게 최신 wiringPi를 직접 설치해야 합니다."
    echo "wiringPi GitHub: https://github.com/WiringPi/WiringPi"
    echo "--------------------------------------------------------"
    echo ""
fi


echo "Building VEDA_Linux_Project..."
mkdir -p "${PROJECT_ROOT}/build/build_server"

echo "==============================================="
echo "🚀 [Client] Build for Ubuntu (Host)..."
echo "==============================================="
mkdir -p "${PROJECT_ROOT}/build/build_client" && cd "${PROJECT_ROOT}/build/build_client"
cmake -DCMAKE_C_COMPILER=gcc -DRUNNING_FROM_SCRIPT=1 "${PROJECT_ROOT}"
cmake --build . --target client
cd "${PROJECT_ROOT}"

# TODO: 테스트로 바꾸기
# if [ "$1" == "linux" ]; then
#     echo "==============================================="
#     echo "🚀 [Server] Build for Ubuntu..."
#     echo "==============================================="
#     mkdir -p build_server && cd build_server
#     cmake -DCMAKE_C_COMPILER=gcc ..
#     cmake --build . --target server
#     cd ..

#     echo "==============================================="
#     echo "✨ Build & Transfer Complete!"
#     echo "==============================================="
#     exit 0
# fi

echo "==============================================="
echo "🚀 [Server] Build for RaspberryPi4..."
echo "==============================================="
cd "${PROJECT_ROOT}/build/build_server"
cmake -DCMAKE_TOOLCHAIN_FILE="${PROJECT_ROOT}/raspberrypi4_toolchain.cmake" \
      -DRUNNING_FROM_SCRIPT=1 \
      "${PROJECT_ROOT}"
cmake --build . --target server
cmake --build . --target led
cmake --build . --target buzzer
cmake --build . --target cds
cmake --build . --target segment
cd "${PROJECT_ROOT}"

echo "==============================================="
echo "🚚 Sending Server to Raspberry Pi4..."
echo "==============================================="

ssh ${RPI_USER}@${RPI_IP} "mkdir -p ${RPI_DEST}/lib"

rm -rf "${PROJECT_ROOT}/temp"
mkdir -p "${PROJECT_ROOT}/temp/lib"
cp "${PROJECT_ROOT}/build/build_server/server" \
   "${PROJECT_ROOT}/temp/"
cp "${PROJECT_ROOT}/config/server_config.ini" \
   "${PROJECT_ROOT}/temp/"
cp "${PROJECT_ROOT}/build/build_server/lib/"*.so \
   "${PROJECT_ROOT}/temp/lib/"
rsync -avz "${PROJECT_ROOT}/temp/" ${RPI_USER}@${RPI_IP}:${RPI_DEST}/
rm -rf "${PROJECT_ROOT}/temp"

echo "==============================================="
echo "✅ Server sent to Raspberry Pi4(${RPI_USER}@${RPI_IP}:${RPI_DEST}/) successfully!"
echo "✨ Build & Transfer Complete!"
echo "==============================================="