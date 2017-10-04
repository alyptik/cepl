#
# cepl - C Read-Eval-Print Loop
#
# AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
# See LICENSE.md file for copyright and license details.

all: $(TARGET)
	$(MAKE) check

# user configuration
MKCFG := config.mk
# if previously built with `-fsanitize=address` we have to use `DEBUG` flags
OPT != test -f debug.mk
ifeq ($(.SHELLSTATUS),0)
	OLVL = $(DEBUG)
endif
-include $(DEP) $(MKCFG)
.PHONY: all check clean debug dist install test uninstall $(MKALL)

debug:
	# debug indicator flag
	@touch debug.mk
	$(MAKE) check OLVL="$(DEBUG)"

$(TARGET): %: $(OBJ)
	$(LD) $(LDFLAGS) $(OLVL) $(LIBS) $^ -o $@
$(TEST): %:  %.o $(OBJ)
	$(LD) $(LDFLAGS) $(OLVL) $(LIBS) $(TAP).o $(<:t/test%=src/%) $< -o $@
%.d %.o: %.c
	$(CC) $(CFLAGS) $(OLVL) $(CPPFLAGS) -c $< -o $@

test check: $(TOBJ) $(TEST)
	./t/testcompile
	./t/testhist
	./t/testparseopts
	echo "test string" | ./t/testreadline
	./t/testvars
clean:
	@echo "cleaning"
	@rm -fv $(DEP) $(TARGET) $(TEST) $(OBJ) $(TOBJ) $(TARGET).tar.gz
install: $(TARGET)
	@echo "installing"
	@mkdir -pv $(DESTDIR)$(PREFIX)/$(BINDIR)
	@mkdir -pv $(DESTDIR)$(PREFIX)/$(MANDIR)
	install -c $(TARGET) $(DESTDIR)$(PREFIX)/$(BINDIR)
	install -c $(MANPAGE) $(DESTDIR)$(PREFIX)/$(MANDIR)
uninstall:
	@rm -fv $(DESTDIR)$(PREFIX)/$(BINDIR)/$(TARGET)
	@rm -fv $(DESTDIR)$(PREFIX)/$(MANDIR)/$(MANPAGE)
dist: clean
	@echo "creating dist tarball"
	@mkdir -pv $(TARGET)/
	@cp -Rv LICENSE.md Makefile README.md $(HDR) $(SRC) $(TSRC) $(MANPAGE) $(TARGET)/
	tar -czf $(TARGET).tar.gz $(TARGET)/
	@rm -rfv $(TARGET)/
