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
 
class StorageShow extends BaculumAPI {
	public function get() {
		$storageid = intval($this->Request['id']);
		$storage = $this->getModule('storage')->getStorageById($storageid);
		if(!is_null($storage)) {
			$storageShow = $this->getModule('bconsole')->bconsoleCommand($this->director, array('show', 'storage="' . $storage->name . '"'), $this->user);
			$this->output = $storageShow->output;
			$this->error = (integer)$storageShow->exitcode;
		} else {
			$this->output = StorageError::MSG_ERROR_STORAGE_DOES_NOT_EXISTS;
			$this->error = StorageError::ERROR_STORAGE_DOES_NOT_EXISTS;
		}
	}
}

?>