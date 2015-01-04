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
 
class Storages extends BaculumAPI {

	public function get() {
		$limit = intval($this->Request['limit']);
		$storages = $this->getModule('storage')->getStorages($limit);
		$allowedStorages = $this->getModule('bconsole')->bconsoleCommand($this->director, array('.storage'), $this->user)->output;
		$storagesOutput = array();
		foreach($storages as $storage) {
			if(in_array($storage->name, $allowedStorages)) {
				$storagesOutput[] = $storage;
			}
		}
		$this->output = $storagesOutput;
		$this->error = StorageError::ERROR_NO_ERRORS;
	}
}
?>
