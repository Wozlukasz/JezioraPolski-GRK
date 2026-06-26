mkdir -p cmake-build-debug
cd cmake-build-debug
cmake ..
make -j8
./JezioraPolski-GRK > log.txt 2>&1 &
PID=$!
sleep 5
kill $PID
cat log.txt | grep "Flat LOD chunks"
