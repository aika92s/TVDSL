# Корневой Makefile: только делегирует в lexer/ и parser/
all test clean:
	$(MAKE) -C lexer $@
	$(MAKE) -C parser $@

lexer-%:
	$(MAKE) -C lexer $*

parser-%:
	$(MAKE) -C parser $*

.PHONY: all test clean
