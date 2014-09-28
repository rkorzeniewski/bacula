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
 
Prado::using('System.Web.UI.ActiveControls.TActiveDataGrid');
Prado::using('System.Web.UI.ActiveControls.TActiveRepeater');
Prado::using('System.Web.UI.ActiveControls.TActiveLinkButton');
Prado::using('System.Web.UI.ActiveControls.TActivePanel');
Prado::using('System.Web.UI.ActiveControls.TCallback');
Prado::using('Application.Portlets.Portlets');

class JobList extends Portlets {

	public $ShowID, $windowTitle;

	private $jobTypes = array('B' => 'Backup', 'M' => 'Migrated', 'V' => 'Verify', 'R' => 'Restore', 'I' => 'Internal', 'D' => 'Admin', 'A' => 'Archive', 'C' => 'Copy', 'g' => 'Migration');

	private $jobStates;

	public $jobLevels;

	public function onLoad($param) {
		parent::onLoad($param);
		$this->prepareData();
		$this->jobLevels = $this->Application->getModule('misc')->getJobLevels();
	}

	public function setWindowTitle($param) {
		$this->windowTitle = $param;
	}

	public function getJobType($jobLetter) {
		return array_key_exists($jobLetter, $this->jobTypes) ? $this->jobTypes[$jobLetter] : null;
	}

	public function getJobState($jobStateLetter) {
		$jobstates =  array(
		'C' => (object)array('value' => 'Created', 'description' =>'Created but not yet running'),
		'R' => (object)array('value' => 'Running', 'description' => 'Running'),
		'B' => (object)array('value' => 'Blocked', 'description' => 'Blocked'),
		'T' => (object)array('value' => 'Terminated', 'description' =>'Terminated normally'),
		'W' => (object)array('value' => 'Terminated with warnings', 'description' =>'Terminated normally with warnings'),
		'E' => (object)array('value' => 'Error', 'description' =>'Terminated in Error'),
		'e' => (object)array('value' => 'Non-fatal error', 'description' =>'Non-fatal error'),
		'f' => (object)array('value' => 'Fatal error', 'description' =>'Fatal error'),
		'D' => (object)array('value' => 'Verify', 'description' =>'Verify Differences'),
		'A' => (object)array('value' => 'Canceled by user', 'description' =>'Canceled by the user'),
		'I' => (object)array('value' => 'Incomplete', 'description' =>'Incomplete Job'),
		'F' => (object)array('value' => 'Waiting on FD', 'description' =>'Waiting on the File daemon'),
		'S' => (object)array('value' => 'Waiting on SD', 'description' =>'Waiting on the Storage daemon'),
		'm' => (object)array('value' => 'Waiting for new vol.', 'description' =>'Waiting for a new Volume to be mounted'),
		'M' => (object)array('value' => 'Waiting for mount', 'description' =>'Waiting for a Mount'),
		's' => (object)array('value' => 'Waiting for storage', 'description' =>'Waiting for Storage resource'),
		'j' => (object)array('value' => 'Waiting for job', 'description' =>'Waiting for Job resource'),
		'c' => (object)array('value' => 'Waiting for client', 'description' =>'Waiting for Client resource'),
		'd' => (object)array('value' => 'Waiting for Max. jobs', 'description' =>'Wating for Maximum jobs'),
		't' => (object)array('value' => 'Waiting for start', 'description' =>'Waiting for Start Time'),
		'p' => (object)array('value' => 'Waiting for higher priority', 'description' =>'Waiting for higher priority job to finish'),
		'i' => (object)array('value' => 'Batch insert', 'description' =>'Doing batch insert file records'),
		'a' => (object)array('value' => 'Despooling attributes', 'description' =>'SD despooling attributes'),
		'l' => (object)array('value' => 'Data despooling', 'description' =>'Doing data despooling'),
		'L' => (object)array('value' => 'Commiting data', 'description' =>'Committing data (last despool)')
	);
		return array_key_exists($jobStateLetter, $jobstates) ? $jobstates[$jobStateLetter] : null;
	}

	public function prepareData($forceReload = false) {
		$allowedButtons = array('JobBtn', 'ReloadJobs', 'Run');
		if($this->Page->IsPostBack || $this->Page->IsCallBack || $forceReload) {
			if(in_array($this->getPage()->CallBackEventTarget->ID, $allowedButtons) || $forceReload) {
				$params = $this->getUrlParams('jobs', $this->getPage()->JobWindow->ID);
				$jobs = $this->Application->getModule('api')->get($params);
				$isDetailView = $_SESSION['view' . $this->getPage()->JobWindow->ID] == 'details';
				$this->RepeaterShow->Visible = !$isDetailView;
				$this->Repeater->DataSource = $isDetailView == false ? $jobs->output : array();
				$this->Repeater->dataBind();
				$this->DataGridShow->Visible = $isDetailView;
				$this->DataGrid->DataSource = $isDetailView === true ? $this->Application->getModule('misc')->objectToArray($jobs->output) : array();
				$this->DataGrid->dataBind();
			}
		}
	}
 
    public function sortDataGrid($sender, $param) {
		$params = $this->getUrlParams('jobs', $this->getPage()->JobWindow->ID);
		$data = $this->Application->getModule('api')->get($params)->output;
		$data = $this->Application->getModule('misc')->objectToArray($data);
		$this->DataGrid->DataSource = $this->sortData($data, $param->SortExpression, $sender->UniqueID);
		$this->DataGrid->dataBind();
	}

	public function setShowID($ShowID) {
		$this->ShowID = $this->getMaster()->ShowID = $ShowID;
	}

	public function getShowID() {
		return $this->ShowID;
	}

	public function configure($sender, $param) {
		if($this->Page->IsCallBack) {
			$this->getPage()->JobConfiguration->configure($param->CallbackParameter);
		}
	}
}
?>
