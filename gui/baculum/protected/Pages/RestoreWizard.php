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
 
Prado::using('System.Exceptions.TException');
Prado::using('System.Web.UI.ActiveControls.TActiveDropDownList');
Prado::using('System.Web.UI.ActiveControls.TActivePanel');
Prado::using('System.Web.UI.ActiveControls.TActiveImageButton');
Prado::using('System.Web.UI.ActiveControls.TDropContainer');
Prado::using('System.Web.UI.ActiveControls.TDraggable');
Prado::using('System.Web.UI.ActiveControls.TActiveRadioButton');
Prado::using('System.Web.UI.ActiveControls.TActiveDataGrid');
Prado::using('System.Web.UI.ActiveControls.TCallback');

class RestoreWizard extends BaculumPage
{
	private $jobLevelsToRestore = array('F' => 'Full', 'I' => 'Incremental', 'D'=> 'Differential');
	private $jobs = null;
	private $filesets = null;
	private $storages = null;
	private $clients = null;
	private $browserRootDir = array('name' => '.', 'type' => 'dir');
	private $browserUpDir = array('name' => '..', 'type' => 'dir');

	const BVFS_PATH_PREFIX = 'b2';

	public function onInit($param) {
		parent::onInit($param);
		if(!$this->IsPostBack && !$this->IsCallBack) {
			$this->setBrowserFiles(array());
			$this->setFileVersions(array());
			$this->setFilesToRestore(array());
			$this->markFileToRestore(null, null);
			$_SESSION['restore_path'] = array();
		}
	}

	public function onLoad($param) {
		parent::onLoad($param);
			if($this->RestoreWizard->ActiveStepIndex == 0) {
				$this->jobs = $this->getModule('api')->get(array('jobs'))->output;
				$this->filesets = $this->getModule('api')->get(array('filesets'))->output;
			}
			$this->clients = $this->getModule('api')->get(array('clients'))->output;

			if(!$this->IsCallBack && (($this->RestoreWizard->ActiveStepIndex == 1 && $this->getPage()->BackupToRestore->ItemCount > 0) || $this->RestoreWizard->ActiveStepIndex == 3)) {
				$this->setFileVersions(array());
				$this->setBrowserPath('');
				$this->prepareFilesToRestore();
				$this->prepareVersionsToRestore();
			}
	}

	public function addFileToRestore($sender, $param) {
		$control=$param->getDroppedControl();
        $item=$control->getNamingContainer();
		list(, , , $dragElementID, , ) = explode('_', $param->getDragElementID(), 6); // I know that it is ugly.
		if($dragElementID == $this->VersionsDataGrid->ID) {
			$fileid = $this->VersionsDataGrid->getDataKeys()->itemAt($item->getItemIndex());
			$fileProperties = $this->getFileVersions($fileid);
		} else {
			$fileid = $this->DataGridFiles->getDataKeys()->itemAt($item->getItemIndex());
			$fileProperties = $this->getBrowserFile($fileid);
		}
		if($fileProperties['name'] != $this->browserRootDir['name'] && $fileProperties['name'] != $this->browserUpDir['name']) {
			$this->markFileToRestore($fileid, $fileProperties);
			$this->prepareFilesToRestore();
		}
	}

	public function removeSelectedFile($sender, $param) {
		$fileid = $param->CallbackParameter;
		$this->unmarkFileToRestore($fileid);
		$this->prepareFilesToRestore();
	}

	public function getVersions($sender, $param) {
		list($filename, $pathid, $filenameid, $jobid) = explode('|', $param->CallbackParameter, 4);
		if($filenameid == 0) {
			$this->setBrowserPath($filename);
			return;
		}
		$clientname = null;
		foreach($this->clients as $client) {
			if($client->clientid == $this->BackupClientName->SelectedValue) {
				$clientname = $client->name;
				break;
			}
		}
		$versions = $this->getModule('api')->get(array('bvfs', 'versions', $clientname, $jobid, $pathid, $filenameid))->output;
		$fileVersions = $this->getModule('misc')->parseFileVersions($filename, $versions);
		$this->setFileVersions($fileVersions);
		$this->VersionsDataGrid->dataSource = $fileVersions;
		$this->VersionsDataGrid->dataBind();
		$this->prepareFilesToRestore();
	}

	public function refreshSelectedFiles($sender, $param) {
		$this->prepareFilesToRestore();
		$this->SelectedVersionsDropper->render($param->NewWriter);
	}

	public function NextStep($sender, $param) {
	}
	
	public function PreviousStep($sender, $param) {
	}

	public function wizardStop($sender, $param) {
		$this->goToDefaultPage();
	}

	public function setJobs($sender, $param) {
		$jobsList = $jobsGroupList = $fileSetsList = array();
		if(is_array($this->jobs)) {
			foreach($this->jobs as $job) {
				if(array_key_exists($job->level, $this->jobLevelsToRestore) && $job->type == 'B' && $job->jobstatus == 'T' && $job->clientid == $this->BackupClientName->SelectedValue) {
					$jobsList[$job->jobid] = sprintf('[%s] %s, %s, %s', $job->jobid, $job->name, $this->jobLevelsToRestore[$job->level], $job->endtime);
					$jobsGroupList[$job->name] = $job->name;
					//$fileSetsList[$job->filesetid] = $this->getFileSet($job->filesetid)->fileset;
				}
			}
		}
		
		foreach($this->filesets as $director => $filesets) {
			$fileSetsList = array_merge($filesets, $fileSetsList);
		}

		$this->BackupToRestore->dataSource = $jobsList;
		$this->BackupToRestore->dataBind();
		$this->GroupBackupToRestore->dataSource = $jobsGroupList;
		$this->GroupBackupToRestore->dataBind();
		$this->GroupBackupFileSet->dataSource = array_combine($fileSetsList, $fileSetsList);
		$this->GroupBackupFileSet->dataBind();
		$this->setRestoreClients($sender, $param);
	}

	public function setBackupClients($sender, $param) {
		if(!$this->IsPostBack) {
			$clientsList = array();
			foreach($this->clients as $client) {
				$clientsList[$client->clientid] = $client->name;
			}
			$this->BackupClientName->dataSource = $clientsList;
			$this->BackupClientName->dataBind();
		}
	}

	public function setRestoreClients($sender, $param) {
		if(!$this->IsPostBack || $sender->ID == $this->BackupClientName->ID) {
			$clientsList = array();
			foreach($this->clients as $client) {
				$clientsList[$client->clientid] = $client->name;
			}
			$this->RestoreClient->SelectedValue = $this->BackupClientName->SelectedValue;
			$this->RestoreClient->dataSource = $clientsList;
			$this->RestoreClient->dataBind();
		}
	}

	public function getFileSet($filesetId) {
		$filesetObj = null;
		foreach($this->filesets as $fileset) {
			if($fileset->filesetid == $filesetId) {
				$filesetObj = $fileset;
				break;
			}
		}
		return $filesetObj;
	}

	public function setStorage($sender, $param) {
		if(!$this->IsPostBack) {
			$storagesList = array();
			$storages = $this->getModule('api')->get(array('storages'))->output;
			foreach($storages as $storage) {
				$storagesList[$storage->storageid] = $storage->name;
			}
			$this->GroupBackupStorage->dataSource = $storagesList;
			$this->GroupBackupStorage->dataBind();
		}
	}

	public function setPool($sender, $param) {
		if(!$this->IsPostBack) {
			$poolsList = array();
			$pools = $this->getModule('api')->get(array('pools'))->output;
			foreach($pools as $pool) {
				$poolsList[$pool->poolid] = $pool->name;
			}
			$this->GroupBackupPool->dataSource = $poolsList;
			$this->GroupBackupPool->dataBind();
		}
	}

	public function setBackupClient($sender, $param) {
		$this->ClientToRestore->SelectedValue = $param->SelectedValue;
	} 

	public function setBackupSelection($sender, $param) {
		$this->GroupBackupToRestoreField->Display = ($sender->ID == $this->GroupBackupSelection->ID) ? 'Dynamic' : 'None';
		$this->BackupToRestoreField->Display = ($sender->ID == $this->OnlySelectedBackupSelection->ID) ? 'Dynamic' : 'None';
		$this->setBrowserFiles(array());
		$this->setFileVersions(array());
		$this->setFilesToRestore(array());
		$this->markFileToRestore(null, null);
		$_SESSION['restore_path'] = array();
	}

	/*public function setGroupBackup($sender, $param) {
		foreach($this->jobs as $job) {
			if($job->name == $sender->SelectedValue) {
				$this->GroupBackupFileSet->SelectedValue = $job->filesetid;
			}
		}
	}*/

	public function resetFileBrowser($sender, $param) {
		$this->markFileToRestore(null, null);
		$this->setBrowserPath($this->browserRootDir['name']);
	}

	private function prepareBrowserFiles($files) {
		$this->setBrowserFiles($files);
		$this->DataGridFiles->dataSource = $files;
		@$this->DataGridFiles->dataBind();
	}

	private function prepareVersionsToRestore() {
		$this->VersionsDataGrid->dataSource = $_SESSION['files_versions'];
		$this->VersionsDataGrid->dataBind();
	}

	private function prepareFilesToRestore() {
		$this->SelectedVersionsDataGrid->dataSource = $_SESSION['restore'];
		$this->SelectedVersionsDataGrid->dataBind();
	}

	private function setBrowserPath($path) {
		if(!empty($path)) {
			if($path == $this->browserUpDir['name']) {
				array_pop($_SESSION['restore_path']);
			} elseif($path == $this->browserRootDir['name']) {
				$_SESSION['restore_path'] = array();
			} else {
				array_push($_SESSION['restore_path'], $path);
			}
		}
		$this->setBrowserLocalizator();
		$this->prepareBrowserContent();
	}

	private function getBrowserPath($stringFormat = false) {
		return ($stringFormat === true) ? implode($_SESSION['restore_path']) : $_SESSION['restore_path'];
	}

	private function setBrowserLocalizator() {
		$localization = $this->getBrowserPath(true);
		$this->PathField->HeaderText = mb_strlen($localization) > 56 ? '...' . mb_substr($localization, -56) : $localization;
	}

	private function prepareBrowserContent() {
		$jobids = $this->getElementaryBackup();
	
		// generating Bvfs may takes a moment
		$this->generateBvfsCacheByJobids($jobids);
		$bvfsDirsList = $this->getModule('api')->get(array('bvfs', 'lsdirs', $jobids, '?path=' .  urlencode($this->getBrowserPath(true)) . ''));
		$bvfsDirs = is_object($bvfsDirsList) ? $bvfsDirsList->output : array();
		$dirs = $this->getModule('misc')->parseBvfsList($bvfsDirs);
		$bvfsFilesList = $this->getModule('api')->get(array('bvfs', 'lsfiles', $jobids, '?path=' .  urlencode($this->getBrowserPath(true)) . ''));
		$bvfsFiles = is_object($bvfsFilesList) ? $bvfsFilesList->output : array();
		$files = $this->getModule('misc')->parseBvfsList($bvfsFiles);
		$elements = array_merge($dirs, $files);
		if(count($this->getBrowserPath()) > 0) {
			array_unshift($elements, $this->browserRootDir);
		}
		$this->prepareBrowserFiles($elements);
	}

	private function getElementaryBackup() {
		$jobids = null;
		if($this->OnlySelectedBackupSelection->Checked === true) {
			$jobs = $this->getModule('api')->get(array('bvfs', 'getjobids', $this->BackupToRestore->SelectedValue));
			$ids = is_object($jobs) ? $jobs->output : array();
			foreach($ids as $jobid) {
				if(preg_match('/^([\d\,]+)$/', $jobid, $match) == 1) {
					$jobids = $match[1];
					break;
				}
			}
		} else {
			$jobsRecent = $this->getModule('api')->get(array('jobs', 'recent', urlencode($this->GroupBackupToRestore->SelectedValue), $this->BackupClientName->SelectedValue));
			if(isset($jobsRecent->output) && count($jobsRecent->output) > 0) {
				$ids = $jobsRecent->output;
				$jobids = implode(',', $ids);
			}
		}

		$completeJobids = (!is_null($jobids)) ? $jobids : $this->BackupToRestore->SelectedValue;
		return $completeJobids;
	}

	private function generateBvfsCacheByJobids($jobids) {
		$this->getModule('api')->set(array('bvfs', 'update', $jobids), array());
	}

	private function setFileVersions($versions) {
		$_SESSION['files_versions'] = $versions;
	}

	private function getFileVersions($fileid) {
		$versions = array();
		foreach($_SESSION['files_versions'] as $file) {
			if(array_key_exists('fileid', $file) && $file['fileid'] == $fileid) {
				$versions = $file;
				break;
			}
		}
		return $versions;
	}

	private function setBrowserFiles($files) {
		$_SESSION['files_browser'] = $files;
	}

	private function getBrowserFile($fileid) {
		$element = array();
		foreach($_SESSION['files_browser'] as $file) {
			if(array_key_exists('fileid', $file) && $file['fileid'] == $fileid) {
				$element = $file;
				break;
			}
		}
		return $element;
	}

	private function markFileToRestore($fileid, $file) {
		if($fileid === null) {
			$_SESSION['restore'] = array();
		} elseif($file['name'] != $this->browserRootDir['name'] && $file['name'] != $this->browserUpDir['name']) {
			$_SESSION['restore'][$fileid] = $file;
		}
	}

	private function unmarkFileToRestore($fileid) {
		if(array_key_exists($fileid, $_SESSION['restore'])) {
			unset($_SESSION['restore'][$fileid]);
		}
	}

	public function getFilesToRestore() {
		return $_SESSION['restore'];
	}

	public function setFilesToRestore($files) {
		$_SESSION['restore'] = $files;
	}

	public function getRestoreElements($asObject = false) {
		$fileids = array();
		$dirids = array();
		foreach($this->getFilesToRestore() as $fileid => $properties) {
			if($properties['type'] == 'dir') {
				$dirids[] = $properties['pathid'];
			} elseif($properties['type'] == 'file') {
				$fileids[] = $fileid;
			}
		}
		$ret = array('fileid' => $fileids, 'dirid' => $dirids);
		if($asObject === true) {
			$ret = (object)$ret;
		}
		return $ret;
	}

	public function wizardCompleted() {
		$jobids = $this->getElementaryBackup();
		$path = self::BVFS_PATH_PREFIX . str_replace(',', '', $jobids);
		$restoreElements = $this->getRestoreElements();
		$cmdProps = array('jobids' => $jobids, 'path' => $path);
		if(count($restoreElements['fileid']) > 0) {
			$cmdProps['fileid'] = implode(',', $restoreElements['fileid']);
		}
		if(count($restoreElements['dirid']) > 0) {
			$cmdProps['dirid'] = implode(',', $restoreElements['dirid']);
		}

		$this->getModule('api')->create(array('bvfs', 'restore'), $cmdProps);
		$restoreProps = array();
		$restoreProps['rpath'] = $path;
		$restoreProps['clientid'] = intval($this->RestoreClient->SelectedValue);
		$restoreProps['fileset'] = $this->GroupBackupFileSet->SelectedValue;
		$restoreProps['priority'] = intval($this->RestoreJobPriority->Text);
		$restoreProps['where'] =  $this->RestorePath->Text;
		$restoreProps['replace'] = $this->ReplaceFiles->SelectedValue;
		
		$ret = $this->getModule('api')->create(array('jobs', 'restore'), $restoreProps);
		$this->goToDefaultPage(array('open' => 'Job'));
	}
}
?>
