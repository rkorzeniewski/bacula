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

Prado::using('System.Web.UI.ActiveControls.TActiveLabel');
Prado::using('System.Web.UI.ActiveControls.TActiveTextBox');
Prado::using('Application.Portlets.Portlets');

class PoolConfiguration extends Portlets {

	public function configure($poolId) {
		$pooldata = $this->Application->getModule('api')->get(array('pools', $poolId))->output;
		$this->PoolName->Text = $pooldata->name;
		$this->PoolID->Text = $pooldata->poolid;
		$this->Enabled->Checked = $pooldata->enabled == 1;
		$this->MaxVolumes->Text = $pooldata->maxvols;
		$this->MaxVolJobs->Text = $pooldata->maxvoljobs;
		$this->MaxVolBytes->Text = $pooldata->maxvolbytes;
		$this->UseDuration->Text = intval($pooldata->voluseduration / 3600); // conversion to hours;
		$this->RetentionPeriod->Text = intval($pooldata->volretention / 3600); // conversion to hours;
		$this->LabelFormat->Text = $pooldata->labelformat;
		$pools = $this->Application->getModule('api')->get(array('pools'))->output;
		$poolList = array('none' => Prado::localize('select pool'));
		foreach($pools as $pool) {
			$poolList[$pool->poolid] = $pool->name;
		}
		$this->ScratchPool->dataSource = $poolList;
		$this->ScratchPool->SelectedValue = $pooldata->scratchpoolid;
		$this->ScratchPool->dataBind();
		$this->RecyclePool->dataSource = $poolList;
		$this->RecyclePool->SelectedValue = $pooldata->recyclepoolid;
		$this->RecyclePool->dataBind();
		$this->Recycle->Checked = $pooldata->recycle == 1;
		$this->AutoPrune->Checked = $pooldata->autoprune == 1;
		$this->ActionOnPurge->Checked = $pooldata->actiononpurge == 1;
	}

	public function restore_configuration($sender, $param) {
		$this->Application->getModule('api')->set(array('pools', 'update', $this->PoolID->Text),  array(''));
	}

	public function update_volumes($sender, $param) {
		if($this->MaxVolumesValidator->IsValid === false || $this->MaxVolJobsValidator->IsValid === false || $this->MaxVolBytesValidator->IsValid === false || $this->UseDurationValidator->IsValid === false || $this->RetentionPeriodValidator->IsValid === false || $this->LabelFormatValidator->IsValid === false) {
			return false;
		}
		$pooldata = array();
		$pooldata['poolid'] = $this->PoolID->Text;
		$pooldata['enabled'] = (integer)$this->Enabled->Checked;
		$pooldata['maxvols'] = $this->MaxVolumes->Text;
		$pooldata['maxvoljobs'] = $this->MaxVolJobs->Text;
		$pooldata['maxvolbytes'] = $this->MaxVolBytes->Text;
		$pooldata['voluseduration'] = $this->UseDuration->Text * 3600; // conversion to seconds
		$pooldata['volretention'] = $this->RetentionPeriod->Text * 3600; // conversion to seconds
		$pooldata['labelformat'] = $this->LabelFormat->Text;
		$pooldata['scratchpoolid'] = (integer)$this->ScratchPool->SelectedValue;
		$pooldata['recyclepoolid'] = (integer)$this->RecyclePool->SelectedValue;
		$pooldata['recycle'] = (integer)$this->Recycle->Checked;
		$pooldata['autoprune'] = (integer)$this->AutoPrune->Checked;
		$pooldata['actiononpurge'] = (integer)$this->ActionOnPurge->Checked;
		$this->Application->getModule('api')->set(array('pools', $this->PoolID->Text), $pooldata);
		$this->Application->getModule('api')->set(array('pools', 'update', 'volumes', $this->PoolID->Text),  array(''));
	}

	public function apply($sender, $param) {
			if($this->MaxVolumesValidator->IsValid === false || $this->MaxVolJobsValidator->IsValid === false || $this->MaxVolBytesValidator->IsValid === false || $this->UseDurationValidator->IsValid === false || $this->RetentionPeriodValidator->IsValid === false || $this->LabelFormatValidator->IsValid === false) {
				return false;
			}
			$pooldata = array();
			$pooldata['poolid'] = $this->PoolID->Text;
			$pooldata['enabled'] = (integer)$this->Enabled->Checked;
			$pooldata['maxvols'] = $this->MaxVolumes->Text;
			$pooldata['maxvoljobs'] = $this->MaxVolJobs->Text;
			$pooldata['maxvolbytes'] = $this->MaxVolBytes->Text;
			$pooldata['voluseduration'] = $this->UseDuration->Text * 3600; // conversion to seconds
			$pooldata['volretention'] = $this->RetentionPeriod->Text * 3600; // conversion to seconds
			$pooldata['labelformat'] = $this->LabelFormat->Text;
			$pooldata['scratchpoolid'] = (integer)$this->ScratchPool->SelectedValue;
			$pooldata['recyclepoolid'] = (integer)$this->RecyclePool->SelectedValue;
			$pooldata['recycle'] = (integer)$this->Recycle->Checked;
			$pooldata['autoprune'] = (integer)$this->AutoPrune->Checked;
			$pooldata['actiononpurge'] = (integer)$this->ActionOnPurge->Checked;
			$this->Application->getModule('api')->set(array('pools', $this->PoolID->Text), $pooldata);

	}

	public function maxVolumesValidator($sender, $param) {
		$isValid = preg_match('/^\d+$/', $this->MaxVolumes->Text) && $this->MaxVolumes->Text >= 0;
		$param->setIsValid($isValid);
	}

	public function maxVolJobsValidator($sender, $param) {
		$isValid = preg_match('/^\d+$/', $this->MaxVolJobs->Text) && $this->MaxVolJobs->Text >= 0;
		$param->setIsValid($isValid);
	}

	public function maxVolBytesValidator($sender, $param) {
		$isValid = preg_match('/^\d+$/', $this->MaxVolBytes->Text) && $this->MaxVolBytes->Text >= 0;
		$param->setIsValid($isValid);
	}

	public function useDurationValidator($sender, $param) {
		$isValid = preg_match('/^\d+$/', $this->UseDuration->Text) && $this->UseDuration->Text >= 0;
		$param->setIsValid($isValid);
	}

	public function retentionPeriodValidator($sender, $param) {
		$isValid = preg_match('/^\d+$/', $this->RetentionPeriod->Text) && $this->RetentionPeriod->Text >= 0;
		$param->setIsValid($isValid);
	}

	public function labelFormatValidator($sender, $param) {
		$value = trim($this->LabelFormat->Text);
		$isValid = !empty($value);
		$param->setIsValid($isValid);
	}
}
?>
