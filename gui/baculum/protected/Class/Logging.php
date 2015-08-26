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

class Logging extends TModule {

	private $debugEnabled = false;

	const CATEGORY_EXECUTE = 'Execute';
	const CATEGORY_EXTERNAL = 'External';
	const CATEGORY_APPLICATION = 'Application';
	const CATEGORY_GENERAL = 'General';
	const CATEGORY_SECURITY = 'Security';

	public function init($config) {
		$this->debugEnabled = $this->isDebugOn();
	}

	public function isDebugOn() {
		$isDebug = false;
		$cfgModule = $this->Application->getModule('configuration');
		if($cfgModule->isApplicationConfig() === true) {
			$config = $cfgModule->getApplicationConfig();
			$isDebug = (isset($config['baculum']['debug']) && intval($config['baculum']['debug']) === 1);
		} else {
			$isDebug = true;
		}
		return $isDebug;
	}

	public function enableDebug($enable) {
		$result = false;
		$cfgModule = $this->Application->getModule('configuration');
		if($cfgModule->isApplicationConfig() === true) {
			$config = $cfgModule->getApplicationConfig();
			$config['baculum']['debug'] = ($enable === true) ? "1" : "0";
			$result = $cfgModule->setApplicationConfig($config);
		}
		return $result;
	}

	private function getLogCategories() {
		$categories = array(
			self::CATEGORY_EXECUTE,
			self::CATEGORY_EXTERNAL,
			self::CATEGORY_APPLICATION,
			self::CATEGORY_GENERAL,
			self::CATEGORY_SECURITY
		);
		return $categories;
	}

	public function log($cmd, $output, $category, $file, $line) {
		if($this->debugEnabled !== true) {
			return;
		}

		if(!in_array($category, $this->getLogCategories())) {
			$category = self::CATEGORY_SECURITY;
		}

		$log = sprintf(
			'Command=%s, Output=%s, File=%s, Line=%d',
			$cmd,
			print_r($output, true),
			$file,
			intval($line)
		);

		Prado::trace($log, $category);
	}
}

?>