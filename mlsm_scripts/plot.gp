
set output "plot_cumulative_count.png"
set terminal png size 800,600


file = "./key_lifetime.txt"

set xlabel " lifetime "
#set ylabel "lifetime count  "


stats file 
plot file using 2:(1.0) smooth cumulative title "cdf avg lifetime"
set xrange [5:]
binwidth = 5
#historgram of count of occurences of lifetimes
#bin(x,width)=width*floor(x/width) 
#plot file using (bin(column(2),binwidth)):(1.0) smooth freq with boxes title "avg lifetime"

#kdensity bandwidth 100
#using 1:2 with linespoints title "avg lifetime"

