DESTDIR=/
PREFIX=/usr/
SYSTEMD_UNIT_DIR=$(shell pkg-config --silence-errors --variable=systemduserunitdir systemd)

CFLAGS=-Wall `pkg-config --cflags xcb-randr xcb`
LDFLAGS=`pkg-config --libs xcb-randr xcb`

.PHONY: install_launcher uninstall_launcher install_service uninstall_service clean

launcher: launcher.c
	cc $< ${CFLAGS} -o $@ ${LDFLAGS}

# Rules for launcher
install_launcher:
	install -D --mode=755 launcher ${DESTDIR}${PREFIX}/bin/autorandr_launcher

uninstall_launcher:
	$(RM) ${DESTDIR}${PREFIX}/bin/autorandr_launcher

install_service: install_launcher ${SYSTEMD_UNIT_DIR}
	install -D --mode=644 launcher.service ${DESTDIR}/${SYSTEMD_UNIT_DIR}/autorandr_launcher.service
	sed -i "s:<autorandr_launcher>:${DESTDIR}${PREFIX}/bin/autorandr_launcher:" ${DESTDIR}/${SYSTEMD_UNIT_DIR}/autorandr_launcher.service

uninstall_service: uninstall_launcher
	$(RM) ${DESTDIR}/${SYSTEMD_UNIT_DIR}/autorandr_launcher.service

install: install_service
uninstall: uninstall_service
clean:
	$(RM) *.o launcher
