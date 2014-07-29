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
Prado::using('Application.Portlets.Portlets');

class BActiveButton extends Portlets{

	public $actionClass, $text, $validationGroup;
	public $causesValidation = true;

	public function onInit($param) {
		parent::onInit($param);
	}

	public function setActionClass($param) {
		$this->actionClass = $param;
	}

	public function save($sender, $param) {
		$this->actionClass->save($sender, $param);
	}

	public function setCommandName($param) {
		$this->commandName = $param;
	}

	public function setText($param) {
		$this->text = $param;
	}

	public function setCausesValidation($param) {
		$this->causesValidation = $param;
	}

	public function setValidationGroup($param) {
		$this->validationGroup = $param;
	}
}

?>