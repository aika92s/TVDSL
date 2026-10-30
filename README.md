# Funny: лексер, парсер, семантика

```
lexer/   HW1: регулярки -> НКА -> ДКА -> минимальный ДКА, лексер, отчёт REPORT.md   (см. lexer/README.md)
parser/  HW2: LL(1)-парсер, AST, ошибки; использует лексер из ../lexer только на чтение (см. parser/README.md)
sema/    HW3: имена, типы, диагностика, аннотированный AST; использует ../parser и ../lexer только на чтение (см. sema/README.md)
```

```sh
make lexer-test     # только лексер (пишет lexer/output/, lexer/REPORT.md)
make parser-test    # только парсер (пишет parser/REPORT_HW2.md, в lexer/ ничего не трогает)
make sema-test      # только семантика (пишет sema/REPORT_HW3.md)
make test           # все три
```
