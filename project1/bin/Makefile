all: ls cat noop number removetag removetag0

ls:
	cp /bin/ls .
cat:
	cp /bin/cat .
%:%.cpp
	g++ $^ -o $@ 
