BIN	= flasher
CC	= gcc
STRIP	= strip

#ECHO	=
#OFLAGS	= -g
ECHO	= @
OFLAGS	= -O3 -flto
CFLAGS	= $(OFLAGS) -Wall -fdata-sections -ffunction-sections
LDFLAGS	= $(OFLAGS) -Wl,--gc-sections -Wl,-Map,$(BIN).map
LDFLAGS	+= -lpigpio

SRC	= $(wildcard *.c)
OBJ	= $(SRC:.c=.o)
DEP	= $(SRC:.c=.d)

all: $(BIN)

-include $(DEP)

$(BIN): $(OBJ)
	@echo "$^ -> $@"
	$(ECHO)$(CC) -o $@ $^ $(LDFLAGS)
	$(ECHO)$(STRIP) $@

%.o: %.c
	@echo "$< -> $@"
	$(ECHO)$(CC) $(CFLAGS) -MMD -o $@ -c $<

.PHONY: clean install

clean:
	@rm -f *.o *.d *.map $(BIN)
