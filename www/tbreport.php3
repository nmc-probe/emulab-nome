<?php
#
# Copyright (c) 2000-2012 University of Utah and the Flux Group.
# 
# {{{EMULAB-LICENSE
# 
# This file is part of the Emulab network testbed software.
# 
# This file is free software: you can redistribute it and/or modify it
# under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or (at
# your option) any later version.
# 
# This file is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
# License for more details.
# 
# You should have received a copy of the GNU Affero General Public License
# along with this file.  If not, see <http://www.gnu.org/licenses/>.
# 
# }}}
#

#
# tbreport.php3  generate testbed usage report (frontend for testbed-report)
# 01-Nov-2014  chuck@ece.cmu.edu
#

include("defs.php3");

#
# Only known and logged in users can end experiments.
#
$this_user = CheckLoginOrDie();
$uid       = $this_user->uid();
$isadmin   = ISADMIN();

$reportbin = "/usr/testbed/local/testbed-report";

if (! $isadmin) {
    USERERROR("You do not have permission to view the testbed report!", 1);
}

#
# Verify page arguments.
#
$optargs = OptionalPageArguments("start",	      PAGEARG_STRING,
				 "end",	              PAGEARG_STRING,
                                 "loglong",           PAGEARG_STRING,
                                 "logshort",          PAGEARG_STRING,
                                 "prior",             PAGEARG_STRING,
                                 "running",           PAGEARG_STRING,
                                 "useproj",           PAGEARG_STRING,
                                 "usesize",           PAGEARG_STRING,
                                 "useuser",           PAGEARG_STRING);

#
# Standard Testbed Header
#
PAGEHEADER("Testbed Report Generator");

$start = (isset($start)) ? $start : "yesterday";
$end = (isset($end)) ? $end : "now";
$loglong = (isset($loglong)) ? $loglong : "Nope";
$logshort = (isset($logshort)) ? $logshort : "Nope";
$prior = (isset($prior)) ? $prior : "Nope";
$running = (isset($running)) ? $running : "Nope";
$useproj = (isset($useproj)) ? $useproj : "Nope";
$usesize = (isset($usesize)) ? $usesize : "Nope";
$useuser = (isset($useuser)) ? $useuser : "Nope";

echo "<form action=tbreport.php3 method=post>
      <B>Start date:</B>
      <input type=text
             name=start
             value=\"$start\"
             size=20
             maxlength=50 /><br>
      <B>End date:</B>
      <input type=text
             name=end
             value=\"$end\"
             size=20
             maxlength=50 /><br>";
echo "(dates can be relative: 'yesterday' '1 month ago' or absolute: 
        '14-Mar-2014 2am')<br><br>";
      
echo "<B>Reports:</B>";

$checked = ($useproj == "Yep") ? "checked" : "";
echo "<input $checked type=checkbox value=Yep name=useproj>use by project";

$checked = ($useuser == "Yep") ? "checked" : "";
echo "<input $checked type=checkbox value=Yep name=useuser>use by user";

$checked = ($usesize == "Yep") ? "checked" : "";
echo "<input $checked type=checkbox value=Yep name=usesize>use by exp size";

$checked = ($prior == "Yep") ? "checked" : "";
echo "<input $checked type=checkbox value=Yep name=prior>prior exps";

$checked = ($running == "Yep") ? "checked" : "";
echo "<input $checked type=checkbox value=Yep name=running>still running exps";

$checked = ($loglong == "Yep") ? "checked" : "";
echo "<input $checked type=checkbox value=Yep name=loglong>long log";

$checked = ($logshort == "Yep") ? "checked" : "";
echo "<input $checked type=checkbox value=Yep name=logshort>short log";

echo "<br><input type=submit name=search value=Generate>
      </form><p>";

$fmt = "h";

if ($useproj == "Yep") {
    $fmt = "$fmt,up";
}
if ($useuser == "Yep") {
    $fmt = "$fmt,uu";
}
if ($usesize == "Yep") {
    $fmt = "$fmt,us";
}
if ($prior == "Yep") {
    $fmt = "$fmt,p";
}
if ($running == "Yep") {
    $fmt = "$fmt,r";
}
if ($loglong == "Yep") {
    $fmt = "$fmt,ll";
}
if ($logshort == "Yep") {
    $fmt = "$fmt,ls";
}

if ($fmt != "h") {
    #$fmt = substr($fmt, 1);   # remove the leading comma
    $estart = escapeshellarg($start);
    $eend = escapeshellarg($end);
    echo "<PRE>";
    system("$reportbin -f $fmt $estart $eend");
    echo "</PRE>";
}

#
# Standard Testbed Footer
# 
PAGEFOOTER();
?>

