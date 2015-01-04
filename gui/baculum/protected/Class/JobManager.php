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
 
class JobManager extends TModule {

	public function getJobs($limit) {
		$criteria = new TActiveRecordCriteria;
		$order = 'JobId';
		$cfg = $this->Application->getModule('configuration');
		$appCfg = $cfg->getApplicationConfig();
		if($cfg->isPostgreSQLType($appCfg['db']['type'])) {
		    $order = strtolower($order);
		}
		$criteria->OrdersBy[$order] = 'desc';
		if(is_int($limit) && $limit > 0) {
			$criteria->Limit = $limit;
		}
		return JobRecord::finder()->findAll($criteria);
	}

	public function getJobById($id) {
		return JobRecord::finder()->findByjobid($id);
	}

	public function getJobByName($name) {
		return JobRecord::finder()->findByname($name);
	}

	public function deleteJobById($id) {
		return JobRecord::finder()->deleteByjobid($id);
	}

	public function getRecentJobids($jobname, $clientid) {
		$sql = "name='$jobname' AND clientid='$clientid' AND jobstatus IN ('T', 'W') AND level IN ('F', 'I', 'D') ORDER BY endtime DESC";
		$finder = JobRecord::finder();
		$jobs = $finder->findAll($sql);

		$jobids = array();
		$waitForFull = false;
		if(!is_null($jobs)) {
			foreach($jobs as $job) {
				if($job->level == 'F') {
					$jobids[] = $job->jobid;
					break;
				} elseif($job->level == 'D' && $waitForFull === false) {
					$jobids[] = $job->jobid;
					$waitForFull = true;
				} elseif($job->level == 'I' && $waitForFull === false) {
					$jobids[] = $job->jobid;
				}
			}
		}
		return $jobids;
	}
}
?>
