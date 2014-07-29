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
 
class PoolUpdate extends BaculumAPI {
	public function get() {}

	public function set($id, $params) {
		$pool = $this->getModule('pool')->getPoolById($id);
		if(!is_null($pool)) {
			$poolUpdate = $this->getModule('bconsole')->bconsoleCommand($this->director, array('update', 'pool="' .  $pool->name . '"'), $this->user);
			$this->output = $poolUpdate->output;
			$this->error = (integer)$poolUpdate->exitcode;
		} else {
			$this->output = PoolError::MSG_ERROR_POOL_DOES_NOT_EXISTS;
			$this->error = PoolError::ERROR_POOL_DOES_NOT_EXISTS;
		}
	}
}

?>