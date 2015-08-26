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
 
class Pools extends BaculumAPI {
	public function get() {
		$limit = intval($this->Request['limit']);
		$pools = $this->getModule('pool')->getPools($limit);
		$allowedPools = $this->getModule('bconsole')->bconsoleCommand($this->director, array('.pool'), $this->user)->output;
		$poolsOutput = array();
		foreach($pools as $pool) {
			if(in_array($pool->name, $allowedPools)) {
				$poolsOutput[] = $pool;
			}
		}
		$this->output = $poolsOutput;
		$this->error = PoolError::ERROR_NO_ERRORS;
	}
}
?>