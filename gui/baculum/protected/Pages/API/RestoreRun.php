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
 
class RestoreRun extends BaculumAPI {

	public function get() {}

	public function create($params) {
		$rfile = property_exists($params, 'rpath') ? $params->rpath : null;
		$clientid = property_exists($params, 'clientid') ? intval($params->clientid) : null;
		$fileset = property_exists($params, 'fileset') ? $params->fileset : null;
		$priority = property_exists($params, 'priority') ? intval($params->priority) : null;
		$where = property_exists($params, 'where') ? $params->where : null;
		$replace = property_exists($params, 'replace') ? $params->replace : null;

		$client = $this->getModule('client')->getClientById($clientid);

		if(!is_null($fileset)) {
			if(!is_null($client)) {
				if(preg_match('/^b2[\d]+$/', $rfile) === 1) {
					if(!is_null($where)) {
						if(!is_null($replace)) {
							$restore = $this->getModule('bconsole')->bconsoleCommand($this->director, array('restore', 'file="?' . $rfile . '"', 'client="' . $client->name . '"', 'where="' . $where . '"', 'replace="' . $replace . '"', 'fileset="' . $fileset . '"', 'priority="' . $priority . '"', 'yes'), $this->user);
							$this->output = $restore->output;
							$this->error = (integer)$restore->exitcode;
						} else {
							$this->output = JobError::MSG_ERROR_INVALID_REPLACE_OPTION;
							$this->error = JobError::ERROR_INVALID_REPLACE_OPTION;
						}
					} else {
						$this->output = JobError::MSG_ERROR_INVALID_WHERE_OPTION;
						$this->error = JobError::ERROR_INVALID_WHERE_OPTION;
					}
				} else {
					$this->output = JobError::MSG_ERROR_INVALID_RPATH;
					$this->error = JobError::ERROR_INVALID_RPATH;
				}
			} else {
				$this->output = JobError::MSG_ERROR_CLIENTID_DOES_NOT_EXISTS;
				$this->error = JobError::ERROR_CLIENTID_DOES_NOT_EXISTS;
			}
		} else {
			$this->output = JobError::MSG_ERROR_FILESETID_DOES_NOT_EXISTS;
			$this->error = JobError::ERROR_FILESETID_DOES_NOT_EXISTS;
		}
	}
}

?>