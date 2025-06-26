########################################################################
####################### Makefile Template ##############################
########################################################################

# Compiler settings - Can be customized.
CC = arm-linux-gcc
CXXFLAGS = -Wall -Ofast -Wall -DLOG4C_ENABLE_DEBUG_LEVEL -DBUILD_BIN -Include
LDFLAGS = 
LIBS = -pthread -ljson-c -lssl -lfepkcs11 -lfetpf -lferkl -lfeclr -luuid -lemvco_ep -lemvco_hal -ltlv -llog4c -lcrypto -lrupay_sbt -lfetrm -ldl -lleds -lbuzzer -lpthread

# To be configured as required
#DEVICE = 192.168.10.210
#DEVICE = 192.168.29.101
DEVICE = 10.0.0.40
SLOTAPP = app3
KEYID = 00A3
#SLOTAPP = app0
#KEYID = 00A0
PIN = 648219
VERSION = "1.0.23"
APPDESC = "Datamatic L3 App - PML3"

# Makefile settings - Can be customized.
APPNAME = pml3
EXT = .c
SRCDIR = src
OBJDIR = obj

############## Do not change anything from here downwards! #############
SRC = $(wildcard $(SRCDIR)/*$(EXT)) $(wildcard $(SRCDIR)/*/*$(EXT))
OBJ = $(SRC:$(SRCDIR)/%$(EXT)=$(OBJDIR)/%.o) 
#DEP = $(OBJ:$(OBJDIR)/%.o=%.d)
# UNIX-based OS variables & settings
RM = rm
DELOBJ = $(OBJ)
MKDIR = mkdir
# Windows OS variables & settings
DEL = del
EXE = .exe
WDELOBJ = $(SRC:$(SRCDIR)/%$(EXT)=$(OBJDIR)\\%.o)

########################################################################
####################### Targets beginning here #########################
########################################################################

$(shell mkdir -p obj/ISO)
$(shell mkdir -p obj/JHost)
$(shell mkdir -p obj/http-parser)

all: cpsql $(APPNAME) signapp copyToTerminal

cpsql:
	cp ./pre-built/sqlite3.o ./obj/sqlite3.o

# Builds the app
$(APPNAME): $(OBJ)
	$(CC) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

# Perform signing
signapp:
	cp $(APPNAME) /home/ganapathy/$(APPNAME)
	fesign --module opensc-pkcs11.so --keyid $(KEYID) --pin $(PIN) --force --no-backup --infile /home/ganapathy/$(APPNAME)
	cp /home/ganapathy/$(APPNAME) $(APPNAME)

copyToTerminal:
	sshpass -p '$(SLOTAPP)' scp ./$(APPNAME) $(SLOTAPP)@$(DEVICE):/home/$(SLOTAPP)
	sshpass -p '$(SLOTAPP)' scp ./src/config/app_config.json $(SLOTAPP)@$(DEVICE):/home/$(SLOTAPP)/config/
# 	sshpass -p '$(SLOTAPP)' scp ./src/config/emv_config.txt $(SLOTAPP)@$(DEVICE):/home/$(SLOTAPP)/config/
	sshpass -p '$(SLOTAPP)' scp ./src/config/emv_config.json $(SLOTAPP)@$(DEVICE):/home/$(SLOTAPP)/config/
	sshpass -p '$(SLOTAPP)' scp ./src/log4crc $(SLOTAPP)@$(DEVICE):/home/$(SLOTAPP)/
	sshpass -p '$(SLOTAPP)' scp ./src/log4crc.info $(SLOTAPP)@$(DEVICE):/home/$(SLOTAPP)/
	sshpass -p '$(SLOTAPP)' scp ./src/log4crc.debug $(SLOTAPP)@$(DEVICE):/home/$(SLOTAPP)/
	sshpass -p '$(SLOTAPP)' scp ./src/log4crc.error $(SLOTAPP)@$(DEVICE):/home/$(SLOTAPP)/
	sshpass -p '$(SLOTAPP)' scp ./l3start.sh $(SLOTAPP)@$(DEVICE):/home/$(SLOTAPP)/
	sshpass -p '$(SLOTAPP)' scp ./runl3.sh $(SLOTAPP)@$(DEVICE):/home/$(SLOTAPP)/runl3.sh
	sshpass -p '$(SLOTAPP)' scp ./autorun.sh $(SLOTAPP)@$(DEVICE):/home/$(SLOTAPP)/autorun.sh

#	sshpass -p '$(SLOTAPP)' scp ./$(APPNAME) $(SLOTAPP)@$(DEVICE):/home/$(SLOTAPP)/
#	sshpass -p '$(SLOTAPP)' scp ./src/config/app_config.json $(SLOTAPP)@$(DEVICE):/home/$(SLOTAPP)/config/
#	sshpass -p '$(SLOTAPP)' scp ./src/config/emv_config.txt $(SLOTAPP)@$(DEVICE):/home/$(SLOTAPP)/config/
#	sshpass -p '$(SLOTAPP)' scp ./src/config/emv_config.json $(SLOTAPP)@$(DEVICE):/home/$(SLOTAPP)/config/
#	sshpass -p '$(SLOTAPP)' scp ./src/log4crc $(SLOTAPP)@$(DEVICE):/home/$(SLOTAPP)/
#	sshpass -p '$(SLOTAPP)' scp ./l3start.sh $(SLOTAPP)@$(DEVICE):/home/$(SLOTAPP)/

update_helper:
	echo "Generating update package ..."
	mkdir -p update/app
	cp $(APPNAME) update/app/
	cp run.in update/app/run
	cp runl3.sh update/app/runl3.sh
	cp autorun.sh update/app/autorun.sh
	cp ./src/log4crc update/app/log4crc
	cp ./src/log4crc.debug update/app/log4crc.debug
	cp ./src/log4crc.error update/app/log4crc.error
	cp ./src/log4crc.info update/app/log4crc.info
	cp ./src/l3_log_mode update/app/l3_log_mode
	mkdir -p update/app/config/
	cp ./src/config/emv_config.txt update/app/config/emv_config.txt
	cp ./src/config/emv_config.json update/app/config/emv_config.json
#	cp ./src/config/app_data.json update/app/config/app_data.json
#	cp ./src/config/app_config.json update/app/config/app_config.json
	cp kolkata update/app/kolkata
	cp version update/app/version
	cd update/app && tar -cvf ../app.tar *
	cd ../../
	cp install.in update/install
	rm -rf update/app/
	mkdir /home/ganapathy/update
	cp update/install /home/ganapathy/update
	cp update/app.tar /home/ganapathy/update

update: update_helper
	create_update -i /home/ganapathy/update/ -o package/ -t $(KEYID) -P $(PIN) -n $(APPDESC) -v $(VERSION)
	echo "Update package successfully created in package folder"
	rm -rf /home/ganapathy/update
	rm -rf update

# Creates the dependecy rules
%.d: $(SRCDIR)/%$(EXT)
	@$(CPP) $(CFLAGS) $< -MM -MT $(@:%.d=$(OBJDIR)/%.o) >$@

# Includes all .h files
-include $(DEP)

# Building rule for .o files and its .c/.cpp in combination with all .h
$(OBJDIR)/%.o: $(SRCDIR)/%$(EXT)
	$(CC) $(CXXFLAGS) -o $@ -c $<

################### Cleaning rules for Unix-based OS ###################
# Cleans complete project
.PHONY: clean
clean:
	$(RM) $(DELOBJ) $(DEP) $(APPNAME)

# Cleans only all files with the extension .d
.PHONY: cleandep
cleandep:
	$(RM) $(DEP)

#################### Cleaning rules for Windows OS #####################
# Cleans complete project
.PHONY: cleanw
cleanw:
	$(DEL) $(WDELOBJ) $(DEP) $(APPNAME)$(EXE)

# Cleans only all files with the extension .d
.PHONY: cleandepw
cleandepw:
	$(DEL) $(DEP)