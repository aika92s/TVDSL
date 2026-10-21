# Funny: лексер и парсер

```
lexer/   HW1: регулярки -> НКА -> ДКА -> минимальный ДКА, лексер, отчёт REPORT.md   (см. lexer/README.md)
parser/  HW2: LL(1)-парсер, AST, ошибки; использует лексер из ../lexer только на чтение (см. parser/README.md)
```

```sh
make lexer-test     # только лексер (пишет lexer/output/, lexer/REPORT.md)
make parser-test    # только парсер (пишет parser/REPORT_HW2.md, в lexer/ ничего не трогает)
make test           # оба
```
