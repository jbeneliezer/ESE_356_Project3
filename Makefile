all:
	g++ -I. -I$$SYSTEMC_HOME/include -L. -L$$SYSTEMC_HOME/lib-linux64 -Wl,-rpath=$$SYSTEMC_HOME/lib-linux64 -o output *.cpp -lsystemc -lm -g
clean:
	rm output
	rm rx.txt
	rm processing1.txt
	rm processing2.txt
	rm  processing3.txt
	rm *.vcd