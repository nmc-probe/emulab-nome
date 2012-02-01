<?php
#
# EMULAB-COPYRIGHT
# Copyright (c) 2000-2012 University of Utah and the Flux Group.
# All rights reserved.
#
include("defs.php3");

#
# Only known and logged in users can do this.
#
$this_user = CheckLoginOrDie();
$uid       = $this_user->uid();
$isadmin   = ISADMIN();

#
# Verify page arguments
#
$optargs = OptionalPageArguments("target_user", PAGEARG_USER,
				 "p12",  PAGEARG_BOOLEAN,
				 "ssh",  PAGEARG_BOOLEAN,
				 "pub",  PAGEARG_BOOLEAN);
if (!isset($p12)) {
    $p12 = 0;
}
if (!isset($ssh)) {
    $ssh = 0;
}
if (!isset($pub)) {
    $pub = 0;
}

# Default to current user if not provided.
if (!isset($target_user)) {
     $target_user = $this_user;
}

# Need these below
$target_uid = $target_user->uid();
$target_idx = $target_user->uid_idx();

#
# Only admin people can create SSL certs for another user.
#
if (!$isadmin && !$target_user->SameUser($this_user)) {
    USERERROR("You do not have permission to download SSL cert ".
	      "for $user!", 1);
}

if ($p12) {
    if ($fp = popen("$TBSUEXEC_PATH $target_uid nobody webspewcert", "r")) {
	header("Content-Type: application/octet-stream;".
	       "filename=\"emulab.p12\";");
	header("Content-Disposition: inline; filename=\"emulab.p12\";");
	header("Cache-Control: no-cache, must-revalidate");
	header("Pragma: no-cache");
#       header("Content-Type: application/x-x509-user-cert");
	while (!feof($fp) && connection_status() == 0) {
	    print(fread($fp, 1024));
	    flush();
	}
	$retval = pclose($fp);
	$fp = 0;
    }
    return;
}

$query_result =& $target_user->TableLookUp("user_sslcerts",
					   "cert,privkey,idx",
					   "encrypted=1 and revoked is null");

if (!mysql_num_rows($query_result)) {
    PAGEHEADER("Download SSL Certificate for $target_uid");
    USERERROR("There is no SSL Certificate for $target_uid!", 1);
}
$row  = mysql_fetch_array($query_result);
$cert = $row["cert"];
$key  = $row["privkey"];

if ($ssh) {
    $serial  = $row['idx'];
    $comment = "sslcert:${serial}";
    $pubkey_result =& $target_user->TableLookUp("user_pubkeys",
						"pubkey",
						"comment='$comment'");
    if (!mysql_num_rows($query_result)) {
	PAGEHEADER("Download SSL Certificate for $target_uid");
	USERERROR("There is no SSH pubkey for certificate!", 1);
    }
    $row  = mysql_fetch_array($pubkey_result);
    $pubkey = $row['pubkey'];
    
    header("Content-Type: text/plain");
    echo "-----BEGIN RSA PRIVATE KEY-----\n";
    echo $key;
    echo "-----END RSA PRIVATE KEY-----\n";
    # The user does not generally need this and it causes confusion.
    if ($pub) {
	echo $pubkey;
	echo "\n";
    }
}
else {
    header("Content-Type: text/plain");
    echo "-----BEGIN RSA PRIVATE KEY-----\n";
    echo $key;
    echo "-----END RSA PRIVATE KEY-----\n";
    echo "-----BEGIN CERTIFICATE-----\n";
    echo $cert;
    echo "-----END CERTIFICATE-----\n";
}

?>
