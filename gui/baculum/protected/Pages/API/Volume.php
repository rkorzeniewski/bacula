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
 
class Volume extends BaculumAPI {
	public function get() {
		$mediaid = intval($this->Request['id']);
		$volume = $this->getModule('volume')->getVolumeById($mediaid);
		if(!is_null($volume)) {
			$this->output = $volume;
			$this->error = VolumeError::ERROR_NO_ERRORS;
		} else {
			$this->output = VolumeError::MSG_ERROR_VOLUME_DOES_NOT_EXISTS;
			$this->error = VolumeError::ERROR_VOLUME_DOES_NOT_EXISTS;
		}
	}
	
	public function set($id, $params) {
		$result = ($this->user === null) ? $this->getModule('volume')->setVolume($id, $params) : true;
		if($result === true) {
			$this->output = null;
			$this->error = VolumeError::ERROR_NO_ERRORS;
		} else {
			$this->output = DatabaseError::MSG_ERROR_WRITE_TO_DB_PROBLEM;
			$this->error = DatabaseError::ERROR_WRITE_TO_DB_PROBLEM;
		}
	}
}

?>