
CXX=g++
INCLUDES=
FLAGS=-D__MACOSX_CORE__ -c
LIBS=-framework CoreAudio -framework CoreMIDI -framework CoreFoundation \
	-framework IOKit -framework Carbon  -framework OpenGL \
	-framework GLUT -framework Foundation \
	-framework AppKit -lstdc++ -lm

OBJS=   RtAudio.o chuck_fft.o Thread.o Stk.o SmotPokin.o

SmotPokin: $(OBJS)
	$(CXX) -o SmotPokin $(OBJS) $(LIBS)

SmotPokin.o: SmotPokin.cpp RtAudio.h chuck_fft.h Stk.h Thread.h
	$(CXX) $(FLAGS) SmotPokin.cpp

Thread.o: Thread.cpp Thread.h 
	$(CXX) $(FLAGS) Thread.cpp

Stk.o: Stk.h Stk.cpp
	$(CXX) $(FLAGS) Stk.cpp

RtAudio.o: RtAudio.h RtAudio.cpp RtError.h
		$(CXX) $(FLAGS) RtAudio.cpp

chuck_fft.o: chuck_fft.h chuck_fft.c
		$(CXX) $(FLAGS) chuck_fft.c
clean:
	rm -f *~ *# *.o SmotPokin
