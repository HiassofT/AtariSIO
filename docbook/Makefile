XMLTO=xmlto

all: docs-man docs-html docs-txt

docs-man:
	mkdir -p man
	$(XMLTO) -o man -m man.xsl man AtariSIO.xml
	
docs-html:
	mkdir -p html
	$(XMLTO) -o html -m html.xsl html AtariSIO.xml
	
docs-txt:
	mkdir -p txt
	$(XMLTO) -o txt -x /usr/share/xml/docbook/stylesheet/nwalsh/html/docbook.xsl -m html.xsl txt AtariSIO.xml
	
