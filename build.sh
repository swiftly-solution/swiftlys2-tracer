export XMAKE_ROOT=y
export DOTNET_PATH=./vendor/runtime
xmake f --cc=gcc-14 --cxx=g++-14 -y
xmake f -m release
xmake build -j $(($(nproc)-1))
