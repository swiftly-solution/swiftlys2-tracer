export XMAKE_ROOT=y
export DOTNET_PATH=./vendor/runtime
xmake f -m release
xmake f --cc=gcc-14 --cxx=g++-14 -y
xmake build -j $(($(nproc)-1))
