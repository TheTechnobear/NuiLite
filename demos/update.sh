#/bin/bash
cp ../build/release/lib/* PolyPd/.
zip -R PolyPd.zip PolyPd/*
cp ../build/release/lib/* SimplePd/.
zip -R SimplePd.zip SimplePd/*

if [ "$1" != "" ]
then
	echo scp *.zip $1
	scp *.zip $1
fi

