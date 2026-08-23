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
$code= $codesum;
$data= $datasum;
$_= "SUMMARY.o";
&doit();


sub doit {
    if (m:^([-_\w]+.o):) {
	$newname= $1;
	if ($name) {
	    $ddd= $ro + $bss + $data + $zero;
	    if ($code || $ddd) {
		printf("%5d %5d %-28s (ro:$ro bss:$bss data:$data zp:$zero)\n",
		       $code, $ddd, $name);
		$codesum+= $code;
		$datasum+= $ddd;
	    }
	}
	$name= $newname;
	$code= $ro= $bss= $data= $zero= 0;
    }
}
