########################################################################
# Makefile for Linux
#

ifeq ($(v), tr)
    VFLAG := -DTR_SOLUTION
    ODIR := build/linux/tobjs
    shaders = lighting.vert lighting.frag
    udflags = -USOLUTION -UPHONG_SOLUTION -DTR_SOLUTION -UEM -UEM_SOLUTION
    iblExtras = 
else ifeq ($(v), phong)
    VFLAG := -DPHONG_SOLUTION -DTR_SOLUTION
    ODIR := build/linux/pobjs
    shaders = lightingPhong.vert lightingPhong.frag 
    udflags = -USOLUTION -DPHONG_SOLUTION -DTR_SOLUTION -UEM -UEM_SOLUTION
    iblExtras = 
else ifeq ($(v), sol)
    VFLAG := -DTR_SOLUTION -DSOLUTION
    ODIR := build/linux/sobjs
    shaders =  finalSol.frag finalSol.vert lightingSol.frag lightingSol.vert reflSol.frag reflSol.vert shadowSol.frag shadowSol.vert 
    udflags = -DSOLUTION -UPHONG_SOLUTION -DTR_SOLUTION -UEM -UEM_SOLUTION
    iblExtras = ibl
else ifeq ($(v), ref)
    VFLAG := -DTR_SOLUTION -DSOLUTION -DREFL
    ODIR := build/linux/robjs
    shaders = lightingSol.vert lightingSol.frag 
    udflags = -DSOLUTION -UPHONG_SOLUTION -DTR_SOLUTION -UEM -UEM_SOLUTION
    iblExtras = 
else ifeq ($(v), em)
    VFLAG := -DEM
    ODIR := build/linux/eobjs
    shaders = lighting.vert lighting.frag emulator.vert emulator.frag
    udflags = -USOLUTION -UPHONG_SOLUTION -UTR_SOLUTION -DEM -UEM_SOLUTION
    iblExtras = 
else ifeq ($(v), emsol)
    VFLAG := -DSOLUTION -DTR_SOLUTION -DEM -DEM_SOLUTION
    ODIR := build/linux/esobjs
    shaders = finalSol.frag finalSol.vert lightingSol.frag lightingSol.vert reflSol.frag reflSol.vert shadowSol.frag shadowSol.vert emulator.vert emulator.frag
    udflags = -DSOLUTION -UPHONG_SOLUTION -DTR_SOLUTION -DEM -DEM_SOLUTION
    iblExtras = 
else
    VFLAG := 
    ODIR := build/linux/bobjs
    shaders = lighting.vert lighting.frag 
    udflags = -USOLUTION -UPHONG_SOLUTION -UTR_SOLUTION -UEM -UEM_SOLUTION
    iblExtras = 
endif

ifdef c
    pkgName=$(c)-framework
else
    pkgName=Graphics-framework
endif

# ifndef v
# $(error This makefile needs a flag: v=basic, v=tr, or v=sol)
# endif

# Search for the libs directory
ifneq (,$(wildcard libs))
    LIBDIR := libs
else
    ifneq (,$(wildcard ../libs))
        LIBDIR := ../libs
    else
        ifneq (,$(wildcard ../../libs))
            LIBDIR := ../../libs
        else
            LIBDIR := ../../../libs
        endif
    endif
endif

# Where the compiler will search for source files.
VPATH = src $(LIBDIR)/imgui-master $(LIBDIR)/imgui-master/backends

# Where the .o files go
vpath %.o  $(ODIR)

CXX = g++
CFLAGS = -g $(VFLAG) -Isrc -I$(LIBDIR)/glm -I$(LIBDIR)/imgui-master -I$(LIBDIR)/imgui-master/backends -I$(LIBDIR)  -I$(LIBDIR)/glfw/include

CXXFLAGS = -std=c++11 $(CFLAGS) -DVK_TAB=9

LIBS =  -L/usr/lib/x86_64-linux-gnu -L$(LIBDIR) -L/usr/lib -L/usr/local/lib -lglbinding -lX11 -lGLU -lGL `pkg-config --static --libs glfw3`

CPPsrc = HDR.cpp framework.cpp interact.cpp transform.cpp scene.cpp texture.cpp shapes.cpp object.cpp shader.cpp simplexnoise.cpp fbo.cpp emulator.cpp
IMGUIsrc = imgui.cpp imgui_widgets.cpp imgui_draw.cpp imgui_demo.cpp imgui_impl_glfw.cpp imgui_impl_opengl3.cpp
Csrc = rply.c

headers = HDR.h framework.h interact.h texture.h shapes.h object.h rply.h scene.h shader.h transform.h simplexnoise.h fbo.h emulator.h
srcFiles = $(addprefix src/,$(CPPsrc) $(Csrc) $(headers)) $(wildcard shaders/*)
extraFiles = framework.sln framework.vcxproj framework.vcxproj.filters Makefile README.md assets docs

pkgDir = /home/gherron/packages
objs = $(patsubst %.cpp,%.o,$(CPPsrc)) $(patsubst %.cpp,%.o,$(IMGUIsrc)) $(patsubst %.c,%.o,$(Csrc))
target = $(ODIR)/framework.exe

$(target): $(objs)
	@echo Link $(target)
	$(CXX) -g -o $@ $(addprefix $(ODIR)/,$(objs)) $(LIBS)

help:
	@echo "Try:"
	@echo "    make -j8         run  // for base level -- no transformations or shading"
	@echo "    make -j8 v=tr    run  // for transformation level -- no shading"
	@echo "    make -j8 v=phong run  // for lighting level"            
	@echo "    make -j8 v=sol   run  // for full solution level"    
	@echo "    make -j8 v=em    run  // for GPU emulator"  
	@echo "    make -j8 v=emsol run  // for GPU emulator solution"
	@echo "Also:"
	@echo "   make v=em    c=CS200 zip // For CS200 -- bare bones"
	@echo "   make         c=CS251 zip // For CS251 -- bare bones"
	@echo "   make         c=CS541 zip // For CS541 -- bare bones"
	@echo "   make v=tr    c=CS300 zip // For CS300 -- includes transformations"
	@echo "   make v=phong c=CS562 zip // For CS562 -- includes transformations and Phong"
	@echo "   make v=sol   c=sol   zip // For whatever -- includes everything but GPU emulator"
	@echo "   make v=emsol c=emsol zip // For whatever -- includes everything"

run: $(target)
	LD_LIBRARY_PATH="$(LIBDIR):$(LD_LIBRARY_PATH)" ./$(target)

gdb: $(target)
	LD_LIBRARY_PATH="$(LIBDIR):$(LD_LIBRARY_PATH)" gdb ./$(target)

what:
	@echo VPATH = src $(VPATH)
	@echo LIBS = $(LIBDIR)
	@echo CFLAGS = $(CFLAGS)
	@echo VFLAG = $(VFLAG)
	@echo objs = $(objs)
	@echo shaders = $(shaders)
	@echo pkgName = $(pkgName)
	@echo srcFiles = $(srcFiles)
	@echo extraFiles = $(extraFiles)
	@echo udflags = $(udflags)

clean:
	rm -rf build/linux dependencies

%.o: %.cpp
	@echo Compile $<  $(VFLAG)
	@mkdir -p $(ODIR)
	@$(CXX) -c $(CXXFLAGS) $< -o $(ODIR)/$@

%.o: %.c
	@echo Compile $< $(VFLAG)
	@mkdir -p $(ODIR)
	@$(CC) -c $(CFLAGS) $< -o $(ODIR)/$@

zip:
	rm -rf $(pkgDir)/$(pkgName) $(pkgDir)/$(pkgName).zip
	mkdir $(pkgDir)/$(pkgName)
	cp -r src shaders $(pkgDir)/$(pkgName)
	cp -r $(extraFiles) $(iblExtras) $(pkgDir)/$(pkgName)
	cp -r $(LIBDIR) $(pkgDir)/$(pkgName)
	rm -rf $(pkgDir)/$(pkgName)/libs/.hg $(pkgDir)/$(pkgName)/libs/assimp $(pkgDir)/$(pkgName)/libs/bvh
	cd $(pkgDir)/$(pkgName);  unifdef $(udflags)  -m  $(srcFiles) || /bin/true

	cd $(pkgDir);  zip -r $(pkgName).zip $(pkgName)

ws:
	unix2dos $(srcFiles)
	@echo
	@echo ========= TABS:
	@grep -P '\t' $(srcFiles)

dependencies: 
	g++ -MM $(CXXFLAGS) $(addprefix src/,$(CPPsrc)) > dependencies

-include dependencies
