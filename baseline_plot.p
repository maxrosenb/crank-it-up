set datafile separator ','
set terminal png
set key right bottom
set ylabel "Ratio of Requests" 
set xlabel "Latency(us)" 
set yrange [0:1]
set logscale x 2
set output 'baseline_plot.png'
plot "nreqs.csv" using 1:2 notitle with lp
