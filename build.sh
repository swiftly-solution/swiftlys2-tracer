export XMAKE_ROOT=y
export DOTNET_PATH=./vendor/runtime
xmake f -m release
xmake build -j $(($(nproc)-1))
