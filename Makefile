# Корневой Makefile: только делегирует в лабораторные каталоги.
all test clean:
	$(MAKE) -C lexer $@
	$(MAKE) -C parser $@
	$(MAKE) -C sema $@
	$(MAKE) -C ir $@

lexer-%:
	$(MAKE) -C lexer $*

parser-%:
	$(MAKE) -C parser $*

sema-%:
	$(MAKE) -C sema $*

ir-%:
	$(MAKE) -C ir $*

.PHONY: all test clean
