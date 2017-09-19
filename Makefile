#
# Copyright (C) 2014 Guido Berhoerster <gber@opensuse.org>
#
# Licensed under the GNU General Public License Version 2
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#

PACKAGE =	pk-update-icon
APP_NAME =	org.opensuse.pk-update-icon
VERSION =	2
DISTNAME :=	$(PACKAGE)-$(VERSION)

# gcc, clang, icc
MAKEDEPEND.c =	$(CC) -MM $(CFLAGS) $(CPPFLAGS)
# Sun/Solaris Studio
#MAKEDEPEND.c =	$(CC) -xM1 $(CFLAGS) $(CPPFLAGS)
# X makedepend
#MAKEDEPEND.c =	makedepend -f- -Y -- $(CFLAGS) $(CPPFLAGS) --
INSTALL :=	install
INSTALL.exec :=	$(INSTALL) -D -m 0755
INSTALL.data :=	$(INSTALL) -D -m 0644
PAX :=		pax
GZIP :=		gzip
SED :=		sed
MSGFMT :=	msgfmt
INTLTOOL_UPDATE :=	intltool-update
INTLTOOL_MERGE :=	intltool-merge
XSLTPROC :=	xsltproc
DOCBOOK5_MANPAGES_STYLESHEET =	http://docbook.sourceforge.net/release/xsl-ns/current/manpages/docbook.xsl
UPDATE_VIEWER_COMMAND =	/usr/bin/gpk-update-viewer

define generate-manpage-rule =
%.$(1): %.$(1).xml
	$$(XSLTPROC) \
	    --xinclude \
	    --stringparam package $$(PACKAGE) \
	    --stringparam version $$(VERSION)\
	    docbook-update-source-data.xsl $$< | \
	    $$(XSLTPROC) \
	    --xinclude \
	    $$(DOCBOOK5_MANPAGES_FLAGS) \
	    --output $$@ \
	    $$(DOCBOOK5_MANPAGES_STYLESHEET) \
	    -
endef

DESTDIR ?=
prefix ?=	/usr/local
bindir ?=	$(prefix)/bin
datadir ?=	$(prefix)/share
mandir ?=	$(datadir)/man
localedir ?=	$(datadir)/locale
sysconfdir ?=	/etc
xdgautostartdir ?=	$(sysconfdir)/xdg/autostart

OBJS =		main.o pkui-icon.o pkui-backend.o
MANPAGES =	$(PACKAGE).1
AUTOSTART_FILE =	$(PACKAGE).desktop
MOFILES :=	$(patsubst %.po,%.mo,$(wildcard po/*.po))
POTFILE =	po/$(PACKAGE).pot
CPPFLAGS := 	$(shell pkg-config --cflags gtk+-2.0 unique-1.0 libnotify packagekit-glib2) \
		-DI_KNOW_THE_PACKAGEKIT_GLIB2_API_IS_SUBJECT_TO_CHANGE \
		-DPACKAGE="\"$(PACKAGE)\"" \
		-DAPP_NAME=\"$(APP_NAME)\" \
		-DVERSION=\"$(VERSION)\" \
		-DLOCALEDIR="\"$(localedir)\"" \
		-DUPDATE_VIEWER_COMMAND="\"$(UPDATE_VIEWER_COMMAND)\""
LDLIBS :=	$(shell pkg-config --libs gtk+-2.0 unique-1.0 libnotify packagekit-glib2)
DOCBOOK5_MANPAGES_FLAGS =	--stringparam man.authors.section.enabled 0 \
				--stringparam man.copyright.section.enabled 0

.DEFAULT_TARGET = all

.PHONY: all clean clobber dist install

all: $(PACKAGE) $(MANPAGES) $(MOFILES) $(AUTOSTART_FILE)

$(PACKAGE): $(OBJS)
	$(LINK.o) $^ $(LDLIBS) -o $@

$(POTFILE): po/POTFILES.in
	cd po/ && $(INTLTOOL_UPDATE) --pot --gettext-package="$(PACKAGE)"

pot: $(POTFILE)

update-po: $(POTFILE)
	cd po/ && for lang in $(patsubst po/%.mo,%,$(MOFILES)); do \
		$(INTLTOOL_UPDATE) --dist --gettext-package="$(PACKAGE)" \
				   $${lang}; \
	done

%.o: %.c
	$(MAKEDEPEND.c) $< | $(SED) -f deps.sed >$*.d
	$(COMPILE.c) -o $@ $<

$(foreach section,1 2 3 4 5 6 7 8 9,$(eval $(call generate-manpage-rule,$(section))))

%.desktop: %.desktop.in $(MOFILES)
	$(INTLTOOL_MERGE) --desktop-style --utf8 po $< $@

%.mo: %.po
	$(MSGFMT) -o $@ $<

install:
	$(INSTALL.exec) $(PACKAGE) $(DESTDIR)$(bindir)/$(PACKAGE)
	$(INSTALL.data) $(AUTOSTART_FILE) \
			$(DESTDIR)$(xdgautostartdir)/$(AUTOSTART_FILE)
	for lang in $(patsubst po/%.mo,%,$(MOFILES)); do \
		$(INSTALL.data) po/$${lang}.mo \
			$(DESTDIR)$(localedir)/$${lang}/LC_MESSAGES/$(PACKAGE).mo; \
	done
	$(INSTALL.data) $(PACKAGE).1 \
			$(DESTDIR)$(mandir)/man1/$(PACKAGE).1

clean:
	rm -f $(PACKAGE) $(OBJS) $(POTFILE) $(MOFILES) $(MANPAGES) $(AUTOSTART_FILE)

clobber: clean
	rm -f $(patsubst %.o,%.d,$(OBJS))

dist: clobber
	$(PAX) -w -x ustar -s ',.*/\..*,,' -s ',./[^/]*\.tar\.gz,,' \
	    -s ',\./,$(DISTNAME)/,' . | $(GZIP) > $(DISTNAME).tar.gz

-include $(patsubst %.o,%.d,$(OBJS))
