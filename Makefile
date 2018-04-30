#####################################################################
# This will only work in GNU GCC or a C compiler who supports       #
# the -MD -MP flags provided by GCC                                 #
#####################################################################

#####################################################################
# Compilation related variables                                     #
#####################################################################

CC := gcc
EXEC := llmnrd

CFLAGS := -g -c -Wall -Wextra
LFLAGS := -pthread -Wall -Wextra -o

#####################################################################
# Tar variables                                                     #
#####################################################################

TARFILE := responder.tar.gz
INSTALLFILE := .install.sh
SCRIPTFILE := .llmnr.sh
MAKEFILE := Makefile

#####################################################################
# Dependency generation variables                                   #
# MMD to generate the dependency file                               #
# MT to set the name of the target                                  #
# MP generates dummy rules for all the prerequisites. This will     #
# prevent make from generate errors from missing prerequistes files #
# MF to indicate where to put the output file                       #
#####################################################################

DEPENDENCYDIR := .d
DEPENDENCYFLAGS = -MMD -MT $@ -MP -MF $(DEPENDENCYDIR)/$*.d
POSTCOMPILE = mv -f $(DEPENDENCYDIR)/$*.td $(DEPENDENCYDIR)/$*.d

#####################################################################
# Variables related to source, header and object files              #
# SRCEXTRA will contain source files that don't have a              #
# header file                                                       #
#####################################################################

SRC := llmnr_net_interface.c llmnr_names.c \
       llmnr_responder_s1.c llmnr_responder_s2.c \
       llmnr_syslog.c llmnr_rr.c llmnr_packet.c \
       llmnr_conflict.c llmnr_sockets.c llmnr_conflict_list.c \
       llmnr_print.c llmnr_signals.c llmnr_utils.c llmnr_str_list.c \

SRCEXXTRA := llmnr_responder.c
INCLUDE := llmnr_defs.h $(SRC:.c=.h)
OBJS := $(SRC:.c=.o) $(SRCEXXTRA:.c=.o)

#####################################################################
# Paths for source and includes directorys                          #
#####################################################################

SRCPATH := src
INCPATH := include
vpath %.c $(SRCPATH)
vpath %.h $(INCPATH)
vpath %.d $(DEPENDENCYDIR)

#####################################################################
# Rules                                                             #
#####################################################################

$(shell mkdir -p $(DEPENDENCYDIR) >/dev/null)

$(EXEC): $(OBJS)
	$(CC) $(LFLAGS) $(EXEC) $(OBJS)

%.o: %.c
	$(CC) $(DEPENDENCYFLAGS) $(CFLAGS) $<

%.d: ;

install:
	@./$(INSTALLFILE)

-include $(patsubst %,$(DEPENDENCYDIR)/%.d,$(basename $(SRC)))

#####################################################################
# Phony rules                                                       #
#####################################################################

.PHONY: clean cleanall tar dummy

clean:
	@rm -f $(OBJS)

cleanall:
	@rm -f $(OBJS) $(EXEC) $(DBGEXEC) $(TARFILE)

tar: $(SRC) $(SRCEXXTRA) $(INCLUDE) $(INSTALLFILE) \
     $(SCRIPTFILE) $(MAKEFILE)
	@tar cvfz $(TARFILE) $?

dummy:
	@$(CC) -o $(EXEC) -Wall -Wextra -pthread \
        src/llmnr_responder.c src/llmnr_names.c \
        src/llmnr_net_interface.c src/llmnr_rr.c \
        src/llmnr_conflict_list.c src/llmnr_syslog.c \
        src/llmnr_responder_s1.c src/llmnr_responder_s2.c \
        src/llmnr_sockets.c src/llmnr_packet.c \
        src/llmnr_signals.c src/llmnr_conflict.c \
        src/llmnr_print.c src/llmnr_utils.c \
        src/llmnr_str_list.c
