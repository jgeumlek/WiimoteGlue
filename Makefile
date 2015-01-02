LINK_LIBS += -ludev
LINK_LIBS += -lxwiimote
EXTRA_CFLAGS += $CONFIG_FLAGS

keycodefix:
	cp src/key_codes.c ./key_codes.c.old
	./build_util/generate_key_codes
	mv key_codes.c src/key_codes.c

basickeys:
	$(MAKE) wiimoteglue CONFIG_FLAGS=-DNO_EXTRA_KEYBOARD_KEYS

wiimoteglue:
default:
	$(CC) -o wiimoteglue $(EXTRA_CFLAGS) $(LINK_LIBS) src/*.c

