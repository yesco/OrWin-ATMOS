$codesum= $datasum= 0;

while(<>) {
    &doit();
    $code  = hex($1) if /CODE.*Size=0+(\w+)/;
    $ro    = hex($1) if /RODATA.*Size=0+(\w+)/;
    $bss   = hex($1) if /BSS.*Size=0+(\w+)/;
    $data  = hex($1) if /DATA.*Size=0+(\w+)/;
    $zero  = hex($1) if /ZERO.*Size=0+(\w+)/;
}

&doit();

print "\n";
$code= $ro= $bss= $data= $zero= 0;
$name= "SUMMARY";
$code= $orwincode;
$data= $orwindata;
$_= "SUMMARY.o";
&doit();

print "\n";
$code= $ro= $bss= $data= $zero= 0;
$name= "CC65";
$code= $cc65code;
$data= $cc65data;
$_= "CC65.o";
&doit();


sub doit {
    $orwin= (m:^([-_\w]+.o):);
    $cc65 = (m:/:); 

    if ($orwin || $cc65) {
	$newname= $orwin? $1: "CC65";
	if ($name) {
	    $ddd= $ro + $bss + $data + $zero;
	    if ($code || $ddd) {
		if ($orwin) {
		    print STDERR sprintf(
			"%5d %5d %-28s (ro:$ro bss:$bss data:$data zp:$zero)\n",
			$code, $ddd, $name);
		    $orwincode+= $code;
		    $orwindata+= $ddd;

		    $name =~ s/\.o//;
		    $app  = $name;
		    $app  =~ s/app-/_/;
		    $app  =~ s/app_//;

		    $name =~ s/^app-(\w+)/$1_main/;
		    if ($app =~ /^(_|CC65|SUMMARY)/ ) {} else {
			print STDOUT sprintf(
			    "{ \"%s\", %s, %d },\n",
			    $app, $name, $code+$ddd);
		    }
		} else {
		    $cc65code+= $code;
		    $cc65data+= $ddd;
		}
	    }
	}
	$name= $newname;
	$code= $ro= $bss= $data= $zero= 0;
    } else {
#	print "%%$_";
#	$cc65code+= $code;
#L	$cc65data+= $ddd;
    }
}
