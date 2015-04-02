BOOST_LIBS=-lboost_system-mt -lboost_thread-mt -lboost_chrono-mt

scheduler: main.cpp scheduler.cpp
	clang++ -I/usr/local/include -L/usr/local/lib -g -ggdb -o scheduler main.cpp scheduler.cpp $(BOOST_LIBS)
