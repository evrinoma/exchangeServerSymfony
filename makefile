TARGET = exchangeServer
PREFIX = /usr/bin
START = exchangeserver
CONFIG = /etc/init.d
SRC = exchangeserver.c

all: $(TARGET)
clean:
	rm -rf $(TARGET) *.o
$(TARGET): $(SRC)
	$(info ************ BUILD RELEASE VERSIOIN **********)
	gcc $(SRC) -o $(TARGET) -lpthread
install:
	$(info ************ INSTALL SERVER**********)
	install $(TARGET) $(PREFIX)
	cp $(START) $(CONFIG)/$(START)
uninstall:
	rm -rf $(PREFIX)/$(TARGET)
	rm -rf $(CONFIG)/$(START)

