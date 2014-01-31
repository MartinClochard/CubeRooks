
BD=./build/
SRC=./
DP=./depend/
CXX=g++-4.8
FLAGS= -O3 -std=c++11 -Wall -Wfatal-errors

exec: $(BD)grid

$(BD)grid: $(BD)main.o $(BD)grid.o $(BD)job.o $(BD)grid_monothread.o
	$(CXX) $(FLAGS) -pthread -o $(BD)grid $(BD)grid.o $(BD)job.o $(BD)main.o $(BD)grid_monothread.o

$(BD)%.o: $(DP)%.cpp.depend
	$(CXX) $(FLAGS) -I$(SRC) -c -o $@ $*.cpp

$(DP)%.depend: $(SRC)%
	rm -rf $@;
	touch $@

$(DP)main.cpp.depend: $(DP)grid_monothread.h.depend

$(DP)grid.cpp.depend: $(DP)grid.h.depend

$(DP)grid.h.depend: $(DP)query.h.depend $(DP)bitset.h.depend $(DP)job.h.depend

$(DP)query.h.depend:

$(DP)bitset.h.depend:

$(DP)job.h.depend:

$(DP)job.cpp.depend: $(DP)job.h.depend

$(DP)grid_monothread.cpp.depend: $(DP)grid_monothread.h.depend

$(DP)grid_monothread.h.depend: $(DP)grid.h.depend

.PHONY: clean clear

clean:
	rm -rf $(BD)*.o

clear: clean
	rm -rf $(BD)grid $(DP)*.depend

