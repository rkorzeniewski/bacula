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
 
Prado::using('System.Web.UI.ActiveControls.TActiveRepeater');
Prado::using('System.Web.UI.ActiveControls.TActiveDataGrid');
Prado::using('Application.Portlets.Portlets');

class ClientList extends Portlets {

	public $ShowID, $windowTitle;

	public function onLoad($param) {
		parent::onLoad($param);
		$this->prepareData();
	}

	public function setWindowTitle($param) {
		$this->windowTitle = $param;
	}

	public function prepareData($forceReload = false) {
		$allowedButtons = array('ClientBtn', 'ReloadClients');
		if($this->Page->IsPostBack || $this->Page->IsCallBack || $forceReload) {
			if(in_array($this->getPage()->CallBackEventTarget->ID, $allowedButtons) || $forceReload) {
				$params = $this->getUrlParams('clients', $this->getPage()->ClientWindow->ID);
				$clients = $this->Application->getModule('api')->get($params);
				$isDetailView = $_SESSION['view' . $this->getPage()->ClientWindow->ID] == 'details';
				$clientsList = $this->Application->getModule('misc')->objectToArray($clients->output);
				$this->RepeaterShow->Visible = !$isDetailView;
				$this->Repeater->DataSource = $isDetailView === false ? $clientsList : array();
				$this->Repeater->dataBind();
				$this->DataGridShow->Visible = $isDetailView;
				$this->DataGrid->DataSource = $isDetailView === true ?  $clientsList : array();
				$this->DataGrid->dataBind();
			}
		}
	}

	public function sortDataGrid($sender, $param) {
		$params = $this->getUrlParams('clients', $this->getPage()->ClientWindow->ID);
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
			$this->getPage()->ClientConfiguration->configure($param->CallbackParameter);
		}
	}
}
?>