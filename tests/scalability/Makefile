FLAGS = -O3 -W -Wall -Wextra -pedantic
INCLUDES = -I$$HOME/include -L$$HOME/lib -lboost_mpi -lboost_program_options -lboost_serialization -lzoltan
CXX = mpic++ $(FLAGS)

HEADERS = \
	../../dccrg.hpp \
	../../dccrg_index.hpp \
	../../dccrg_arbitrary_geometry.hpp \
	../../dccrg_constant_geometry.hpp

all: scalability

scalability: scalability.cpp $(HEADERS) Makefile
	$(CXX) scalability.cpp $(INCLUDES) -o scalability

c: clean
clean:
	rm -f scalability
