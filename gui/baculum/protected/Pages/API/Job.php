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
 
class Job extends BaculumAPI {
	public function get() {
		$jobid = intval($this->Request['id']);
		$job = $this->getModule('job')->getJobById($jobid);
		$allowedJobs = $this->getModule('bconsole')->bconsoleCommand($this->director, array('.jobs'), $this->user)->output;
		if(!is_null($job) && in_array($job->name, $allowedJobs)) {
			$this->output = $job;
			$this->error = JobError::ERROR_NO_ERRORS;
		} else {
			$this->output = JobError::MSG_ERROR_JOB_DOES_NOT_EXISTS;
			$this->error = JobError::ERROR_JOB_DOES_NOT_EXISTS;
		}
	}

	public function remove($id) {
		$jobid = intval($id);
		$job = $this->getModule('job')->getJobById($jobid);
		if(!is_null($job)) {
			$delete = $this->getModule('bconsole')->bconsoleCommand($this->director, array('delete', 'jobid="' . $job->jobid . '"'), $this->user);
			$this->output = $delete->output;
			$this->error = (integer)$delete->exitcode;
		} else {
			$this->output = JobError::MSG_ERROR_JOB_DOES_NOT_EXISTS;
			$this->error = JobError::ERROR_JOB_DOES_NOT_EXISTS;
		}
	}
}

?>