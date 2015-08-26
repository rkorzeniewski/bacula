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
 
class Jobs extends BaculumAPI {
	public function get() {
		$limit = intval($this->Request['limit']);
		$jobs = $this->getModule('job')->getJobs($limit);
		$allowedJobs = $this->getModule('bconsole')->bconsoleCommand($this->director, array('.jobs'), $this->user)->output;
		$jobsOutput = array();
		foreach($jobs as $job) {
			if(in_array($job->name, $allowedJobs)) {
				$jobsOutput[] = $job;
			}
		}
		$this->output = $jobsOutput;
		$this->error = JobError::ERROR_NO_ERRORS;
	}
}
?>