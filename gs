git status > .gs
cat .gs | sed '/Untracked files:/Q'


echo
echo "---------- NOT TRACKED ----------"

cat .gs | grep -i "	" | grep -v '#' | grep -v '~' | grep -v ': ' | grep -v "modified: " | grep -P '\.([chp]|bnf|asm|pas)$'




