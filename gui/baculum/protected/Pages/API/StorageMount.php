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
 
class StorageMount extends BaculumAPI {
	public function get() {
		$storageid = intval($this->Request['id']);
		$drive = intval($this->Request['drive']);
		$slot = intval($this->Request['slot']);
		$storage = $this->getModule('storage')->getStorageById($storageid);
		if(!is_null($storage)) {
			$storageMount = $this->getModule('bconsole')->bconsoleCommand($this->director, array('mount', 'storage="' . $storage->name . '"', 'drive=' . $drive, 'slot=' . $slot));
			$this->output = $storageMount->output;
			$this->error = (integer)$storageMount->exitcode;
		} else {
			$this->output = StorageError::MSG_ERROR_STORAGE_DOES_NOT_EXISTS;
			$this->error = StorageError::ERROR_STORAGE_DOES_NOT_EXISTS;
		}
		
	}
}

?>