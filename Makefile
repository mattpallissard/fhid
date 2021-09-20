BIN_PREFIX=~
make:
	$(CC) -o fhid src/concept.c

install:
	mkdir -p $(BIN_PREFIX)/bin
	install -m 0750 fhid $(BIN_PREFIX)/bin/
	install -m 0750 bin/click $(BIN_PREFIX)/bin/
	install -m 0640 config/systemd/user/fhid.service ~/.config/systemd/user/
	systemctl --user daemon-reload
	systemctl --user enable fhid.service
	$(info 'make sure $(BIN_PREFIX)/bin is added to your PATH')

udev:
	install -m 0644 etc/udev/rules.d/10-fhid.rules /etc/udev/rules.d/
	install -m 0644 etc/modules-load.d/fhid.conf /etc/modules-load.d/
	udevadm control --reload-rules


start:
	systemctl --user restart fhid.service

clean:
	unlink fhid
