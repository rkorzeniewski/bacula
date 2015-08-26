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
 
class JobEstimate extends BaculumAPI {

	public function get() {}

	public function create($params) {
		$jobid = property_exists($params, 'id') ? intval($params->id) : 0;
		$job = $jobid > 0 ? $this->getModule('job')->getJobById($jobid)->name : $params->name;
		$level = $params->level;
		$fileset = $params->fileset;
		$clientid = intval($params->clientid);
		$client = $this->getModule('client')->getClientById($clientid);
		$accurateJob = intval($params->accurate);
		$accurate = $accurateJob === 1 ? 'yes' : 'no';

		if(!is_null($job)) {
			$isValidLevel = $this->getModule('misc')->isValidJobLevel($params->level);
			if($isValidLevel === true) {
				if(!is_null($fileset)) {
					if(!is_null($client)) {
						$joblevels  = $this->getModule('misc')->getJobLevels();
						$estimation = $this->getModule('bconsole')->bconsoleCommand($this->director, array('estimate', 'job="' . $job . '"', 'level="' . $joblevels[$level] . '"', 'fileset="' . $fileset. '"', 'client="' . $client->name . '"', 'accurate="' . $accurate . '"'), $this->user);
						$this->output = $estimation->output;
						$this->error = (integer)$estimation->exitcode;
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