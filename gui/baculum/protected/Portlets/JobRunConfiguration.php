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

Prado::using('System.Web.UI.ActiveControls.TActivePanel');
Prado::using('Application.Portlets.Portlets');

class JobRunConfiguration extends Portlets {

	const DEFAULT_JOB_PRIORITY = 10;

	public $jobToVerify = array('C', 'O', 'd');

	public $verifyOptions = array('jobname' => 'Verify by Job Name', 'jobid' => 'Verify by JobId');

	public function configure($jobname) {
		$this->JobName->Text = $jobname;
		$this->Estimation->Text = '';

		$this->Level->dataSource = $this->Application->getModule('misc')->getJobLevels();
		$this->Level->dataBind();

		$this->JobToVerifyOptionsLine->Display = 'None';
		$this->JobToVerifyJobNameLine->Display = 'None';
		$this->JobToVerifyJobIdLine->Display = 'None';
		$this->AccurateLine->Display = 'Dynamic';
		$this->EstimateLine->Display = 'Dynamic';

		$verifyValues = array();

		foreach($this->verifyOptions as $value => $text) {
			$verifyValues[$value] = Prado::localize($text);
		}

		$this->JobToVerifyOptions->dataSource = $verifyValues;
		$this->JobToVerifyOptions->dataBind();

		$jobTasks = $this->Application->getModule('api')->get(array('jobs', 'tasks'))->output;

		$jobsAllDirs = array();
		foreach($jobTasks as $director => $tasks) {
			$jobsAllDirs = array_merge($jobsAllDirs, $tasks);
		}

		$this->JobToVerifyJobName->dataSource = array_combine($jobsAllDirs, $jobsAllDirs);
		$this->JobToVerifyJobName->dataBind();

		$clients = $this->Application->getModule('api')->get(array('clients'))->output;
		$clientsList = array();
		foreach($clients as $client) {
			$clientsList[$client->clientid] = $client->name;
		}
		$this->Client->dataSource = $clientsList;
		$this->Client->dataBind();

		$filesetsAll = $this->Application->getModule('api')->get(array('filesets'))->output;
		$filesetsList = array();
		foreach($filesetsAll as $director => $filesets) {
			$filesetsList = array_merge($filesets, $filesetsList);
		}
		$this->FileSet->dataSource = array_combine($filesetsList, $filesetsList);
		$this->FileSet->dataBind();

		$pools = $this->Application->getModule('api')->get(array('pools'))->output;
		$poolList = array();
		foreach($pools as $pool) {
			$poolList[$pool->poolid] = $pool->name;
		}
		$this->Pool->dataSource = $poolList;
		$this->Pool->dataBind();

		$storages = $this->Application->getModule('api')->get(array('storages'))->output;
		$storagesList = array();
		foreach($storages as $storage) {
			$storagesList[$storage->storageid] = $storage->name;
		}
		$this->Storage->dataSource = $storagesList;
		$this->Storage->dataBind();

		$this->Priority->Text = self::DEFAULT_JOB_PRIORITY;
	}

	public function run_job($sender, $param) {
		if($this->PriorityValidator->IsValid === false) {
			return false;
		}
		$params = array();
		$params['name'] = $this->JobName->Text;
		$params['level'] = $this->Level->SelectedValue;
		$params['fileset'] = $this->FileSet->SelectedValue;
		$params['clientid'] = $this->Client->SelectedValue;
		$params['storageid'] = $this->Storage->SelectedValue;
		$params['poolid'] = $this->Pool->SelectedValue;
		$params['priority'] = $this->Priority->Text;

		if (in_array($this->Level->SelectedItem->Value, $this->jobToVerify)) {
			$verifyVals = $this->getVerifyVals();
			if ($this->JobToVerifyOptions->SelectedItem->Value == $verifyVals['jobname']) {
				$params['verifyjob'] = $this->JobToVerifyJobName->SelectedValue;
			} elseif ($this->JobToVerifyOptions->SelectedItem->Value == $verifyVals['jobid']) {
				$params['jobid'] = $this->JobToVerifyJobId->Text;
			}
		}

		$result = $this->Application->getModule('api')->create(array('jobs', 'run'), $params)->output;
		$this->Estimation->Text = implode(PHP_EOL, $result);
	}

	public function estimate($sender, $param) {
		$params = array();
		$params['name'] = $this->JobName->Text;
		$params['level'] = $this->Level->SelectedValue;
		$params['fileset'] = $this->FileSet->SelectedValue;
		$params['clientid'] = $this->Client->SelectedValue;
		$params['accurate'] = (integer)$this->Accurate->Checked;
		$result = $this->Application->getModule('api')->create(array('jobs', 'estimate'), $params)->output;
		$this->Estimation->Text = implode(PHP_EOL, $result);
	}

	public function priorityValidator($sender, $param) {
		$isValid = preg_match('/^[0-9]+$/', $this->Priority->Text) === 1 && $this->Priority->Text > 0;
		$param->setIsValid($isValid);
	}

	public function jobIdToVerifyValidator($sender, $param) {
		$verifyVals = $this->getVerifyVals();
		if(in_array($this->Level->SelectedValue, $this->jobToVerify) && $this->JobToVerifyOptions->SelectedItem->Value == $verifyVals['jobid']) {
			$isValid = preg_match('/^[0-9]+$/',$this->JobToVerifyJobId->Text) === 1 && $this->JobToVerifyJobId->Text > 0;
		} else {
			$isValid = true;
		}
		$param->setIsValid($isValid);
		return $isValid;
	}

	private function getVerifyVals() {
		$verifyOpt = array_keys($this->verifyOptions);
		$verifyVals = array_combine($verifyOpt, $verifyOpt);
		return $verifyVals;
	}
}
?>
