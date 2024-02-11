SRC := src
INCLUDE := include

CXX := g++
CXXFLAGS := -std=c++20

mytraceroute: $(wildcard $(SRC)/*.cc)
	$(CXX) $(CXXFLAGS) $^ -o $@ -I $(INCLUDE) 