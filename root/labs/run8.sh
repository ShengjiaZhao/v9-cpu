#!/usr/bin/env bash
./xc -o ../bin/c -I../lib ../bin/c.c
./xc -v -o bin/lab8 -I../lib lab8.c
./xmkfs sfs.img ../../root
mv sfs.img ../etc/.
./xmkfs ../../fs.img ../../root
./xem -f ../../fs.img bin/lab8