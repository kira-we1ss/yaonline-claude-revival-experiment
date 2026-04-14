#!/bin/bash

if [ $(which lrelease > /dev/null 2>&1; echo $?) -eq 0 ]; then
	LRELEASE=$(which lrelease);
elif [ $(which lrelease-qt4 > /dev/null 2>&1; echo $?) -eq 0 ]; then
	echo 2
	LRELEASE=$(which lrelease-qt4);
else
	echo "lrelease not found."
	exit 2
fi

cd ../lang && $LRELEASE psi_ru.ts && $LRELEASE qt_ru.ts && cd ../src
