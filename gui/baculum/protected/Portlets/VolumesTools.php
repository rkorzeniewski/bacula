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
Prado::using('System.Web.UI.ActiveControls.TActiveRadioButton');
Prado::using('System.Web.UI.ActiveControls.TActiveTextBox');
Prado::using('System.Web.UI.ActiveControls.TActiveDropDownList');
Prado::using('Application.Portlets.Portlets');

class VolumesTools extends Portlets {

	public $labelVolumePattern = '^[0-9a-zA-Z\-_]+$';
	public $slotsPattern = '^[0-9\-\,]+$';
	public $drivePattern = '^[0-9]+$';

	public function onLoad($param) {
		parent::onLoad($param);
		if(!$this->getPage()->IsPostBack) {
			$storages = $this->Application->getModule('api')->get(array('storages'))->output;
			$storagesList = array();
			foreach($storages as $storage) {
				$storagesList[$storage->storageid] = $storage->name;
			}
			$this->StorageLabel->dataSource = $storagesList;
			$this->StorageLabel->dataBind();
			$this->StorageUpdateSlots->dataSource = $storagesList;
			$this->StorageUpdateSlots->dataBind();
			$this->StorageUpdateSlotsScan->dataSource = $storagesList;
			$this->StorageUpdateSlotsScan->dataBind();

			$pools = $this->Application->getModule('api')->get(array('pools'))->output;
			$poolsList = array();
			foreach($pools as $pool) {
				$poolsList[$pool->poolid] = $pool->name;
			}
			$this->PoolLabel->dataSource = $poolsList;
			$this->PoolLabel->dataBind();
		}
	}

	public function labelVolume($sender, $param) {
		if($this->LabelNameValidator->isValid === false) {
			return;
		}
		$cmd = array('label');
		if($this->Barcodes->Checked == true) {
			$cmd[] = 'barcodes';
			$cmd[] = 'slots="' . $this->SlotsLabel->Text . '"';
		} else {
			$cmd[] = 'volume="' . $this->LabelName->Text . '"';
		}
		$cmd[] = 'drive="' . $this->DriveLabel->Text . '"';
		$cmd[] = 'storage="'. $this->StorageLabel->SelectedItem->Text . '"';
		$cmd[] = 'pool="'. $this->PoolLabel->SelectedItem->Text . '"';
		$this->getPage()->Console->CommandLine->Text = implode(' ', $cmd);
		$this->getPage()->Console->sendCommand($sender, $param);
	}

	public function updateSlots($sender, $param) {
		$cmd = array('update');
		$cmd[] = 'slots="' . $this->SlotsUpdateSlots->Text . '"';
		$cmd[] = 'drive="' . $this->DriveUpdateSlots->Text . '"';
		$cmd[] = 'storage="'. $this->StorageUpdateSlots->SelectedItem->Text . '"';
		$this->getPage()->Console->CommandLine->Text = implode(' ', $cmd);
		$this->getPage()->Console->sendCommand($sender, $param);
	}

	public function updateSlotsScan($sender, $param) {
		$cmd = array('update');
		$cmd[] = 'slots="' . $this->SlotsUpdateSlotsScan->Text . '"';
		$cmd[] = 'scan';
		$cmd[] = 'drive="' . $this->DriveUpdateSlotsScan->Text . '"';
		$cmd[] = 'storage="'. $this->StorageUpdateSlotsScan->SelectedItem->Text . '"';
		$this->getPage()->Console->CommandLine->Text = implode(' ', $cmd);
		$this->getPage()->Console->sendCommand($sender, $param);
	}

	public function setBarcodes($sender, $param) {
		$this->LabelNameField->Display = $sender->Checked === false ? 'Dynamic' : 'None';
		$this->BarcodeSlotsField->Display = $sender->Checked === true ? 'Dynamic' : 'None';
	}

	public function setLabelVolume($sender, $param) {
		$this->Labeling->Display = $sender->Checked === true ? 'Dynamic' : 'None';
		$this->UpdatingSlots->Display = 'None';
		$this->UpdatingSlotsScan->Display = 'None';
	}

	public function setUpdateSlots($sender, $param) {
		$this->UpdatingSlots->Display = $sender->Checked === true ? 'Dynamic' : 'None';
		$this->Labeling->Display = 'None';
		$this->UpdatingSlotsScan->Display = 'None';
	}

	public function setUpdateSlotsScan($sender, $param) {
		$this->UpdatingSlotsScan->Display = $sender->Checked === true ? 'Dynamic' : 'None';
		$this->UpdatingSlots->Display = 'None';
		$this->Labeling->Display = 'None';
	}

	public function labelNameValidator($sender, $param) {
		$isValid = true;
		if($this->LabelVolume->Checked === true && $this->Barcodes->Checked === false) {
			$isValid = preg_match('/'. $this->labelVolumePattern . '/', $this->LabelName->Text) === 1;
		}
		$param->setIsValid($isValid);
	}

	public function slotsLabelValidator($sender, $param) {
		$isValid = true;
		if($this->LabelVolume->Checked === true && $this->Barcodes->Checked === true) {
			$isValid = preg_match('/' . $this->slotsPattern . '/', $this->SlotsLabel->Text) === 1;
		}
		$param->setIsValid($isValid);
	}

	public function driveLabelValidator($sender, $param) {
		$isValid = true;
		if($this->LabelVolume->Checked === true) {
			$isValid = preg_match('/' . $this->drivePattern . '/', $this->DriveLabel->Text) === 1;
		}
		$param->setIsValid($isValid);
	}

	public function slotsUpdateSlotsValidator($sender, $param) {
		$isValid = true;
		if($this->UpdateSlots->Checked === true) {
			$isValid = preg_match('/' . $this->slotsPattern . '/', $this->SlotsUpdateSlots->Text) === 1;
		}
		$param->setIsValid($isValid);
	}

	public function driveUpdateSlotsValidator($sender, $param) {
		$isValid = true;
		if($this->UpdateSlots->Checked === true) {
			$isValid = preg_match('/' . $this->drivePattern . '/', $this->DriveUpdateSlots->Text) === 1;
		}
		$param->setIsValid($isValid);
	}

	public function slotsUpdateSlotsScanValidator($sender, $param) {
		$isValid = true;
		if($this->UpdateSlotsScan->Checked === true) {
			$isValid = preg_match('/' . $this->slotsPattern . '/', $this->SlotsUpdateSlotsScan->Text) === 1;
		}
		$param->setIsValid($isValid);
	}

	public function driveUpdateSlotsScanValidator($sender, $param) {
		$isValid = true;
		if($this->UpdateSlotsScan->Checked === true) {
			$isValid = preg_match('/' . $this->drivePattern . '/', $this->DriveUpdateSlotsScan->Text) === 1;
		}
		$param->setIsValid($isValid);
	}
}

?>
