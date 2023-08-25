for i in $(seq 1 10000)
do
    iperf -c 127.0.0.1 -t 60 &
done