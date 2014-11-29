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

 
Prado::using('System.Web.UI.WebControls.TCompareValidator');
Prado::using('System.Web.UI.ActiveControls.TActiveLabel');
Prado::using('System.Web.UI.ActiveControls.TActivePanel');
Prado::using('System.Web.UI.ActiveControls.TActiveDropDownList');
Prado::using('System.Web.UI.ActiveControls.TActiveTextBox');
Prado::using('System.Web.UI.ActiveControls.TActiveRadioButton');
Prado::using('System.Web.UI.ActiveControls.TCallback');

Prado::using('Application.Portlets.Portlets');

class SlideWindow extends Portlets {

	public $elementsLimits = array(10, 25, 50, 100, 200, 500, 1000, 'unlimited');
        public $actions = array(
		'VolumeWindow' => array(
			'NoAction' => 'select action',
			'delete' => 'Delete',
			'prune' => 'Prune',
			'purge' => 'Purge'
		),
		'JobWindow' => array(
			'NoAction' => 'select action',
			'delete' => 'Delete'
		)
	);

	const NORMAL_VIEW = 'simple';
	const DETAIL_VIEW = 'details';

	public function onInit($param) {
		parent::onInit($param);
		if(empty($_SESSION['view' . $this->getParent()->ID]) && empty($_SESSION['limit' . $this->getParent()->ID])) {
			$_SESSION['view' . $this->getParent()->ID] = self::NORMAL_VIEW;
			$_SESSION['limit' . $this->getParent()->ID] = 'unlimited';
		}
	}

	public function onLoad($param) {
		parent::onLoad($param);
		if(!$this->getPage()->IsPostBack) {
			$this->Limit->dataSource = array_combine($this->elementsLimits, $this->elementsLimits);
			$this->Limit->SelectedValue = $_SESSION['limit' . $this->getParent()->ID];
			$this->Limit->dataBind();
			$this->Simple->Checked = ($_SESSION['view' . $this->getParent()->ID] == self::NORMAL_VIEW);
			$this->Details->Checked = ($_SESSION['view' . $this->getParent()->ID] == self::DETAIL_VIEW);
			$actions = array_key_exists($this->getParent()->ID, $this->actions) ? $this->actions[$this->getParent()->ID] : array();
			$this->Actions->dataSource = $actions;
			$this->Actions->dataBind();
		}
	}

	public function switchView($sender, $param) {
		$_SESSION['view' . $this->getParent()->ID] = ($this->Simple->Checked === true) ? self::NORMAL_VIEW : self::DETAIL_VIEW;
		$_SESSION['limit' . $this->getParent()->ID] = $this->Limit->SelectedValue;
		$this->getParent()->prepareData(true);
	}

	public function action($sender, $param) {
		if(method_exists($this->getParent(), 'executeAction')) {
			$this->getParent()->executeAction($this->Actions->SelectedValue, $sender, $param);
		}
	}
}
?>
