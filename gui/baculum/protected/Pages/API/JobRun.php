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
 
class JobRun extends BaculumAPI {

	public function get() {}

	public function create($params) {
		$jobid = property_exists($params, 'id') ? intval($params->id) : 0;
		$job = $jobid > 0 ? $this->getModule('job')->getJobById($jobid)->name : (property_exists($params, 'name') ? $params->name : null);
		$level = $params->level;
		$fileset = property_exists($params, 'fileset') ? $params->fileset : null;
		$clientid = property_exists($params, 'clientid') ? intval($params->clientid) : null;
		$client = $this->getModule('client')->getClientById($clientid);
		$storageid = property_exists($params, 'storageid') ? intval($params->storageid) : null;
		$storage = $this->getModule('storage')->getStorageById($storageid);
		$poolid = property_exists($params, 'poolid') ? intval($params->poolid) : null;
		$pool = $this->getModule('pool')->getPoolById($poolid);
		$priority = property_exists($params, 'priority') ? intval($params->priority) : 10; // default priority is set to 10
		$jobid = property_exists($params, 'jobid') ? 'jobid="' . intval($params->jobid) . '"' : '';
		// @TODO: Move Job name pattern in more appropriate place
		$verifyjob = property_exists($params, 'verifyjob')  && preg_match('/^[\w\d\.\-\s]+$/', $params->verifyjob) === 1 ? 'verifyjob="' . ($params->verifyjob) . '"' : '';
		
		if(!is_null($job)) {
			$isValidLevel = $this->getModule('misc')->isValidJobLevel($params->level);
			if($isValidLevel === true) {
				if(!is_null($fileset)) {
					if(!is_null($client)) {
						if(!is_null($storage)) {
							if(!is_null($pool)) {
								$joblevels  = $this->getModule('misc')->getJobLevels();
								$run = $this->getModule('bconsole')->bconsoleCommand($this->director, array('run', 'job="' . $job . '"', 'level="' . $joblevels[$level] . '"', 'fileset="' . $fileset . '"', 'client="' . $client->name . '"', 'storage="' . $storage->name . '"', 'pool="' . $pool->name . '"' , 'priority="' . $priority . '"',  $jobid , $verifyjob, 'yes'), $this->user);
								$this->output = $run->output;
								$this->error = (integer)$run->exitcode;
							} else {
								$this->output = JobError::MSG_ERROR_POOLID_DOES_NOT_EXISTS;
								$this->error = JobError::ERROR_POOLID_DOES_NOT_EXISTS;
							}
						} else {
							$this->output = JobError::MSG_ERROR_STORAGEID_DOES_NOT_EXISTS;
							$this->error = JobError::ERROR_STORAGEID_DOES_NOT_EXISTS;
						}
					} else {
						$this->output = JobError::MSG_ERROR_CLIENTID_DOES_NOT_EXISTS;
						$this->error = JobError::ERROR_CLIENTID_DOES_NOT_EXISTS;
					}
				} else {
					$this->output = JobError::MSG_ERROR_FILESETID_DOES_NOT_EXISTS;
					$this->error = JobError::ERROR_FILESETID_DOES_NOT_EXISTS;
				}
			} else {
				$this->output = JobError::MSG_ERROR_INVALID_JOBLEVEL;
				$this->error = JobError::ERROR_INVALID_JOBLEVEL;
			}
		} else {
			$this->output = JobError::MSG_ERROR_JOB_DOES_NOT_EXISTS;
			$this->error = JobError::ERROR_JOB_DOES_NOT_EXISTS;
		}
	}
}

?>
