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
 
class FileSet extends BaculumAPI {
	public function get() {
		$filesetid = intval($this->Request['id']);
		$fileset = $this->getModule('fileset')->getFileSetById($filesetid);
		$allowedFileSets = $this->getModule('bconsole')->bconsoleCommand($this->director, array('.fileset'), $this->user)->output;
		if(!is_null($fileset) && in_array($fileset->fileset, $allowedFileSets)) {
			$this->output = $fileset;
			$this->error = FileSetError::ERROR_NO_ERRORS;
		} else {
			$this->output = FileSetError::MSG_ERROR_FILESET_DOES_NOT_EXISTS;
			$this->error = FileSetError::ERROR_FILESET_DOES_NOT_EXISTS;
		}
	}
}

?>