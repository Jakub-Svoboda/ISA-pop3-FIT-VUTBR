Testovací skript na POP3 server v ISA
=====================================

Několik málo testů na POP3 server

Použití
-------

1. Pokud není váš `popser` přeložený, přeložte ho.
2. Umístěte složku s testovacím skriptem do složky s projektem tak, aby
    se binární soubor `popser` nacházel v nadřazeném adresáři od adresáře
    s testovacím skriptem, nebo upravte cestu k binárce v souboru `test.py`.
3. Server není třeba spouštět, testovací skript si ho pustí sám.
4. Spusťte testovací skript:
        
        ./test.py
        
---

Nebo lze místo kroků 1 a 4 použít `./make-and-test.sh`.

Co to netestuje
---------------

Protože se mi s nad tím nechtělo trávit více času, je dost věcí, které
tento skript netestuje. Mimo jiné mezi ně patří:

* To, jakým způsobem server přesouvá maily mezi `new/` a `cur/`
* Posílání příliš velkého mailu
* Co server udělá, pokud mu pošlete zprávu, která nekončí `\r\n`
