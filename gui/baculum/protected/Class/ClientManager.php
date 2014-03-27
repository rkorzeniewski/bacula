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
 
class ClientManager extends TModule {

	public function getClients($limit) {
		$criteria = new TActiveRecordCriteria;
		if(is_numeric($limit) && $limit > 0) {
			$criteria->Limit = $limit;
		}
		return ClientRecord::finder()->findAll($criteria);
	}

	public function getClientByName($name) {
		return ClientRecord::finder()->findByName($name);
	}

	public function getClientById($id) {
		return ClientRecord::finder()->findByclientid($id);
	}

	public function setClient($clientId, $clientOptions) {
		$client = $this->getClientById($clientId);
		if(property_exists($clientOptions, 'fileretention')) {
			$client->fileretention = $clientOptions->fileretention;
		}
		if(property_exists($clientOptions, 'jobretention')) {
			$client->jobretention = $clientOptions->jobretention;
		}
		if(property_exists($clientOptions, 'autoprune')) {
			$client->autoprune = $clientOptions->autoprune;
		}
		return $client->save();
	}
}
?>