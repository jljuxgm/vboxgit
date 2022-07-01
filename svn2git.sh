#!/bin/bash

root=$(pwd)
cd ./vboxgit
echo $(pwd)
rootpath=$(pwd)
sourcepath=$rootpath/vbox

if [ -e .git ]; then
	echo .git exsitting...
else 
	git init
fi

if [ -e .gitignore ]; then
	echo .gitignore exsiting...
else
	touch .gitignore
	echo ./vbox/.svn/ > .gitignore
	git add .gitignore
fi

if [ -d $sourcepath ]; then
	cd $sourcepath
	echo $(pwd)
	int=$(svn info | grep Revision: | sed 's/Revision: //g')
	svnf=update
	cd $rootpath
else
	int=1
	svnf='co https://www.virtualbox.org/svn/vbox/trunk vbox'
fi

echo $int


#:<<EOF
while(($int<=95063))
do
	echo $int
	[ -d vbox ] && cd $sourcepath
	[ -e .svn ] && svn cleanup
	svn $svnf -r $int
	log=$(svn log -r $int)
	[ -e .svn ] && svn cleanup
	cd $rootpath
	git add $sourcepath/*
	git commit -m "commit_$int $log"
	git push
	svnf=update
	let "int++"
done
#EOF
