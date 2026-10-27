# Generate STATIC library.
TARGET   = libgurux_dlms_c.a

#arm-none-eabi-gcc for embedded ARM systems.
CC       = gcc 
# compiling flags here
#CFLAGS   = -g -std=c99 -Wall -fPIC
CFLAGS   = -g -O2 -std=c99 -Wall
#Add this if used on microcontroller. -DGX_DLMS_MICROCONTROLLER
#Add this if you don't want to use malloc. -DDLMS_IGNORE_MALLOC
#Add this if you don't want to use object pointers. -DDLMS_IGNORE_OBJECT_POINTERS
#Add this if you want to use EPOC time. -DDLMS_USE_EPOCH_TIME
#Example: make CFLAGS='-DDLMS_IGNORE_OBJECT_POINTERS'

CFLAGS += -DDLMS_IGNORE_IP6_SETUP=1

LINKER   = ar rcvs
# linking flags here
LFLAGS   = 

# change these to set the proper directories where each files shoould be
SRCDIR   = src
OBJDIR   = obj
BINDIR   = lib

AUTODIR	:= $(OBJDIR) $(BINDIR)

SOURCES  := $(wildcard $(SRCDIR)/*.c)
INCLUDES := $(wildcard $(SRCDIR)/*.h)
OBJECTS  := $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
rm       = rm -vf


$(BINDIR)/$(TARGET): $(OBJECTS) | $(BINDIR)
	$(LINKER) $@ $(LFLAGS) $(OBJECTS)

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONEY: clean
clean:
	$(rm) $(OBJECTS)

.PHONEY: clobber
clobber: clean
	$(rm) $(BINDIR)/$(TARGET)
	rmdir $(AUTODIR)

$(sort $(AUTODIR)):; mkdir -p "$@"
