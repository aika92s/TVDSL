# Корневой Makefile: только делегирует в lexer/, parser/ и sema/
all test clean:
	$(MAKE) -C lexer $@
	$(MAKE) -C parser $@
	$(MAKE) -C sema $@

lexer-%:
	$(MAKE) -C lexer $*

parser-%:
	$(MAKE) -C parser $*

sema-%:
	$(MAKE) -C sema $*

.PHONY: all test clean
