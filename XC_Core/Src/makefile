CXX = g++

PACKAGENAME = XC_Core

include ../../makefile-common

INCLUDES = -I. -I../Inc -I../../Core/Inc -I../../Engine/Inc -I../../CacusLib -I/usr/include/i386-linux-gnu/ -I/usr/local/include/SDL2

LIBS = ../../System/Core.so ../../System/Engine.so ../../System/Cacus.so

SRCS = XC_CoreScript.cpp	\
	XC_Networking.cpp	\
	XC_LZMA.cpp	\
	XC_Globals.cpp	\
	XC_Generic.cpp	\
	Devices.cpp	\
	PathBuilder.cpp	\
	RouteMapper.cpp	\
	Math.cpp	\
	URI.cpp

OBJS = $(SRCS:%.cpp=$(OBJDIR)%.o)

DEPS = $(SRCS:%.cpp=$(OBJDIR)%.d)

.PHONY: all
all: $(OUTPUT)

#Link
$(OUTPUT) : $(OBJS)
	$(CXX) -o $@ $(LDFLAGS) $^ $(LIBS)
	strip --strip-all --discard-all $(OUTPUT)

#Compile
$(OBJS) : $(OBJDIR)%.o : %.cpp
	$(CXX) -c $(PREPROCESSORS) $(CXXFLAGS) $(INCLUDES) -o $@ $< > $(OBJDIR)$*.lst


#Generate dependancies
#%.d : %.cpp
$(DEPS) : $(OBJDIR)%.d : %.cpp
	@echo $(@:%.d=%.o) $@ : \\ > $@
	@/bin/sh -ec '$(CXX) -MM $(PREPROCESSORS) $(CXXFLAGS) $(INCLUDES) $< | \
		sed -e '\''s/.*://'\'' >> $@'

include $(DEPS)

.PHONY: clean
clean:
	-${RM} -f ${OBJS} $(SRCS:%.cpp=$(OBJDIR)%.lst) $(SRCS:%.cpp=$(OBJDIR)%.d)


