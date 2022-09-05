# set term png
# set terminal png size 1024,768

set datafile separator ','

set key autotitle columnhead # use the first line as title
set ylabel "Bytes" # label for the Y axis
set xlabel 'Time' # label for the X axis
set style line 100 lt 4 lc rgb "grey" lw 0.5 

set xdata time # tells gnuplot the x axis is time data
set timefmt "%s" # specify our time string format
set format x "%m/%d/%Y"

set style line 101 lw 3 lt rgb "#f62aa0" # style for targetValue (1) (pink)
set style line 102 lw 3 lt rgb "#26dfd0" # style for measuredValue (2) (light blue)
set style line 103 lw 4 lt rgb "#b8ee30" # style for secondYAxisValue (3) (limegreen)

set xtics rotate # rotate labels on the x axis
set key right center # legend placement

# set output "binarySize.png"
# plot "benchmark.csv" using 11:4 with lines ls 101, '' using 11:9 with lines ls 102

# set output "compilationTime.png"
set ylabel "Seconds" 
plot "benchmark.csv" using 11:5 with lines ls 103, '' using 11:10 with lines ls 102
