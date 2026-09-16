# oledd - no dependencies beyond libc; builds on the robot itself.
CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra
# /usr, not /usr/local: the same path the meta-dogzilla recipe packages it at,
# so oledd.service works whether the program was installed by hand or by image.
prefix ?= /usr
DESTDIR ?=

oledd: oledd.c font.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ oledd.c

# Regenerate the glyph tables. Needs python3 and the X11 misc bitmap fonts
# (xorg-fonts-misc / xfonts-base), so run it on a workstation, not the robot.
font:
	./genfont.py > font.h

# For installing by hand; the recipe does its own install, putting the unit in
# ${systemd_system_unitdir} (/usr/lib/systemd/system) where a package belongs.
install: oledd
	install -Dm755 oledd $(DESTDIR)$(prefix)/bin/oledd
	install -Dm644 oledd.service $(DESTDIR)/etc/systemd/system/oledd.service
	install -Dm644 i2c-dev.conf $(DESTDIR)/etc/modules-load.d/i2c-dev.conf

clean:
	rm -f oledd

.PHONY: font install clean
