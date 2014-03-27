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
 
class Directors extends BaculumAPI {
	public function get() {
		$directors = $this->getModule('bconsole')->getDirectors();
		if($directors->exitcode === 0) {
			$this->output = $directors->output;
			$this->error = BconsoleError::ERROR_NO_ERRORS;
		} else {
			$this->output = BconsoleError::MSG_ERROR_BCONSOLE_CONNECTION_PROBLEM;
			$this->error = BconsoleError::ERROR_BCONSOLE_CONNECTION_PROBLEM;
		}
	}
}
?>