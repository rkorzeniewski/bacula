rm wx-console.mak
sed -e 's/^\(.*\)\\\(.*\)$/FILENAME=\2\nSOURCE=\1\\\2.cpp\n@@OBJMAKIN@@\n/g' filelist > objtargets1.tmp
sed -e '/@@OBJMAKIN@@/r wx-console-obj.mak.in' -e '/@@OBJMAKIN@@/d' objtargets1.tmp > objtargets.tmp
sed -e 's/^\(.*\)\\\(.*\)$/\t-@erase "\$(INTDIR)\\\2.obj"/g' filelist > relclean.tmp
sed -e 's/^\(.*\)\\\(.*\)$/\t"\$(INTDIR)\\\2.obj" \\/g' filelist > relobjs.tmp
sed 	-e '/@@OBJTARGETS@@/r objtargets.tmp' -e '/@@OBJTARGETS@@/d' \
	-e '/@@REL-CLEAN@@/r relclean.tmp' -e '/@@REL-CLEAN@@/d' \
	-e '/@@REL-OBJS@@/r relobjs.tmp' -e '/@@REL-OBJS@@/d' \
	wx-console.mak.in > wx-console.mak
rm *.tmp