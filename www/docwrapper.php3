<?php
#
# EMULAB-COPYRIGHT
# Copyright (c) 2000-2007 University of Utah and the Flux Group.
# All rights reserved.
#
require("defs.php3");

#
# Verify page arguments.
#
$reqargs = RequiredPageArguments("docname",    PAGEARG_STRING);
$optargs = OptionalPageArguments("printable",  PAGEARG_BOOLEAN);

#
# Need to sanity check the path! Allow only [word].html files
#
if (!preg_match("/^[-\w]+\.(html|txt)$/", $docname)) {
    header(' ', true, 400);
    USERERROR("Illegal document name: $docname!", 1);
}

$to_wiki = array(
  'hardware.html' => 'UtahHardware',
  'hardware-emulab.net.html' => 'UtahHardware',
  'auth.html' => 'Auth',
  'otheremulabs.html' => 'OtherEmulabs',
);

#
# Make sure the file exists
#
$fh = @fopen("$docname", "r");
if (!$fh) {
    if (isset ($to_wiki{$docname})) {
      $wikiname = $to_wiki{$docname};
      header("Location: $WIKIDOCURL/$wikiname", TRUE, 301);
      return 0;
    } else {
      header(' ', true, 404);
      USERERROR("Can't read document file: $docname!", 1);
    }
}

if (!isset($printable))
    $printable = 0;

#
# Standard Testbed Header
#
if (!$printable) {
    PAGEHEADER("Emulab Documentation");
}

if ($printable) {
    #
    # Need to spit out some header stuff.
    #
    echo "<html>
          <head>
  	  <link rel='stylesheet' href='tbstyle-plain.css' type='text/css'>
          </head>
          <body>\n";
}
else {
	echo "<b><a href=$REQUEST_URI&printable=1>
                 Printable version of this document</a></b><br>\n";
}

fpassthru($fh);
fclose($fh);

#
# Standard Testbed Footer
# 
if ($printable) {
    echo "</body>
          </html>\n";
}
else {
    PAGEFOOTER();
}
?>

