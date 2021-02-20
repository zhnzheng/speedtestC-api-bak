CC = gcc
OPENSSL_LIBS = -lssl -lcrypto
CFLAGS = -fPIC -Os -Wall -std=gnu99
LDFLAGS	=  
LIBS := -lm -lpthread $(OPENSSL_LIBS) -L../output/lib/ -lspeedtest 

BIN = sample
SRCDIR = ./
OBJDIR = ./
SOURCES := $(wildcard $(SRCDIR)/*.c)
OBJS := $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

all: $(BIN)
	
$(BIN):$(OBJS)
	@echo " CC $@"
	@$(CC) -o $@ $(OBJS) $(LIBS)

clean:
	rm -rf $(BIN) *.o

$(OBJS): $(OBJDIR)/%.o : $(SRCDIR)/%.c
	@echo " CC $@"
	@$(CC) $(CFLAGS) -c $< -o $@


