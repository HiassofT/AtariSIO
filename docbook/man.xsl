<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version='1.0'>

<!-- disable output of AUTHORS and COPYRIGHT sections -->
<xsl:param name="man.authors.section.enabled">0</xsl:param>
<xsl:param name="man.copyright.section.enabled">0</xsl:param>

<!-- no endnotes -->
<xsl:param name="man.endnotes.list.enabled">0</xsl:param>

<!-- enable version profiling to get rid of annoying warnings -->
<xsl:param name="refentry.version.profile.enabled">1</xsl:param>

<!-- use title from referenceinfo, not reference -->
<xsl:param name="refentry.manual.profile.enabled">1</xsl:param>
<xsl:param name="refentry.manual.profile">
  (($info[//title])[last()]/title)[1]
</xsl:param>


<xsl:param name="man.th.title.max.length">40</xsl:param>

</xsl:stylesheet>

