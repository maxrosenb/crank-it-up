set datafile separator '\t'
set terminal png
set key right bottom
set ylabel "95th Percentile Latency" 
set xlabel "Number of threads"
set output "percents.png" 
plot "percents.csv" with lp