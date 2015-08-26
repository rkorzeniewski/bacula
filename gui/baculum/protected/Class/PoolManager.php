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
 
class PoolManager extends TModule {
	public function getPools($limit) {
		$criteria = new TActiveRecordCriteria;
		$order = 'Name';
		$cfg = $this->Application->getModule('configuration');
		$appCfg = $cfg->getApplicationConfig();
		if($cfg->isPostgreSQLType($appCfg['db']['type'])) {
		    $order = strtolower($order);
		}
		$criteria->OrdersBy[$order] = 'asc';
		if(is_int($limit) && $limit > 0) {
			$criteria->Limit = $limit;
		}
		return PoolRecord::finder()->findAll($criteria);
	}

	public function getPoolByName($poolName) {
		return PoolRecord::finder()->findByname($poolName);
	}

	public function getPoolById($poolId) {
		return PoolRecord::finder()->findBypoolid($poolId);
	}

	public function setPool($poolid, $poolOptions) {
		$pool = $this->getPoolById($poolid);
		$pool->enabled = $poolOptions->enabled;
		$pool->maxvols = $poolOptions->maxvols;
		$pool->maxvoljobs = $poolOptions->maxvoljobs;
		$pool->maxvolbytes = $poolOptions->maxvolbytes;
		$pool->voluseduration = $poolOptions->voluseduration;
		$pool->volretention = $poolOptions->volretention;
		$pool->labelformat = $poolOptions->labelformat;
		$pool->scratchpoolid = $poolOptions->scratchpoolid;
		$pool->recyclepoolid = $poolOptions->recyclepoolid;
		$pool->recycle = $poolOptions->recycle;
		$pool->autoprune = $poolOptions->autoprune;
		$pool->actiononpurge = $poolOptions->actiononpurge;
		return $pool->save();
	}
}
?>