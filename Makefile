#
# cepl - C Read-Eval-Print Loop
#
# AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
# See LICENSE.md file for copyright and license details.

all:
	$(MAKE) $(TARGET) check

# user configuration
MKCFG := config.mk
export C_INCLUDE_PATH=$(READLINE)/include
# if previously built with `-fsanitize=address` we have to use `ASAN` flags
ASAN_OPT != test -f asan.mk
ifeq ($(.SHELLSTATUS),0)
	OLVL = $(ASAN)
endif
-include $(DEP) $(MKCFG)
.PHONY: all asan check clean debug dist install test uninstall $(MKALL)

asan:
	# asan indicator flag
	@touch asan.mk
	$(MAKE) clean
	$(MAKE) $(TARGET) check OLVL= CFLAGS="$(DEBUG_CFLAGS)" LDFLAGS="$(DEBUG_LDFLAGS)"
debug:
	$(MAKE) $(TARGET) check OLVL= CFLAGS="$(DEBUG_CFLAGS)" LDFLAGS="$(DEBUG_LDFLAGS)"

$(TARGET): %: $(OBJ)
	$(LD) $(LDFLAGS) $^ $(LIBS) $(ELF_LIBS) -o $@
$(TEST): %: %.o $(TAP).o $(OBJ) $(TOBJ)
	$(LD) $(LDFLAGS) $(TAP).o $(<:t/test%=src/%) $< $(LIBS) $(ELF_LIBS) -o $@
%.o: %.c $(HDR)
	$(CC) $(CFLAGS) $(OLVL) $(CPPFLAGS) -c $< -o $@

test check: $(TEST)
	./t/testcompile
	./t/testhist
	./t/testparseopts
	echo "test string" | ./t/testreadline
	./t/testvars
clean:
	@echo "cleaning"
	$(RM) $(DEP) $(TARGET) $(TEST) $(OBJ) $(TOBJ) $(TARGET).tar.gz asan.mk
install: $(TARGET)
	@echo "installing"
	mkdir -p $(DESTDIR)$(PREFIX)/$(BINDIR)
	mkdir -p $(DESTDIR)$(PREFIX)/$(MANDIR)
	mkdir -p $(DESTDIR)$(PREFIX)/$(COMPDIR)
	install -c $(TARGET) $(DESTDIR)$(PREFIX)/$(BINDIR)
	install -c $(MANPAGE) $(DESTDIR)$(PREFIX)/$(MANDIR)
	install -c $(COMPLETION) $(DESTDIR)$(PREFIX)/$(COMPDIR)
uninstall:
	$(RM) $(DESTDIR)$(PREFIX)/$(BINDIR)/$(TARGET)
	$(RM) $(DESTDIR)$(PREFIX)/$(MANDIR)/$(MANPAGE)
	$(RM) $(DESTDIR)$(PREFIX)/$(COMPDIR)/$(COMPLETION)
dist: clean
	@echo "creating dist tarball"
	mkdir -p $(TARGET)/
	cp -R LICENSE.md Makefile README.md $(HDR) $(SRC) $(TSRC) $(MANPAGE) $(TARGET)/
	tar -czf $(TARGET).tar.gz $(TARGET)/
	$(RM) -r $(TARGET)/
