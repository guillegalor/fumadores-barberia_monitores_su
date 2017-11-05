.SUFFIXES:
.PHONY: x1, x2, clean

compilador:=g++
opcionesc:= -std=c++11 -pthread -Wfatal-errors -I.
hmonsrcs:= HoareMonitor.hpp HoareMonitor.cpp

x0: x2

x1: fumadores_su
	./$<

x2: barberia_su
	./$<

fumadores_su: fumadores_su.cpp $(hmonsrcs)
	$(compilador) $(opcionesc)  -o $@ $< HoareMonitor.cpp

barberia_su: barberia_su.cpp $(hmonsrcs)
	$(compilador) $(opcionesc)  -o $@ $< HoareMonitor.cpp

clean:
	rm -f fumadores_su barberia_su
