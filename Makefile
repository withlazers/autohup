######################################################################
# @author      : Enno Boland (mail@eboland.de)
# @file        : Makefile
# @created     : Tuesday Nov 09, 2021 18:51:02 CET
######################################################################

CFLAGS ?= -Wall -Wpedantic -Werror
VERSION ?= 0.1

ifeq ($(STATIC),1)
    LDFLAGS += -static
endif

ifeq ($(DEBUG),1)
    LDFLAGS += -g
else
	CFLAGS += -O2
endif

ifeq ($(STRIP),1)
    LDFLAGS += -s
endif

autohup: main.c
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS) -DVERSION='"$(VERSION)"'

.PHONY: clean release
clean:
	rm -f autohup

release: clean
	test -z "`git status -s`"
	test "`git branch --show-current`" = main
	test "`git rev-parse HEAD`" = "`git rev-parse origin/main`"
	git tag v$(VERSION)
	git push --tags
