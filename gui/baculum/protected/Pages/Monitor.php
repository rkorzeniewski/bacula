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

class Monitor extends BaculumPage {
	public function onInit($param) {
		parent::onInit($param);
		$_SESSION['monitor_data'] = array('jobs' => array(), 'running_jobs' => array(), 'terminated_jobs' => array());
		$_SESSION['monitor_data']['jobs'] = $this->Application->getModule('api')->get(array('jobs'))->output;

		$runningJobStates = $this->Application->getModule('misc')->getRunningJobStates();

		for ($i = 0; $i < count($_SESSION['monitor_data']['jobs']); $i++) {
			if (in_array($_SESSION['monitor_data']['jobs'][$i]->jobstatus, $runningJobStates)) {
				$_SESSION['monitor_data']['running_jobs'][] = $_SESSION['monitor_data']['jobs'][$i];
			} else {
				$_SESSION['monitor_data']['terminated_jobs'][] = $_SESSION['monitor_data']['jobs'][$i];
			}
		}
		echo json_encode($_SESSION['monitor_data']);
		exit();
	}
}

?>
