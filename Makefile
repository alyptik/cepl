#
# cepl - a C and C++ REPL
#
# AUTHOR: Joey Pabalinas <joeypabalinas@gmail.com>
# See LICENSE file for copyright and license details.

all:
	$(MAKE) $(TARGET)

# user configuration
MKCFG := config.mk
-include $(DEP) $(MKCFG)
.PHONY: all clean debug dist install uninstall

debug:
	CFLAGS="$(DEBUG_CFLAGS)" LDFLAGS="$(DEBUG_LDFLAGS)" $(MAKE) $(TARGET)

$(TARGET): %: $(OBJ)
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@
%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	@echo "[cleaning]"
	$(RM) $(TARGET) $(OBJ) $(DEP) cscope.* tags TAGS \
		cepl-$(shell sed '1!d; s/.*\([0-9]\).*\([0-9]\).*\([0-9]\).*/\1.\2.\3/' cepl.1).tar.gz
install: $(TARGET)
	@echo "[installing]"
	mkdir -p $(DESTDIR)$(PREFIX)/$(BINDIR)
	mkdir -p $(DESTDIR)$(PREFIX)/$(MANDIR)
	mkdir -p $(DESTDIR)$(PREFIX)/$(COMPDIR)
	install -c $(TARGET) $(DESTDIR)$(PREFIX)/$(BINDIR)
	bzip2 -c $(MANPAGE) > $(DESTDIR)$(PREFIX)/$(MANDIR)/$(MANPAGE).bz2
	cat $(COMPLETION) > $(DESTDIR)$(PREFIX)/$(COMPDIR)/$(COMPLETION)
uninstall:
	@echo "[uninstalling]"
	$(RM) $(DESTDIR)$(PREFIX)/$(BINDIR)/$(TARGET)
	$(RM) $(DESTDIR)$(PREFIX)/$(MANDIR)/$(MANPAGE).bz2
	$(RM) $(DESTDIR)$(PREFIX)/$(COMPDIR)/$(COMPLETION)
dist: clean
	@echo "[creating source tarball]"
	tar cf cepl-$(shell sed '1!d; s/.*\([0-9]\).*\([0-9]\).*\([0-9]\).*/\1.\2.\3/' cepl.1).tar \
		LICENSE Makefile README.md _cepl cepl.1 cepl.gif cepl.json config.mk src
	gzip cepl-$(shell sed '1!d; s/.*\([0-9]\).*\([0-9]\).*\([0-9]\).*/\1.\2.\3/' cepl.1).tar
cscope:
	@echo "[creating cscope database]"
	$(RM) cscope.*
	cscope -Rub
tags TAGS:
	@echo "[creating tag file]"
	$(RM) tags TAGS
	ctags -Rf $@ --tag-relative=yes --langmap=c:+.h.C.H --fields=+l --c-kinds=+p --c++-kinds=+p --fields=+iaS --extras=+q .
