XMLTO=xmlto

all: docs-man

docs-man:
	mkdir -p man
	$(XMLTO) -o man -m man.xsl man AtariSIO.xml
	
