lib := libuthread.a
objs := sem.o tps.o
other := thread.o queue.o
CC      := gcc
CFLAGS  := -Wall -Wextra -Werror
CFLAGS  += -g

ifneq ($(V),1)
Q=@
endif

all: $(lib)

deps := $(patsubst %.o,%.d,$(objs))
-include $(deps)
DEPFLAGS = -MMD -MF $(@:.o=.d)

%.o: %.c
	@echo "CC $@"
	$(Q) $(CC) $(CFLAGS) -c -o $@ $< $(DEPFLAGS)

$(lib): $(objs) $(other)
	ar rcs $(lib) $(objs) $(other)

clean:
	@echo "clean"
	$(Q) rm -f $(lib) $(objs) $(deps)
