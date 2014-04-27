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
 
class JobTasks extends BaculumAPI {
	public function get() {
		$limit = intval($this->Request['limit']);
		$directors = $this->getModule('bconsole')->getDirectors();
		if($directors->exitcode === 0) {
			$jobs = array();
			for($i = 0; $i < count($directors->output); $i++) {
				$jobsList = $this->getModule('bconsole')->bconsoleCommand($directors->output[$i], array('.jobs'), $this->user)->output;
				$jobsshow = $this->getModule('bconsole')->bconsoleCommand($directors->output[$i], array('show', 'jobs'), $this->user)->output;
				$jobs[$directors->output[$i]] = array();
				for($j = 0; $j < count($jobsList); $j++) {
					/**
					 * Checking by "show job" command is ugly way to be sure that is reading jobname but not some 
					 * random output (eg. "You have messages." or debugging).
					 * For now I did not find nothing better for be sure that output contains job.
					 */
					for($k = 0; $k < count($jobsshow); $k++) {
						if(preg_match('/^Job: name=' . $jobsList[$j] . '.*/', $jobsshow[$k]) === 1) {
							$jobs[$directors->output[$i]][] = $jobsList[$j];
							break;
						}
					}
					// limit per director, not for entire elements
					if($limit > 0 && count($jobs[$directors->output[$i]]) === $limit) {
						break;
					}
				}
			}
			$this->output = $jobs;
			$this->error =  BconsoleError::ERROR_NO_ERRORS;
		} else {
			$this->output = BconsoleError::MSG_ERROR_BCONSOLE_CONNECTION_PROBLEM;
			$this->error = BconsoleError::ERROR_BCONSOLE_CONNECTION_PROBLEM;
		}
	}
}

?>