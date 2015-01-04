<?php
/**
 * Bacula® - The Network Backup Solution
 * Baculum - Bacula web interface
 *
 * Copyright (C) 2013-2015 Marcin Haba
 *
 * The main author of Baculum is Marcin Haba.
 * The main author of Bacula is Kern Sibbald, with contributions from many
 * others, a complete list can be found in the file AUTHORS.
 *
 * You may use this file and others of this release according to the
 * license defined in the LICENSE file, which includes the Affero General
 * Public License, v3.0 ("AGPLv3") and some additional permissions and
 * terms pursuant to its AGPLv3 Section 7.
 *
 * Bacula® is a registered trademark of Kern Sibbald.
 */
date_default_timezone_set('UTC');

// Support for web servers which do not provide direct info about HTTP Basic auth to PHP superglobal $_SERVER array.
if(!isset($_SERVER['PHP_AUTH_USER']) && !isset($_SERVER['PHP_AUTH_PW'])) {
    list($_SERVER['PHP_AUTH_USER'], $_SERVER['PHP_AUTH_PW']) = explode(':', base64_decode(substr($_SERVER['HTTP_AUTHORIZATION'], 6)));
}

define('APPLICATION_DIRECTORY', __DIR__);

require_once('./protected/Pages/Requirements.php');
new Requirements(__DIR__);
require_once('./framework/prado.php');

$application=new TApplication;
Prado::using('Application.Portlets.BButton');
Prado::using('Application.Portlets.BActiveButton');
$application->run();
?>
