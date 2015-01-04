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

class JobConfiguration extends Portlets {

	const DEFAULT_JOB_PRIORITY = 10;


	public $jobToVerify = array('C', 'O', 'd');

	public $verifyOptions = array('jobname' => 'Verify by Job Name', 'jobid' => 'Verify by JobId');

	public function configure($jobId) {
		$jobdata = $this->Application->getModule('api')->get(array('jobs', $jobId))->output;
		$this->JobName->Text = $jobdata->job;
		$this->JobID->Text = $jobdata->jobid;
		$joblog = $this->Application->getModule('api')->get(array('joblog', $jobdata->jobid))->output;
		$this->Estimation->Text = is_array($joblog) ? implode(PHP_EOL, $joblog) : Prado::localize("Output for selected job is not available yet or you do not have enabled logging job logs to catalog database. For watching job log there is need to add to the job Messages resource next directive: console = all, !skipped, !saved");

		$this->Level->dataSource = $this->Application->getModule('misc')->getJobLevels();
		$this->Level->SelectedValue = $jobdata->level;
		$this->Level->dataBind();

		$isVerifyOption = in_array($jobdata->level, $this->jobToVerify);
		$this->JobToVerifyOptionsLine->Display = ($isVerifyOption === true) ? 'Dynamic' : 'None';
		$this->JobToVerifyJobNameLine->Display = ($isVerifyOption === true) ? 'Dynamic' : 'None';
		$this->JobToVerifyJobIdLine->Display = 'None';
		$this->AccurateLine->Display = ($isVerifyOption === true) ? 'None' : 'Dynamic';
		$this->EstimateLine->Display = ($isVerifyOption === true) ? 'None' : 'Dynamic';

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
		$this->Client->SelectedValue = $jobdata->clientid;
		$this->Client->dataBind();

		$filesetsAll = $this->Application->getModule('api')->get(array('filesets'))->output;
		$filesetsList = array();
		foreach($filesetsAll as $director => $filesets) {
			$filesetsList = array_merge($filesets, $filesetsList);
		}
		if($jobdata->filesetid != 0) {
			$selectedFileset = $this->Application->getModule('api')->get(array('filesets', $jobdata->filesetid))->output;
		}
		$this->FileSet->dataSource = array_combine($filesetsList, $filesetsList);
		$this->FileSet->SelectedValue = @$selectedFileset->fileset;
		$this->FileSet->dataBind();

		$pools = $this->Application->getModule('api')->get(array('pools'))->output;
		$poolList = array();
		foreach($pools as $pool) {
			$poolList[$pool->poolid] = $pool->name;
		}
		$this->Pool->dataSource = $poolList;
		$this->Pool->SelectedValue = $jobdata->poolid;
		$this->Pool->dataBind();

		$storages = $this->Application->getModule('api')->get(array('storages'))->output;
		$storagesList = array();
		foreach($storages as $storage) {
			$storagesList[$storage->storageid] = $storage->name;
		}
		$this->Storage->dataSource = $storagesList;
		$this->Storage->dataBind();

		$runningJobStates = $this->Application->getModule('misc')->getRunningJobStates();

		$this->Priority->Text = ($jobdata->priorjobid == 0) ? self::DEFAULT_JOB_PRIORITY : $jobdata->priorjobid;
		$this->DeleteButton->Visible = true;
		$this->CancelButton->Visible = in_array($jobdata->jobstatus, $runningJobStates);
	}

	public function status($sender, $param) {
		$joblog = $this->Application->getModule('api')->get(array('joblog', $this->JobID->Text))->output;
		$this->Estimation->Text = is_array($joblog) ? implode(PHP_EOL, $joblog) : Prado::localize("Output for selected job is not available yet or you do not have enabled logging job logs to catalog database. For watching job log there is need to add to the job Messages resource next directive: console = all, !skipped, !saved");
	}

	public function delete($sender, $param) {
		$this->Application->getModule('api')->remove(array('jobs', $this->JobID->Text));
		$this->DeleteButton->Visible = false;
	}

	public function cancel($sender, $param) {
		$this->Application->getModule('api')->set(array('jobs', 'cancel', $this->JobID->Text), array('a' => 'b'));
		$this->CancelButton->Visible = false;
	}

	public function run_again($sender, $param) {
		if($this->PriorityValidator->IsValid === false) {
			return false;
		}
		$params = array();
		$params['id'] = $this->JobID->Text;
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
		$params['id'] = $this->JobID->Text;
		$params['level'] = $this->Level->SelectedValue;
		$params['fileset'] = $this->FileSet->SelectedValue;
		$params['clientid'] = $this->Client->SelectedValue;
		$params['accurate'] = (integer)$this->Accurate->Checked;
		$result = $this->Application->getModule('api')->create(array('jobs', 'estimate'), $params)->output;
		$this->Estimation->Text = implode(PHP_EOL, $result);
	}

	public function priorityValidator($sender, $param) {
		$isValid = preg_match('/^[0-9]+$/',$this->Priority->Text) === 1 && $this->Priority->Text > 0;
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
