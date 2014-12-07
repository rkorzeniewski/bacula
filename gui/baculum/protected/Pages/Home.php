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
 
Prado::using('System.Web.UI.ActiveControls.TActiveButton');
Prado::using('System.Web.UI.ActiveControls.TActivePanel');
Prado::using('System.Web.UI.ActiveControls.TActiveDropDownList');
Prado::using('System.Web.UI.ActiveControls.TActiveCheckBox');
Prado::using('System.Web.UI.ActiveControls.TActiveLinkButton');

class Home extends BaculumPage
{
	public $jobs;

	public function onInit($param) {
		parent::onInit($param);
		$isConfigExists = $this->getModule('configuration')->isApplicationConfig();
		if($isConfigExists === false) {
			$this->goToPage('ConfigurationWizard');
		}

		$appConfig = $this->getModule('configuration')->getApplicationConfig();

		$this->SettingsWizardBtn->Visible = $this->User->getIsAdmin();
		$this->MediaBtn->Visible = $this->User->getIsAdmin();
		$this->ClearBvfsCache->Visible = $this->User->getIsAdmin();
		$this->Logging->Visible = $this->User->getIsAdmin();

		if(!$this->IsPostBack && !$this->IsCallBack) {
			$this->Logging->Checked = $this->getModule('logging')->isDebugOn();
		}

		$directors = $this->getModule('api')->get(array('directors'))->output;

		if(!$this->IsPostBack && !$this->IsCallBack) {
			if(!array_key_exists('director', $_SESSION)) {
				$_SESSION['director'] = $directors[0];
			}
			$this->Director->dataSource = array_combine($directors, $directors);
			$this->Director->SelectedValue = $_SESSION['director'];
			$this->Director->dataBind();
			$this->setJobs();
			$this->setClients();
		}
	}

	public function restore($sender, $param) {
		$this->goToPage('RestoreWizard');
	}

	public function configuration($sender, $param) {
		$this->goToPage('ConfigurationWizard');
	}

	public function director($sender, $param) {
		$_SESSION['director'] = $this->Director->SelectedValue;
	}

	public function setDebug($sender, $param) {
		if($this->User->getIsAdmin() === true) {
			$this->getModule('logging')->enableDebug($this->Logging->Checked);
		}
	}

	public function clearBvfsCache($sender, $param) {
		if($this->User->getIsAdmin() === true) {
			$this->getModule('api')->set(array('bvfs', 'clear'), array());
		}
	}

	public function getJobs() {
		return json_encode($this->jobs);
	}

	public function setJobs() {
		$this->jobs = $this->getModule('api')->get(array('jobs'));
		$jobs = array('@' => Prado::localize('select job'));
		foreach($this->jobs->output as $key => $job) {
			$jobs[$job->name] = $job->name;
		}
		$this->Jobs->dataSource = $jobs;
		$this->Jobs->dataBind();
	}

	public function setClients() {
		$clients_obj = $this->getModule('api')->get(array('clients'));
		$clients = array('@' => Prado::localize('select client'));
		foreach($clients_obj->output as $key => $client) {
			$clients[$client->clientid] = $client->name;
		}
		$this->Clients->dataSource = $clients;
		$this->Clients->dataBind();

	}
}
?>
