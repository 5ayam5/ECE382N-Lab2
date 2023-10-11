all:	sim 

sim:	cache.cpp cache_snoop.cpp cache.h helpers.cpp iu.cpp network.cpp main.cpp test.cpp my_fifo.h generic_error.h types.h
	g++ -g cache.cpp cache_snoop.cpp helpers.cpp iu.cpp network.cpp main.cpp test.cpp -o sim

clean:
	rm sim