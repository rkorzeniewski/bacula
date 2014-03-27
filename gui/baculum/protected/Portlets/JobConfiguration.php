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

Prado::using('Application.Portlets.Portlets');

class JobConfiguration extends Portlets {

	const DEFAULT_JOB_PRIORITY = 10;

	private $runningJobStates = array('C', 'R');

	public function onInit($param) {
		parent::onInit($param);
		$this->Run->setActionClass($this);
		$this->Status->setActionClass($this);
		$this->Cancel->setActionClass($this);
		$this->Delete->setActionClass($this);
		$this->Estimate->setActionClass($this);
	}

	public function configure($jobId) {
		$jobdata = $this->Application->getModule('api')->get(array('jobs', $jobId))->output;
		$this->JobName->Text = $jobdata->name;
		$this->JobID->Text = $jobdata->jobid;
		$joblog = $this->Application->getModule('api')->get(array('joblog', $jobdata->jobid))->output;
		$this->Estimation->Text = is_array($joblog) ? implode(PHP_EOL, $joblog) : Prado::localize("Output for selected job is not available yet or you do not have enabled logging job logs to catalog database."  . PHP_EOL . PHP_EOL .  "For watching job log there is need to add to the job Messages resource next directive:" . PHP_EOL . PHP_EOL . "console = all, !skipped, !saved" . PHP_EOL);

		$this->Level->dataSource = $this->Application->getModule('misc')->getJobLevels();
		$this->Level->SelectedValue = $jobdata->level;
		$this->Level->dataBind();

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

		$this->Priority->Text = ($jobdata->priorjobid == 0) ? self::DEFAULT_JOB_PRIORITY : $jobdata->priorjobid;
		$this->DeleteButton->Visible = true;
		$this->CancelButton->Visible = in_array($jobdata->jobstatus, $this->runningJobStates);
	}

	public function save($sender, $param) {
		switch($sender->getParent()->ID) {
			case $this->Estimate->ID: {
				$params = array();
				$params['id'] = $this->JobID->Text;
				$params['level'] = $this->Level->SelectedValue;
				$params['fileset'] = $this->FileSet->SelectedValue;
				$params['clientid'] = $this->Client->SelectedValue;
				$params['accurate'] = (integer)$this->Accurate->Checked;
				$result = $this->Application->getModule('api')->create(array('jobs', 'estimate'), $params)->output;
				$this->Estimation->Text = implode(PHP_EOL, $result);
				break;
			}
			case $this->Run->ID: {
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
				$result = $this->Application->getModule('api')->create(array('jobs', 'run'), $params)->output;
				$this->Estimation->Text = implode(PHP_EOL, $result);
				break;
			}
			case $this->Delete->ID: {
				$this->Application->getModule('api')->remove(array('jobs', $this->JobID->Text));
				$this->DeleteButton->Visible = false;
				break;
			}
			case $this->Cancel->ID: {
				$this->Application->getModule('api')->set(array('jobs', 'cancel', $this->JobID->Text), array('a' => 'b'));
				$this->CancelButton->Visible = false;
				break;
			}
			case $this->Status->ID: {
				$joblog = $this->Application->getModule('api')->get(array('joblog', $this->JobID->Text))->output;
				$this->Estimation->Text = is_array($joblog) ? implode(PHP_EOL, $joblog) : Prado::localize("Output for selected job is not available yet or you do not have enabled logging job logs to catalog database."  . PHP_EOL . PHP_EOL .  "For watching job log there is need to add to the job Messages resource next directive:" . PHP_EOL . PHP_EOL . "console = all, !skipped, !saved" . PHP_EOL);
				break;
			}
		}
	}

	public function priorityValidator($sender, $param) {
		$isValid = preg_match('/^[0-9]+$/',$this->Priority->Text) === 1 && $this->Priority->Text > 0;
		$param->setIsValid($isValid);
	}
}
?>