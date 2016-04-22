g++ -o bin/add_program add_program.cpp
./xc -v -o bin/lab5 -I../lib lab5.c
./xc -v -o bin/lab5_user -I../lib lab5_user.c
./bin/add_program bin/lab5 bin/lab5_user
./xem bin/lab5
