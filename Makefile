
CXX := clang++
PKG_CXX := g++

OUT_DIR  := bin
INT_DIR  := ./obj
EXT_NAME := mailru-cloud-extension
PYTHON_GUI_FILE_NAME := nautilus_mailru_cloud_gtk_gui_services.py

CPP_FILES := $(wildcard ./*.cpp)
OBJ_FILES := $(addprefix obj/,$(notdir $(CPP_FILES:.cpp=.o)))
OBJ_FILES_REL := $(addprefix obj/,$(notdir $(CPP_FILES:.cpp=-rel.o)))
OBJ_FILES_PKG := $(addprefix obj/,$(notdir $(CPP_FILES:.cpp=-pkg.o)))

LINK_FLAGS := $(shell pkg-config libnautilus-extension --libs)  \
			  $(shell pkg-config --libs libnotify)     \
              -lpthread -lboost_filesystem -lboost_system -lboost_thread  \
              -L./dep/cpp-netlib-0.9.4/lib/ -lcppnetlib-client-connections -lcppnetlib-uri -lcppnetlib-server-parsers \
              -lssl -lcrypto

LOCAL_INCLUDES := -I./dep/cpp-netlib-0.9.4/include
COMPILE_FLAGS := $(LOCAL_INCLUDES) $(shell pkg-config --cflags libnotify) \
                 $(shell pkg-config libnautilus-extension --cflags) \
                 $(shell pkg-config  gtk+-3.0 --cflags)

#==================================================================================================
# debug build


extension: extension_so
	echo "copying to nautilus extensions dir"
	sudo cp $(OUT_DIR)/$(EXT_NAME).so /usr/lib/nautilus/extensions-3.0/$(EXT_NAME).so
	sudo cp ./$(PYTHON_GUI_FILE_NAME) /usr/lib/nautilus/extensions-3.0/$(PYTHON_GUI_FILE_NAME)


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
	
	
#==================================================================================================
# release build	

	
extension_release: extension_so_release
	echo "copying to nautilus extensions dir"
	sudo cp $(OUT_DIR)/$(EXT_NAME).so /usr/lib/nautilus/extensions-3.0/$(EXT_NAME).so
	sudo cp ./$(PYTHON_GUI_FILE_NAME) /usr/lib/nautilus/extensions-3.0/$(PYTHON_GUI_FILE_NAME)
	

extension_so_release: $(OBJ_FILES_REL)
	echo "linking release version of " $(OUT_DIR)/$(EXT_NAME).so 
	$(CXX) -fPIC -shared -O3 -Wl,--gc-sections -o $(OUT_DIR)/$(EXT_NAME).so $^ $(LINK_FLAGS)
	

$(INT_DIR)/%-rel.o: ./%.cpp
	echo "compiling $<"  
	$(CXX) -c -std=c++11 -fPIC -O3 -fdata-sections -ffunction-sections -o $@  $(COMPILE_FLAGS) $<
	
	
	
#==================================================================================================
# binary package build	
	
	
deb_package: extension_so_pkg
	echo "building deb package ..."
	cp $(OUT_DIR)/$(EXT_NAME).so ./pkg/nautilus-mailru-cloud-0.1-preview/$(EXT_NAME).so
	cp ./$(PYTHON_GUI_FILE_NAME) ./pkg/nautilus-mailru-cloud-0.1-preview/$(PYTHON_GUI_FILE_NAME)
	cd ./pkg/nautilus-mailru-cloud-0.1-preview; debuild -b
	

extension_so_pkg: $(OBJ_FILES_PKG)
	echo "linking release version (for binary package) of " $(OUT_DIR)/$(EXT_NAME).so 
	$(PKG_CXX) -fPIC -shared -O3 -Wl,--gc-sections -o $(OUT_DIR)/$(EXT_NAME).so $^ $(LINK_FLAGS)
	

$(INT_DIR)/%-pkg.o: ./%.cpp
	echo "(pkg) compiling $<"  
	$(PKG_CXX) -c -std=c++11 -fPIC -O3 -fdata-sections -ffunction-sections -o $@  $(COMPILE_FLAGS) $<
	
	
clean:
	rm -rf $(INT_DIR)/*
	rm $(OUT_DIR)/*
    
