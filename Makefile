#
# cepl - C Read-Eval-Print Loop
#
# AUTHOR: Joey Pabalinas <joeypabalinas@gmail.com>
# See LICENSE.md file for copyright and license details.

all:
	$(MAKE) $(TARGET) check

# user configuration
MKCFG := config.mk
export C_INCLUDE_PATH=$(READLINE)/include
-include $(DEP) $(MKCFG)
.PHONY: all check clean debug dist install test uninstall

debug:
	$(MAKE) $(TARGET) check OLVL= CFLAGS="$(DEBUG_CFLAGS)" LDFLAGS="$(DEBUG_LDFLAGS)"

$(TARGET): %: $(OBJ)
	$(LD) $(LDFLAGS) $^ $(LDLIBS) -o $@
$(TEST): %: %.o $(OBJ) $(TOBJ) $(COBJ)
	$(LD) $(LDFLAGS) contrib/libtap/tap.o $(<:t/test%=src/%) $< $(LDLIBS) -o $@
%.o: %.c $(HDR)
	$(CC) $(CFLAGS) $(OLVL) $(CPPFLAGS) -c $< -o $@

test check: $(TEST)
	@echo "[running unit tests]"
	./t/testcompile
	./t/testhist
	./t/testparseopts
	echo "test string" | ./t/testreadline
	./t/testvars
clean:
	@echo "[cleaning]"
	$(RM) $(DEP) $(SU) $(TARGET) $(TEST) $(OBJ) $(TOBJ) $(COBJ) \
		cepl-$(shell sed '1!d; s/.*\([0-9]\).*\([0-9]\).*\([0-9]\).*/\1.\2.\3/' cepl.1).tar.gz \
		cscope.* tags TAGS
install: $(TARGET)
	@echo "[installing]"
	mkdir -p $(DESTDIR)$(PREFIX)/$(BINDIR)
	mkdir -p $(DESTDIR)$(PREFIX)/$(MANDIR)
	mkdir -p $(DESTDIR)$(PREFIX)/$(COMPDIR)
	install -c $(TARGET) $(DESTDIR)$(PREFIX)/$(BINDIR)
	install -c $(MANPAGE) $(DESTDIR)$(PREFIX)/$(MANDIR)
	cat $(COMPLETION) > $(DESTDIR)$(PREFIX)/$(COMPDIR)/$(COMPLETION)
uninstall:
	@echo "[uninstalling]"
	$(RM) $(DESTDIR)$(PREFIX)/$(BINDIR)/$(TARGET)
	$(RM) $(DESTDIR)$(PREFIX)/$(MANDIR)/$(MANPAGE)
	$(RM) $(DESTDIR)$(PREFIX)/$(COMPDIR)/$(COMPLETION)
dist:
	@echo "[creating source archive]"
	tar cf cepl-$(shell sed '1!d; s/.*\([0-9]\).*\([0-9]\).*\([0-9]\).*/\1.\2.\3/' cepl.1).tar \
		LICENSE Makefile README.md _cepl cepl.1 cepl.gif cepl.json config.mk \
		src t contrib
	gzip cepl-$(shell sed '1!d; s/.*\([0-9]\).*\([0-9]\).*\([0-9]\).*/\1.\2.\3/' cepl.1).tar
cscope:
	@echo "[creating cscope database]"
	$(RM) cscope.*
	cscope -Rub
tags TAGS:
	@echo "[creating ctags file]"
	$(RM) tags TAGS
	ctags -Rf $@ --tag-relative=yes --langmap=c:+.h.C.H --fields=+l --c-kinds=+p --c++-kinds=+p --fields=+iaS --extras=+q .
