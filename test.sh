source ./build.sh
export CORECLR_ENABLE_PROFILING=1
export CORECLR_PROFILER={a2648b53-a560-486c-9e56-c3922a330182}
export CORECLR_PROFILER_PATH=./build/linux/x64/release/libsw2tracer.so
./DotnetTest/bin/Debug/net10.0/DotnetTest