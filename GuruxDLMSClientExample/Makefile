TARGET = dlms-c

CC = gcc
LD = gcc
CFLAGS = -std=gnu99 -g -O -pipe -Wall -I../development/include -Iinclude -D__linux__
LFLAGS = -L../development/lib

OBJDIR = obj
INCLUDEDIR = include
AUTODIR += $(OBJDIR)

SOURCES := $(wildcard *.c)
INCLUDES := $(wildcard $(INCLUDEDIR)/*.h)
OBJECTS := $(SOURCES:%.c=$(OBJDIR)/%.o)
rm = rm -f

$(TARGET): $(OBJECTS)
	$(LD) -o $@ $(LFLAGS) $(OBJECTS) -lgurux_dlms_c -lm

$(OBJECTS): $(OBJDIR)/%.o : %.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONEY: clean
clean:
	$(rm) $(wildcard $(OBJECTS))

.PHONEY: clobber
clobber: clean
	$(rm) $(TARGET)

$(sort $(AUTODIR)):; mkdir -p $@
