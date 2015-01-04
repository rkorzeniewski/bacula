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
 
Prado::using('System.Web.UI.ActiveControls.TActiveDropDownList');
Prado::using('System.Web.UI.ActiveControls.TActiveTextBox');
Prado::using('System.Web.UI.ActiveControls.TActivePanel');
Prado::using('System.Web.UI.ActiveControls.TActiveLabel');
Prado::using('System.Web.UI.ActiveControls.TActiveButton');
Prado::using('System.Web.UI.ActiveControls.TActiveImage');
Prado::using('System.Web.UI.ActiveControls.TActiveTableCell');

class ConfigurationWizard extends BaculumPage
{
	public $firstRun;
	public $applicationConfig;

	const DEFAULT_DB_NAME = 'bacula';
	const DEFAULT_DB_LOGIN = 'bacula';
	const DEFAULT_BCONSOLE_BIN = '/usr/sbin/bconsole';
	const DEFAULT_BCONSOLE_CONF = '/etc/bacula/bconsole.conf';
	const DEFAULT_BCONSOLE_CONF_CUSTOM = '/etc/bacula/bconsole-{user}.conf';

	public function onInit($param) {
		parent::onInit($param);
		$this->Lang->SelectedValue = $_SESSION['language'];
		$this->firstRun = !$this->getModule('configuration')->isApplicationConfig();
		$this->applicationConfig = $this->getModule('configuration')->getApplicationConfig();
		if($this->firstRun === false && $this->User->getIsAdmin() === false) {
			die('Access denied.');
		}
	}

	public function onLoad($param) {
		parent::onLoad($param);
		$this->Licence->Text = $this->getModule('misc')->getLicence();
		$this->Port->setViewState('port', $this->Port->Text);
		if(!$this->IsPostBack && !$this->IsCallBack) {
			if($this->firstRun === true) {
				$this->DBName->Text = self::DEFAULT_DB_NAME;
				$this->Login->Text = self::DEFAULT_DB_LOGIN;
				$this->BconsolePath->Text = self::DEFAULT_BCONSOLE_BIN;
				$this->BconsoleConfigPath->Text = self::DEFAULT_BCONSOLE_CONF;
				$this->BconsoleConfigCustomPath->Text = self::DEFAULT_BCONSOLE_CONF_CUSTOM;
			} else {
				$this->DBType->SelectedValue = $this->getPage()->applicationConfig['db']['type'];
				$this->DBName->Text = $this->applicationConfig['db']['name'];
				$this->Login->Text = $this->applicationConfig['db']['login'];
				$this->Password->Text = $this->applicationConfig['db']['password'];
				$this->IP->Text = $this->applicationConfig['db']['ip_addr'];
				$this->Port->Text = $this->applicationConfig['db']['port'];
				$this->Port->setViewState('port', $this->applicationConfig['db']['port']);
				$this->DBPath->Text = $this->applicationConfig['db']['path'];
				$this->BconsolePath->Text = $this->applicationConfig['bconsole']['bin_path'];
				$this->BconsoleConfigPath->Text = $this->applicationConfig['bconsole']['cfg_path'];
				$this->BconsoleConfigCustomPath->Text = array_key_exists('cfg_custom_path', $this->applicationConfig['bconsole']) ? $this->applicationConfig['bconsole']['cfg_custom_path'] : self::DEFAULT_BCONSOLE_CONF_CUSTOM;
				$this->UseSudo->Checked = $this->getPage()->applicationConfig['bconsole']['use_sudo'] == 1;
				$this->PanelLogin->Text = $this->applicationConfig['baculum']['login'];
				$this->PanelPassword->Text = $this->applicationConfig['baculum']['password'];
				$this->RetypePanelPassword->Text = $this->applicationConfig['baculum']['password'];
			}
		}
	}

	public function NextStep($sender, $param) {
	}
	
	public function PreviousStep($sender, $param) {
	}

	public function wizardStop($sender, $param) {
		$this->goToDefaultPage();
	}

	public function wizardCompleted() {
		$cfgData = array('db' => array(), 'bconsole' => array(), 'baculum' => array());
		$cfgData['db']['type'] = $this->DBType->SelectedValue;
		$cfgData['db']['name'] = $this->DBName->Text;
		$cfgData['db']['login'] = $this->Login->Text;
		$cfgData['db']['password'] = $this->Password->Text;
		$cfgData['db']['ip_addr'] = $this->IP->Text;
		$cfgData['db']['port'] = $this->Port->Text;
		$cfgData['db']['path'] = $this->Application->getModule('configuration')->isSQLiteType($cfgData['db']['type']) ? $this->DBPath->Text : '';
		$cfgData['bconsole']['bin_path'] = $this->BconsolePath->Text;
		$cfgData['bconsole']['cfg_path'] = $this->BconsoleConfigPath->Text;
		$cfgData['bconsole']['cfg_custom_path'] = $this->BconsoleConfigCustomPath->Text;
		$cfgData['bconsole']['use_sudo'] = (integer)($this->UseSudo->Checked === true);
		$cfgData['baculum']['login'] = $this->PanelLogin->Text;
		$cfgData['baculum']['password'] = $this->PanelPassword->Text;
		$cfgData['baculum']['debug'] = isset($this->applicationConfig['baculum']['debug']) ? $this->applicationConfig['baculum']['debug'] : "0";
		$cfgData['baculum']['lang'] = $_SESSION['language'];
		$ret = $this->getModule('configuration')->setApplicationConfig($cfgData);
		if($ret === true) {
			if($this->getModule('configuration')->isUsersConfig() === true) { // version with users config file, so next is try to auto-login
				$previousUser = ($this->firstRun === false) ? $this->applicationConfig['baculum']['login'] : null;
				$this->getModule('configuration')->setUsersConfig($cfgData['baculum']['login'], $cfgData['baculum']['password'], $this->firstRun, $previousUser);
				// Automatic login after finish wizard.
				$http_protocol = isset($_SERVER['HTTPS']) && !empty($_SERVER['HTTPS']) ? 'https' : 'http';
				$urlPrefix = $this->Application->getModule('friendly-url')->getUrlPrefix();
				$location = sprintf("%s://%s:%s@%s:%d%s", $http_protocol, $cfgData['baculum']['login'], $cfgData['baculum']['password'], $_SERVER['SERVER_NAME'], $_SERVER['SERVER_PORT'], $urlPrefix);
				header("Location: $location");
				exit();
			} else { // standard version (user defined auth method)
				$this->goToDefaultPage();
			}
		}
	}

	public function setDBType($sender, $param) {
		$db = $this->DBType->SelectedValue;
		$this->setLogin($db);
		$this->setPassword($db);
		$this->setIP($db);
		$this->setDefaultPort($db);
		$this->setDBPath($db);
	}

	public function setLogin($db) {
		$this->Login->Enabled = ($this->getModule('configuration')->isSQLiteType($db) === false);
	}

	public function setPassword($db) {
		$this->Password->Enabled = ($this->getModule('configuration')->isSQLiteType($db) === false);
	}

	public function setIP($db) {
		$this->IP->Enabled = ($this->getModule('configuration')->isSQLiteType($db) === false);
	}

	public function setDefaultPort($db) {
		$configuration = $this->Application->getModule('configuration');
		$port = null;
		if($configuration->isPostgreSQLType($db) === true) {
			$port = $configuration::PGSQL_PORT;
		} elseif($configuration->isMySQLType($db) === true) {
			$port = $configuration::MYSQL_PORT;
		} elseif($configuration->isSQLiteType($db) === true) {
			$port = $configuration::SQLITE_PORT;
		}

		$prevPort = $this->Port->getViewState('port');

		if(is_null($port)) {
			$this->Port->Text = '';
			$this->Port->Enabled = false;
		} else {
			$this->Port->Enabled = true;
			$this->Port->Text = (empty($prevPort)) ? $port : $prevPort;
		}
		$this->Port->setViewState('port', '');
	}

	public function setDBPath($db) {
		if($this->getModule('configuration')->isSQLiteType($db) === true) {
			$this->DBPath->Enabled = true;
			$this->DBPathField->Display = 'Fixed';
		} else {
			$this->DBPath->Enabled = false;
			$this->DBPathField->Display = 'Hidden';
		}
	}

	public function setLang($sender, $param) {
		$_SESSION['language'] = $sender->SelectedValue;
		$this->goToPage('ConfigurationWizard');
	}

	public function validateAdministratorPassword($sender, $param) {
		$sender->Display = ($this->RetypePasswordRequireValidator->IsValid === true && $this->RetypePasswordRegexpValidator->IsValid === true) ? 'Dynamic' : 'None';
		$param->IsValid = ($param->Value === $this->PanelPassword->Text);
	}

	 public function renderPanel($sender, $param) {
		$this->LoginValidator->Display = ($this->Login->Enabled === true) ? 'Dynamic' : 'None';
		$this->PortValidator->Display = ($this->Port->Enabled === true) ? 'Dynamic' : 'None';
		$this->IPValidator->Display = ($this->IP->Enabled === true) ? 'Dynamic' : 'None';
		$this->DBPathValidator->Display = ($this->DBPath->Enabled === true) ? 'Dynamic' : 'None';
		$this->DbTestResultOk->Display = 'None';
		$this->DbTestResultErr->Display = 'None';
		$this->Step2Content->render($param->NewWriter);
	}
	
	public function connectionDBTest($sender, $param) {
		$validation = false;
		$configuration = $this->getModule('configuration');
		$dbParams = array();
		$dbParams['type'] = $this->DBType->SelectedValue;
		if($configuration->isMySQLType($dbParams['type']) === true || $configuration->isPostgreSQLType($dbParams['type']) === true) {
			$dbParams['name'] = $this->DBName->Text;
			$dbParams['login'] = $this->Login->Text;
			$dbParams['password'] = $this->Password->Text;
			$dbParams['host'] = $this->IP->Text;
			$dbParams['port'] = $this->Port->Text;
			$validation = true;
		} elseif($configuration->isSQLiteType($dbParams['type']) === true && !empty($this->DBPath->Text)) {
			$dbParams['path'] = $this->DBPath->Text;
			$validation = true;
		}
		
		$isValidate = ($validation === true) ? $this->getModule('db')->testDbConnection($dbParams) : false;
		$this->DbTestResultOk->Display = ($isValidate === true) ? 'Dynamic' : 'None';
		$this->DbTestResultErr->Display = ($isValidate === false) ? 'Dynamic' : 'None';
		$this->Step2Content->render($param->NewWriter);

	}

	public function connectionBconsoleTest($sender, $param) {
		$result = $this->getModule('bconsole')->testBconsoleCommand(array('version'), $this->BconsolePath->Text, $this->BconsoleConfigPath->Text, $this->UseSudo->Checked)->exitcode;
		$isValidate = ($result === 0);
		$this->BconsoleTestResultOk->Display = ($isValidate === true) ? 'Dynamic' : 'None';
		$this->BconsoleTestResultErr->Display = ($isValidate === false) ? 'Dynamic' : 'None';
		$this->Step4Content->render($param->NewWriter);
	}
}
?>
