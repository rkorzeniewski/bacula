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
 
Prado::using('System.Web.UI.ActiveControls.TActiveCustomValidator');
Prado::using('Application.Portlets.Portlets');

class StorageConfiguration extends Portlets {

	public function onInit($param) {
		parent::onInit($param);
		$this->Mount->setActionClass($this);
		$this->Release->setActionClass($this);
		$this->Umount->setActionClass($this);
		$this->Status->setActionClass($this);
	}

	public function configure($storageId) {
		$storagedata = $this->Application->getModule('api')->get(array('storages', 'show', $storageId))->output;
		$this->ShowStorage->Text = implode(PHP_EOL, $storagedata);
		$storage = $this->Application->getModule('api')->get(array('storages', $storageId))->output;
		$this->StorageName->Text = $storage->name;
		$this->StorageID->Text = $storage->storageid;
		$this->AutoChanger->Visible = (boolean)$storage->autochanger;
	}

	public function save($sender, $param) {
		$isValid = $this->DriveValidator->IsValid === true && $this->SlotValidator->IsValid === true;
		switch($sender->getParent()->ID) {
			case $this->Status->ID: {
				$status = $this->Application->getModule('api')->get(array('storages', 'status', $this->StorageID->Text))->output;
				$this->ShowStorage->Text = implode(PHP_EOL, $status);
				break;
			}
			case $this->Mount->ID: {
				if($isValid === false) {
					return;
				}
				$drive = ($this->AutoChanger->Visible === true) ? intval($this->Drive->Text) : 0;
				$slot = ($this->AutoChanger->Visible === true) ? intval($this->Slot->Text) : 0;
				$mount = $this->Application->getModule('api')->get(array('storages', 'mount', $this->StorageID->Text, $drive, $slot))->output;
				$this->ShowStorage->Text = implode(PHP_EOL, $mount);
				break;
			}
			case $this->Umount->ID: {
				if($isValid === false) {
					return;
				}
				$drive = ($this->AutoChanger->Visible === true) ? intval($this->Drive->Text) : 0;
				$umount = $this->Application->getModule('api')->get(array('storages', 'umount', $this->StorageID->Text, $drive))->output;
				$this->ShowStorage->Text = implode(PHP_EOL, $umount);
				break;
			}
			case $this->Release->ID: {
				$release = $this->Application->getModule('api')->get(array('storages', 'release', $this->StorageID->Text))->output;
				$this->ShowStorage->Text = implode(PHP_EOL, $release);
				break;
			}
		}
	}

	public function driveValidator($sender, $param) {
		$isValid = is_numeric($this->Drive->Text);
		$param->setIsValid($isValid);
	}

	public function slotValidator($sender, $param) {
		$isValid = is_numeric($this->Slot->Text);
		$param->setIsValid($isValid);
	}
}
?>