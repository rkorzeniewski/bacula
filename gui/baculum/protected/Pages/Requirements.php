<?php
/**
 * Bacula® - The Network Backup Solution
 * Baculum - Bacula web interface
 *
 * Copyright (C) 2013-2014 Marcin Haba
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
 
class Requirements {
	const ASSETS_DIR = 'assets';
	const DATA_DIR = 'protected/Data';
	const RUNTIME_DIR = 'protected/runtime';

	public function __construct($baseDir) {
		$this->validateEnvironment($baseDir);
	}

	private function validateEnvironment($baseDir) {
		$requirements = array();
		if(!is_writable(self::ASSETS_DIR)) {
			$requirements[] = 'Please make writable by the web server next directory: <b>' . $baseDir . '/' . self::ASSETS_DIR . '</b>';
		}

		if(!is_writable(self::DATA_DIR)) {
			$requirements[] = 'Please make writable by the web server next directory: <b>' . $baseDir . '/' . self::DATA_DIR . '</b>';
		}

		if(!is_writable(self::RUNTIME_DIR)) {
			$requirements[] = 'Please make writable by the web server next directory: <b>' . $baseDir . '/' . self::RUNTIME_DIR . '</b>';
		}

		if(!function_exists('curl_init') || !function_exists('curl_setopt') || !function_exists('curl_exec') || !function_exists('curl_close')) {
			$requirements[] = 'Please install <b>cURL PHP module</b>.';
		}

		if(!function_exists('bcmul') || !function_exists('bcpow')) {
			$requirements[] = 'Please install <b>BCMath PHP module</b>.';
		}

		if(!extension_loaded('pdo_pgsql') && !extension_loaded('pdo_mysql')) {
			$requirements[] = 'Please install <b>PDO (PHP Data Objects) PHP module for PostgreSQL or MySQL</b> depending on what database type you are using with Bacula.';
		}

		if(!function_exists('mb_strlen')) {
			$requirements[] = 'Please install <b>MB String PHP module</b> for support for multi-byte string handling to PHP.';
		}

		if(count($requirements) > 0) {
			echo '<html><body><h2>Baculum - Missing dependencies</h2><ul>';
			for($i = 0; $i < count($requirements); $i++) {
				echo '<li>' . $requirements[$i] . '</li>';
				
			}
			echo '</ul>';
			echo 'For run Baculum <u>please correct above requirements</u> and refresh this page in web browser.';
			echo '</body></html>';
			exit();
		}
	}
}
?>