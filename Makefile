
CXX := g++-5

OUT_DIR  := bin
INT_DIR  := ./obj
EXT_NAME := mailru-cloud-extension

CPP_FILES := $(wildcard ./*.cpp)
OBJ_FILES := $(addprefix obj/,$(notdir $(CPP_FILES:.cpp=.o)))

LINK_FLAGS := $(shell pkg-config libnautilus-extension --libs)  \
			  $(shell pkg-config --libs libnotify)     \
              -lpthread -lboost_filesystem -lboost_system -lboost_thread  \
              -L./dep/cpp-netlib-0.9.4/lib/ -lcppnetlib-client-connections -lcppnetlib-server-parsers -lcppnetlib-uri  \
              -lssl -lcrypto

LOCAL_INCLUDES := -I./dep/cpp-netlib-0.9.4/include
COMPILE_FLAGS := $(LOCAL_INCLUDES) $(shell pkg-config --cflags libnotify) $(shell pkg-config libnautilus-extension --cflags)


extension: extension_so
	echo "copying to nautilus extensions dir"
	sudo cp $(OUT_DIR)/$(EXT_NAME).so /usr/lib/nautilus/extensions-3.0/$(EXT_NAME).so


extension_so: $(OBJ_FILES)
	echo "linking" $(OUT_DIR)/$(EXT_NAME).so 
	$(CXX) -g -fPIC -shared -o $(OUT_DIR)/$(EXT_NAME).so $^ $(LINK_FLAGS)
	

test: $(OBJ_FILES)
	echo "linking" $(OUT_DIR)/$(EXT_NAME) 
	$(CXX) -std=c++11 -g -fPIC -o $(OUT_DIR)/$(EXT_NAME) $^ $(LINK_FLAGS)


boost_hdrs: boost_headers.hpp
	echo "generating boost precompiled headers"
	$(CXX) -fPIC $(LOCAL_INCLUDES) boost_headers.hpp
	

$(INT_DIR)/%.o: ./%.cpp
	echo "compiling $<"  
	$(CXX) -c -g -std=c++11 -fPIC -o $@  $(COMPILE_FLAGS) $<
	
	
clean:
	rm -rf $(INT_DIR)/*
	rm $(OUT_DIR)/*
    
