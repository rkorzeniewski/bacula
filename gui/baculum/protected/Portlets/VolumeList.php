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

Prado::using('System.Web.UI.ActiveControls.TActiveDataGrid');
Prado::using('System.Web.UI.ActiveControls.TActiveRepeater');
Prado::using('System.Web.UI.ActiveControls.TActiveLinkButton');
Prado::using('System.Web.UI.ActiveControls.TActivePanel');
Prado::using('System.Web.UI.ActiveControls.TActiveHiddenField');
Prado::using('System.Web.UI.ActiveControls.TCallback');
Prado::using('Application.Portlets.ISlideWindow');
Prado::using('Application.Portlets.Portlets');

class VolumeList extends Portlets implements ISlideWindow {

	public $ID;
	public $buttonID;
	public $windowTitle;
	public $pools;
	public $oldPool;
	public $view;

	public function setID($id) {
		$this->ID = $id;
	}

	public function getID($hideAutoID = true) {
		return $this->ID;
	}

	public function setButtonID($id) {
		$this->buttonID = $id;
	}

	public function getButtonID() {
		return $this->buttonID;
	}

	public function setWindowTitle($param) {
		$this->windowTitle = $param;
	}

	public function getWindowTitle() {
		return $this->windowTitle;
	}

	public function onLoad($param) {
		parent::onLoad($param);
		$this->prepareData();
	}

	public function prepareData($forceReload = false) {
		$allowedButtons = array('MediaBtn', 'ReloadVolumes');
		if($this->Page->IsPostBack || $this->Page->IsCallBack || $forceReload) {
			if(in_array($this->getPage()->CallBackEventTarget->ID, $allowedButtons) || $forceReload) {
				$params = $this->getUrlParams('volumes', $this->getPage()->VolumeWindow->ID);
				array_push($params, '?showpools=1');
				$volumes = $this->Application->getModule('api')->get($params);
				$isDetailView = $_SESSION['view' . $this->getPage()->VolumeWindow->ID] == 'details';
				if($isDetailView === true) {
					$this->RepeaterShow->Visible = false;
					$this->DataGridShow->Visible = true;
					$this->DataGrid->DataSource = $this->Application->getModule('misc')->objectToArray($volumes->output);
					$this->DataGrid->dataBind();

				} else {
					$this->Repeater->DataSource = $volumes->output;
					$this->Repeater->dataBind();
					$this->RepeaterShow->Visible = true;
					$this->DataGridShow->Visible = false;
				}
			}
		}
	}

	protected function sortData($data, $key, $id) {
		if($this->getSortOrder($id) == parent::SORT_DESC) {
			if($key == 'pool') {
				$key = 'name';
				$compare = create_function('$a,$b','if ($a["pool"]["'.$key.'"] == $b["pool"]["'.$key.'"]) {return 0;}else {return ($a["pool"]["'.$key.'"] < $b["pool"]["'.$key.'"]) ? 1 : -1;}');
			} else {
				$compare = create_function('$a,$b','if ($a["'.$key.'"] == $b["'.$key.'"]) {return 0;}else {return ($a["'.$key.'"] < $b["'.$key.'"]) ? 1 : -1;}');
			}
		} else {
			if($key == 'pool') {
				$key = 'name';
				$compare = create_function('$a,$b','if ($a["pool"]["'.$key.'"] == $b["pool"]["'.$key.'"]) {return 0;}else {return ($a["pool"]["'.$key.'"] > $b["pool"]["'.$key.'"]) ? 1 : -1;}');
			} else {
				$compare = create_function('$a,$b','if ($a["'.$key.'"] == $b["'.$key.'"]) {return 0;}else {return ($a["'.$key.'"] > $b["'.$key.'"]) ? 1 : -1;}');
			}
		}
		usort($data,$compare);
		return $data;
	}

	public function sortDataGrid($sender, $param) {
		$params = $this->getUrlParams('volumes', $this->getPage()->VolumeWindow->ID);
		array_push($params, '?showpools=1');
		$data = $this->Application->getModule('api')->get($params)->output;
		$data = $this->Application->getModule('misc')->objectToArray($data);
		$this->DataGrid->DataSource = $this->sortData($data, $param->SortExpression, $sender->UniqueID);
		$this->DataGrid->dataBind();
	}

	public function configure($sender, $param) {
		if($this->Page->IsCallBack) {
			$this->getPage()->VolumeConfiguration->configure($param->CallbackParameter);
		}
	}

	public function executeAction($action) {
		$params = explode(';', $this->CheckedValues->Value);
		$commands = array();
		switch($action) {
			case 'prune': {
				for($i = 0; $i < count($params); $i++) {
					$cmd = array('prune');
					$cmd[] = 'volume="' . $params[$i] . '"';
					$cmd[] = 'yes';
					$cmd[] = PHP_EOL;
					$commands[] = implode(' ', $cmd);
				}
				$this->getPage()->Console->CommandLine->Text = implode(' ', $commands);
				$this->getPage()->Console->sendCommand(null, null);
				break;
			}
			case 'purge': {
				for($i = 0; $i < count($params); $i++) {
					$cmd = array('purge');
					$cmd[] = 'volume="' . $params[$i] . '"';
					$cmd[] = 'yes';
					$cmd[] = PHP_EOL;
					$commands[] = implode(' ', $cmd);
				}
				$this->getPage()->Console->CommandLine->Text = implode(' ', $commands);
				$this->getPage()->Console->sendCommand(null, null);
				break;
			}
			case 'delete': {
				for($i = 0; $i < count($params); $i++) {
					$cmd = array('delete');
					$cmd[] = 'volume="' . $params[$i] . '"';
					$cmd[] = 'yes';
					$cmd[] = PHP_EOL;
					$commands[] = implode(' ', $cmd);
				}
				$this->getPage()->Console->CommandLine->Text = implode(' ', $commands);
				$this->getPage()->Console->sendCommand(null, null);
				break;
			}
		}
		$this->CheckedValues->Value = "";
	}
}
?>
