LINK_LIBS += -ludev
LINK_LIBS += -lxwiimote
EXTRA_CFLAGS += $(CONFIG_FLAGS)

wiimoteglue: src/*.c
	$(CC) -o wiimoteglue $(EXTRA_CFLAGS) $(LINK_LIBS) src/*.c

keycodefix:
	if ! [ -e key_codes.c.old ] ; \
	  then cp src/key_codes.c ./key_codes.c.old ; \
	fi

	@echo ""

	./build_util/generate_key_codes $(INPUT_HEADER_PATH)
	mv key_codes.c src/key_codes.c

	@echo ""
	@echo "key_codes.c has been updated. Try building again?"

basickeys:
	$(MAKE) wiimoteglue CONFIG_FLAGS=-DNO_EXTRA_KEYBOARD_KEYS

clean:
	rm wiimoteglue


