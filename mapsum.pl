while (<>) {
    if (/objects by size/) {
	printf "SIZE:$_";
	$here= 1;
    }
    next unless $here;

    if (/^.... \((....)\) : (.*)/) {
	$sz= hex($1);
	$sum+= $sz;
	print sprintf("%5d: %04d = $2\n", $sum, $sz, $sum);
    } else {
#printf "%% $_";
    }
}
