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
 
class VolumeManager extends TModule {


	public function getVolumes($limit, $withPools = false) {
		$criteria = new TActiveRecordCriteria;
		$orderPool = 'PoolId';
		$orderVolume = 'VolumeName';
		$cfg = $this->Application->getModule('configuration');
		$appCfg = $cfg->getApplicationConfig();
		if($cfg->isPostgreSQLType($appCfg['db']['type'])) {
		    $orderPool = strtolower($orderPool);
		    $orderVolume = strtolower($orderVolume);
		}
		$criteria->OrdersBy[$orderPool] = 'asc';
		$criteria->OrdersBy[$orderVolume] = 'asc';
		if(is_int($limit) && $limit > 0) {
			$criteria->Limit = $limit;
		}
		$volumes = VolumeRecord::finder()->findAll($criteria);
		if($withPools) {
			$this->setExtraVariables($volumes);
		}
		return $volumes;
	}

	public function getVolumesByPoolId($poolid) {
		return VolumeRecord::finder()->findBypoolid($poolid);
	}

	public function getVolumeByName($volumeName) {
		return VolumeRecord::finder()->findByvolumename($volumeName);
	}

	public function getVolumeById($volumeId) {
		return VolumeRecord::finder()->findBymediaid($volumeId);
	}

	public function setVolume($mediaid, $volumeOptions) {
		$volume = $this->getVolumeById($mediaid);
		if(property_exists($volumeOptions, 'volstatus')) {
			$volume->volstatus = $volumeOptions->volstatus;
		}
		if(property_exists($volumeOptions, 'poolid')) {
			$volume->poolid = $volumeOptions->poolid;
		}
		if(property_exists($volumeOptions, 'volretention')) {
			$volume->volretention = $volumeOptions->volretention;
		}
		if(property_exists($volumeOptions, 'voluseduration')) {
			$volume->voluseduration = $volumeOptions->voluseduration;
		}
		if(property_exists($volumeOptions, 'maxvoljobs')) {
			$volume->maxvoljobs = $volumeOptions->maxvoljobs;
		}
		if(property_exists($volumeOptions, 'maxvolfiles')) {
			$volume->maxvolfiles = $volumeOptions->maxvolfiles;
		}
		if(property_exists($volumeOptions, 'maxvolbytes')) {
			$volume->maxvolbytes = $volumeOptions->maxvolbytes;
		}
		if(property_exists($volumeOptions, 'slot')) {
			$volume->slot = $volumeOptions->slot;
		}
		if(property_exists($volumeOptions, 'recycle')) {
			$volume->recycle = $volumeOptions->recycle;
		}
		if(property_exists($volumeOptions, 'enabled')) {
			$volume->enabled = $volumeOptions->enabled;
		}
		if(property_exists($volumeOptions, 'inchanger')) {
			$volume->inchanger = $volumeOptions->inchanger;
		}
		return $volume->save();
	}

	private function setExtraVariables(&$volumes) {
		$pools = $this->Application->getModule('pool')->getPools(false);
		foreach($volumes as $volume) {
			$volstatus = strtolower($volume->volstatus);
			$volume->whenexpire = ($volstatus == 'full' || $volstatus == 'used') ? date( 'Y-m-d H:i:s', (strtotime($volume->lastwritten) + $volume->volretention)) : 'no date';
			foreach($pools as $pool) {
				if($volume->poolid == $pool->poolid) {
					$volume->pool = $pool;
				}
			}
		}
	}
}
?>