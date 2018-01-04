#
# cepl - C Read-Eval-Print Loop
#
# AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
# See LICENSE.md file for copyright and license details.

all:
	$(MAKE) $(TARGET) check

# user configuration
MKCFG := config.mk
# if previously built with `-fsanitize=address` we have to use `ASAN` flags
OPT != test -f asan.mk
ifeq ($(.SHELLSTATUS),0)
	OLVL = $(DEBUG) $(ASAN)
endif
-include $(DEP) $(MKCFG)
.PHONY: all asan check clean debug dist install test uninstall $(MKALL)

asan:
	# asan indicator flag
	@touch asan.mk
	$(MAKE) clean
	$(MAKE) $(TARGET) check
debug:
	$(MAKE) clean
	$(MAKE) $(TARGET) check OLVL="$(DEBUG)"

$(TARGET): %: $(OBJ) $(HDR)
	$(LD) $(LDFLAGS) $(OLVL) $(LIBS) $^ -o $@
$(TEST): %: %.o $(TAP).o $(OBJ) $(HDR)
	$(LD) $(LDFLAGS) $(OLVL) $(LIBS) $(TAP).o $(<:t/test%=src/%) $< -o $@
%.d %.o: %.c $(HDR)
	$(CC) $(CFLAGS) $(OLVL) $(CPPFLAGS) -c $< -o $@

test check: $(TOBJ) $(TEST)
	./t/testcompile
	./t/testhist
	./t/testparseopts
	echo "test string" | ./t/testreadline
	./t/testvars
clean:
	@echo "cleaning"
	@rm -fv $(DEP) $(TARGET) $(TEST) $(OBJ) $(TOBJ) $(TARGET).tar.gz asan.mk
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
