CXX = g++

PACKAGENAME = XC_IpDrv

include ../../makefile-common

INCLUDES = -I. -I../Inc -I../../Core/Inc -I../../Engine/Inc -I../../XC_Core/Inc -I../../CacusLib -I/usr/include/i386-linux-gnu/ -I/usr/local/include/SDL2

LIBS = ../../System/Core.so ../../System/Engine.so ../../System/Cacus.so ../../System/XC_Core.so 

SRCS = DownloadURL.cpp	\
	HTTP.cpp	\
	IPv6.cpp	\
	NetDriver.cpp	\
	Socket.cpp	\
	XC_IpDrv.cpp

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
	mkdir -p Obj
	@echo $(@:%.d=%.o) $@ : \\ > $@
	@/bin/sh -ec '$(CXX) -MM $(PREPROCESSORS) $(CXXFLAGS) $(INCLUDES) $< | \
		sed -e '\''s/.*://'\'' >> $@'

include $(DEPS)

.PHONY: clean
clean:
	-${RM} -f ${OBJS} $(SRCS:%.cpp=$(OBJDIR)%.lst) $(SRCS:%.cpp=$(OBJDIR)%.d)


