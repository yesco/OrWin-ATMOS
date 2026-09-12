echo "--- CC65 ---"
cl65 -DPROGSIZE -t sim6502 $* timestamp.c -o timestamp.sim && sim65 timestamp.sim && ls -l timestamp.sim

echo 
echo "--- OSCAR64 ---"
#./oscar -DNOFLOAT -Os $* timestamp.c -e && ls -l timestamp.prg
./oscar -DNOLONG -DNOFLOAT -Os $* timestamp.c -e && ls -l timestamp.prg
