WALLTIMEHOURS := 24
# WALLTIMEHOURS IS DEPRECATED: use command line argument e.g. "./sn3d -w 24" in the future
GIT_VERSION := $(shell git describe --dirty --always --tags)
GIT_HASH := $(shell git rev-parse HEAD)
GIT_BRANCH := $(shell git branch | sed -n '/\* /s///p')

SYSNAME := $(shell uname -s)

ifeq ($(SYSNAME),Darwin)
	# macOS

	#if using homebrew gsl, gperftools, etc, you might need to add:
	# export C_INCLUDE_PATH=$C_INCLUDE_PATH:/usr/local/include/
	# export LIBRARY_PATH=$LIBRARY_PATH:/usr/local/lib/
	# to your .zshrc/.bashrc startup script

	# CXX = gcc-9
	# CXX = icc
	# CXX = mpicc
	CXX = clang++
	CXXFLAGS = -std=c++17 -O3 -fstrict-aliasing -ftree-vectorize -flto

	CXXFLAGS += -Winline -Wall -Wextra -Wredundant-decls -Wundef -Wstrict-prototypes -Wmissing-prototypes -Wno-unused-parameter -Wno-unused-function -Wstrict-aliasing

	# CXXFLAGS += -fopenmp-simd
	# CXXFLAGS += -fvectorize

	# enable OpenMP for Clang
	# CXXFLAGS += -Xpreprocessor -fopenmp -lomp

	# in GCC6, -Wmisleading-indentation will be useful
	# also -fopenmp after -I$(INCLUDE)
	# maybe  -fopt-info-vec-missed
	#  -fwhole-program
	# add -lprofiler for gperftools
	LDFLAGS = $(LIB) -lgsl -lgslcblas -lprofiler

else ifneq (,$(findstring kelvin,$(HOSTNAME)))
  # QUB Kelvin cluster
	# needs
	#  mpi/openmpi/1.8.5/gcc-4.4.7
	#  compilers/gcc/system(default)
	#  libs/gsl/1.16/gcc-4.4.7

	CXX = mpicc
	CXXFLAGS = -mcmodel=medium -O3 -I$(GSLINCLUDE) #-fopenmp=libomp
	LDFLAGS= -lgsl -lgslcblas -lm -L$(GSLLIB)

	sn3d: CXXFLAGS += -DMPI_ON

else ifneq (, $(shell which mpicc))
	# any other system which has mpicc available (Juwels, Cambridge, Gadi, etc)

	CXX = mpicc
	CXX = c++
	CXXFLAGS = -march=native -Wstrict-aliasing -O3 -g -fstrict-aliasing #-fopenmp=libomp
	LDFLAGS = -lgsl -lgslcblas -lm


# sn3d: CXXFLAGS += -DMPI_ON
else
	CXX = c++
	CXXFLAGS = -march=native -Wstrict-aliasing -O3 -fstrict-aliasing #-fopenmp=libomp
	LDFLAGS= -lgsl -lgslcblas -lm
endif


# Use GSL inline functions and skip array range checking for performance
CXXFLAGS += -DHAVE_INLINE -DGSL_C99_INLINE -DGSL_RANGE_CHECK_OFF

exspec exgamma: CXXFLAGS += -DDO_EXSPEC
exgamma: CXXFLAGS += -DDO_EXGAMMA

sn3dmpi: CXX = mpicc
sn3dmpi: CXXFLAGS += -DMPI_ON
sn3dmpi: sn3d

sn3dopenmp: CXXFLAGS += -Xpreprocessor
sn3dopenmp: CXXFLAGS += -fopenmp
sn3dopenmp: LDFLAGS += -lomp
sn3dopenmp: sn3d

sn3dcuda sn3dcudawhole: LDFLAGS += -lcudart

sn3dcuda sn3dcudawhole: CXXFLAGS += -DCUDA_ENABLED=true

# QUB ARC jakita.starfleet
ifneq (,$(findstring jakita,$(HOSTNAME)))
	# Tesla K80
	CUDA_NVCC_FLAGS += --gpu-architecture=sm_37
	INCLUDE += -I/usr/local/cuda/samples/common/inc/
endif

# Gadi
ifneq (,$(findstring gadi,$(HOSTNAME)))
	# Tesla V100
	CUDA_NVCC_FLAGS += -arch=sm_70 -gencode=arch=compute_70,code=sm_70 -gencode=arch=compute_70,code=compute_70
	CXX = icc
	INCLUDE += -I/home/120/ljs120/cuda_samples/common/inc
endif

# CXXFLAGS += -std=c++11
# CXXFLAGS += -fPIC -shared
# CUDA_NVCC_FLAGS += -Xcompiler -fPIC -shared -rdc=true
CUDA_NVCC_FLAGS += -ccbin=$(CXX) -std=c++11 -O3 -Xcompiler "$(CXXFLAGS)" -rdc=true
CUDA_NVCC_FLAGS += -G -g

### use pg when you want to use gprof the profiler
#CXXFLAGS = -g -pg -Wall -I$(INCLUDE)
sn3d_files = sn3d.cc atomic.cc boundary.cc emissivities.cc gamma.cc globals.cc grey_emissivities.cc grid_init.cc gsl_managed.cc input.cc kpkt.cc ltepop.cc macroatom.cc nltepop.cc nonthermal.cc decay.cc packet_init.cc photo_electric.cc polarization.cc radfield.cc ratecoeff.cc rpkt.cc thermalbalance.cc update_grid.cc update_packets.cc vectors.cc vpkt.cc md5.cc

sn3d_objects = sn3d.o atomic.o boundary.o emissivities.o gamma.o globals.o grey_emissivities.o grid_init.o gsl_managed.o input.o kpkt.o ltepop.o macroatom.o nltepop.o nonthermal.o decay.o packet_init.o photo_electric.o polarization.o radfield.o ratecoeff.o rpkt.o thermalbalance.o update_grid.o update_packets.o vectors.o vpkt.o md5.o


all: sn3d exspec

sn3d: clean version
	$(CXX) $(CXXFLAGS) $(sn3d_files) $(LDFLAGS) -o sn3d

sn3ddebug: clean version $(sn3d_objects)
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(LDFLAGS) $(sn3d_objects) -o sn3d

sn3dcudawhole: version
	nvcc -x cu $(CUDA_NVCC_FLAGS) $(INCLUDE) $(LDFLAGS) $(sn3d_files) -o sn3d

sn3dcuda: version $(sn3d_objects)
	nvcc $(CUDA_NVCC_FLAGS) $(INCLUDE) $(LDFLAGS) $(sn3d_objects) -o sn3d

%.o: %.cc
	nvcc -x cu $(CUDA_NVCC_FLAGS) $(INCLUDE) --device-c $< -c

exspec_files = exspec.cc grid_init.cc globals.cc input.cc vectors.cc packet_init.cc update_grid.cc update_packets.cc gamma.cc boundary.cc macroatom.cc decay.cc rpkt.cc kpkt.cc photo_electric.cc emissivities.cc grey_emissivities.cc ltepop.cc atomic.cc ratecoeff.cc thermalbalance.cc light_curve.cc spectrum.cc polarization.cc nltepop.cc radfield.cc nonthermal.cc vpkt.cc md5.cc

exspec: clean version
	$(CXX) $(CXXFLAGS) $(exspec_files) $(LDFLAGS) -o exspec

exgamma: clean version
	$(CXX) $(CXXFLAGS) $(exspec_files) $(LDFLAGS) -o exgamma


.PHONY: clean version

version:
	@echo "#define GIT_VERSION \"$(GIT_VERSION)\"" > version.h
	@echo "#define GIT_HASH \"$(GIT_HASH)\"" >> version.h
	@echo "#define GIT_BRANCH \"$(GIT_BRANCH)\"" >> version.h
	@echo "#define COMPILETIME \"`date`\"" >> version.h

clean:
	rm -f *.o version.h
