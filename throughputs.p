set datafile separator '\t'
set terminal png
set key right top
set ylabel "Requests per second"
set y2label "95th Percentile Latency"
set xlabel "Number of threads"
set output "throughputs.png" 
set ytics 2000 nomirror tc lt 1
set y2tics 1000 nomirror tc lt 2
plot "throughputs.csv" using 1:2 notitle with lp, \
"percents.csv"  using 1:2 notitle with lp