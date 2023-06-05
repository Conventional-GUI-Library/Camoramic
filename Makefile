CC ?= cc
SOURCES = $(wildcard src/*.c)
PKGCONFIG = $(shell which pkg-config)
CFLAGS = -Wno-deprecated-declarations -ggdb3 $(shell $(PKGCONFIG) --cflags gtk+-3.0 gthread-2.0 gstreamer-video-1.0 libcanberra alsa)
LIBS = $(shell $(PKGCONFIG) --libs gtk+-3.0 gthread-2.0 gstreamer-video-1.0 libcanberra alsa) -lm
PACKAGE = camoramic

all: ${PACKAGE} po/tr_TR/$(PACKAGE).mo  po/it_IT/$(PACKAGE).mo

camoramic: $(SOURCES)
	$(CC) $(^) -o ${PACKAGE} ${CFLAGS} ${LIBS}

po/tr_TR/$(PACKAGE).mo: po/tr_TR/$(PACKAGE).po
	msgfmt $(^) --output-file=$@ 

po/it_IT/$(PACKAGE).mo: po/it_IT/$(PACKAGE).po
	msgfmt $(^) --output-file=$@ 
	
clean:
	rm -f ${PACKAGE}
	rm -f po/tr_TR/$(PACKAGE).mo
	rm -f po/it_IT/$(PACKAGE).mo

uninstall:
	rm -f /usr/local/bin/camoramic
	rm -f /usr/share/glib-2.0/schemas/org.tga.camoramic.gschema.xml
	rm -f /usr/share/locale/tr_TR/LC_MESSAGES/$(PACKAGE).mo
	rm -f /usr/share/applications/camoramic.desktop
	rm -f /usr/share/locale/it_IT/LC_MESSAGES/$(PACKAGE).mo
	
install: all
	install -Dm0755 camoramic /usr/local/bin/camoramic
	install -Dm0755 res/camoramic.desktop /usr/share/applications/camoramic.desktop
	install -Dm0644 res/org.tga.camoramic.gschema.xml /usr/share/glib-2.0/schemas/org.tga.camoramic.gschema.xml
	install -Dm0644 po/tr_TR/$(PACKAGE).mo /usr/share/locale/tr/LC_MESSAGES/$(PACKAGE).mo
	install -Dm0644 po/it_IT/$(PACKAGE).mo /usr/share/locale/it_IT/LC_MESSAGES/$(PACKAGE).mo
	glib-compile-schemas /usr/share/glib-2.0/schemas/

	
.PHONY: clean install uninstall
