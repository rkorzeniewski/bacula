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
 
class PoolUpdateVolumes extends BaculumAPI {
	public function get() {}

	public function set($id, $params) {
		file_put_contents('/tmp/kekeks', print_r($id, true));
		$pool = $this->getModule('pool')->getPoolById($id);
		if(!is_null($pool)) {
			$voldata = $this->getModule('volume')->getVolumesByPoolId($pool->poolid);
			if(!is_null($voldata)) {
				$poolUpdateVolumes = $this->getModule('bconsole')->bconsoleCommand($this->director, array('update', 'volume="' .  $voldata->volumename . '"', 'allfrompool="' . $pool->name . '"'), $this->user);
				$this->output = $poolUpdateVolumes->output;
				$this->error = (integer)$poolUpdateVolumes->exitcode;
			} else {
				$this->output = PoolError::MSG_ERROR_NO_VOLUMES_IN_POOL_TO_UPDATE;
				$this->error = PoolError::ERROR_NO_VOLUMES_IN_POOL_TO_UPDATE;
			}
		} else {
			$this->output = PoolError::MSG_ERROR_POOL_DOES_NOT_EXISTS;
			$this->error = PoolError::ERROR_POOL_DOES_NOT_EXISTS;
		}
	}
}

?>