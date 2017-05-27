#------------------------------------------------------------------------------
where-am-i = $(dir $(abspath $(CURDIR)/$(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))))

myINC:=inc \
../communicationprotocol/inc \

mySRC:=esp8266.c

include ../../makefile.def 

whattobuild: $(OBJ) $(LIBNAME)


