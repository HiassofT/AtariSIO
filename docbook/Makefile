XMLTO=xmlto

all: docs-man docs-html

docs-man:
	mkdir -p man
	$(XMLTO) -o man -m man.xsl man AtariSIO.xml
	
docs-html:
	mkdir -p html
	$(XMLTO) -o html -m html.xsl html AtariSIO.xml
	
