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
 
class StorageManager extends TModule {

	public function getStorages($limit) {
		$criteria = new TActiveRecordCriteria;
		if(is_int($limit) && $limit > 0) {
			$criteria->Limit = $limit;
		}
		return StorageRecord::finder()->findAll($criteria);
	}

	public function getStorageByName($name) {
		return StorageRecord::finder()->findByName($name);
	}

	public function getStorageById($storageid) {
		return StorageRecord::finder()->findBystorageid($storageid);
	}
}
?>
