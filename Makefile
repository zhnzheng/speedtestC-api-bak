CC = gcc
AR = ar
OPENSSL_LIBS = -lssl -lcrypto
CFLAGS = -fPIC -Os -Wall -std=gnu99
LDFLAGS	=
LIBS := -lm -lpthread $(OPENSSL_LIBS)

SRCDIR = src
OUTPUTDIR = output
OBJDIR = $(OUTPUTDIR)/obj
LIBDIR = $(OUTPUTDIR)/lib
BINDIR = $(OUTPUTDIR)/bin
SAMPLEDIR = sample
LIBNAME = libspeedtest.a
SOURCES := $(wildcard $(SRCDIR)/*.c)

OBJS := $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

all: prepare lib sample

prepare:
	@echo " CC $@"
	@mkdir -p $(OBJDIR)
	@mkdir -p $(LIBDIR)

lib: $(OBJS)
	@echo " CC $@"
	@$(AR) r $(LIBDIR)/$(LIBNAME) $(OBJS)

sample:
	@echo " CC $@"
	@$(MAKE) -C $@
	
clean:
	rm -rf $(OUTPUTDIR)
	@$(MAKE) -C sample clean

$(OBJS): $(OBJDIR)/%.o : $(SRCDIR)/%.c
	@echo " CC $@"
	@$(CC) $(CFLAGS) -c $< -o $@ $(LIBS)

.PHONY: prepare lib sample clean

