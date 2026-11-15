git status > .gs
cat .gs | sed '/Untracked files:/Q' > .gs-out


echo
echo "---------- NOT TRACKED ----------"

cat .gs | grep -i "	" | grep -v '#' | grep -v '~' | grep -v ': ' | grep -v "modified: " | grep -P '\.([chp]|bnf|asm|pas)$'

echo "^^^^------ NOT TRACKED ------^^^^"
echo
echo "------------- STATE --------------"
cat .gs-out



