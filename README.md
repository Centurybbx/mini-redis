# mini-redis
Multiple-threads supported redis implemented in C++.

## How to use?
1. `cmake -B build`
2. `cd build && make`
3. `cd bin`
4. `./mini-redis -w [number of worker threads]`

## How to run benchmark?
### Compare to single-thread redis
Just run it, use `-t` to specify the command you want to test.
1. `go run bench.go -[args]`