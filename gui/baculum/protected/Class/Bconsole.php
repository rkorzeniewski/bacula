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

Prado::using('Application.Class.ConfigurationManager');
Prado::using('Application.Class.Logging');
Prado::using('Application.Class.Errors');

class Bconsole extends TModule {

	const SUDO = 'sudo';

	const BCONSOLE_COMMAND_PATTERN = "%s%s -c %s %s <<END_OF_DATA\n%s\nquit\n<<END_OF_DATA";

	const BCONSOLE_DIRECTORS_PATTERN = "%s%s -c %s -l";

	const BCONSOLE_CFG_USER_KEYWORD = '{user}';

	private $availableCommands = array('version', 'status', 'list', 'messages', 'show', 'mount', 'umount', 'release', 'prune', 'purge', 'update', 'estimate', 'run', '.bvfs_update', '.bvfs_lsdirs', '.bvfs_lsfiles', '.bvfs_versions', '.bvfs_get_jobids', '.bvfs_restore', '.bvfs_clear_cache', 'restore', 'cancel', 'delete', '.jobs', 'label', 'reload', '.fileset', '.storage', '.client', '.pool');

	private $useSudo = false;

	private $bconsoleCmdPath;

	private $bconsoleCfgPath;

	private $bconsoleCfgCustomPath;

	public function init($config) {
		if($this->Application->getModule('configuration')->isApplicationConfig() === true) {
			$params = ConfigurationManager::getApplicationConfig();
			$useSudo = ((integer)$params['bconsole']['use_sudo'] === 1);
			$bconsoleCmdPath = $params['bconsole']['bin_path'];
			$bconsoleCfgPath = $params['bconsole']['cfg_path'];
			$bconsoleCfgCustomPath = array_key_exists('cfg_custom_path', $params['bconsole']) ? $params['bconsole']['cfg_custom_path'] : null;
			$this->setEnvironmentParams($bconsoleCmdPath, $bconsoleCfgPath, $bconsoleCfgCustomPath, $useSudo);
		}
	}

	private function setEnvironmentParams($bconsoleCmdPath, $bconsoleCfgPath, $bconsoleCfgCustomPath, $useSudo) {
		$this->bconsoleCmdPath = $bconsoleCmdPath;
		$this->bconsoleCfgPath = $bconsoleCfgPath;
		$this->bconsoleCfgCustomPath = $bconsoleCfgCustomPath;
		$this->useSudo = $useSudo;
	}

	private function isCommandValid($command) {
		$command = trim($command);
		return in_array($command, $this->availableCommands);
	}

	private function prepareResult(array $output, $exitcode, $bconsoleCommand) {
		array_pop($output); // deleted 'quit' bconsole command
		for($i = 0; $i < count($output); $i++) {
			if(strstr($output[$i], $bconsoleCommand) == false) {
				unset($output[$i]);
			} else {
				break;
			}
		}
		$output = count($output) > 1 ? array_values($output) : array_shift($output);
		return (object)array('output' => $output, 'exitcode' => $exitcode);
	}

	public function bconsoleCommand($director, array $command, $user = null) {
		$baseCommand = count($command) > 0 ? $command[0] : null;
		if($this->isCommandValid($baseCommand) === true) {
			$result = $this->execCommand($director, $command, $user);
		} else {
			$result = $this->prepareResult(array(BconsoleError::MSG_ERROR_INVALID_COMMAND, ''), BconsoleError::ERROR_INVALID_COMMAND, ' ');
		}
		return $result;
	}

	private function execCommand($director, array $command, $user) {
		if(!is_null($director) && $this->isValidDirector($director) === false) {
			$output = array(BconsoleError::MSG_ERROR_INVALID_DIRECTOR, '');
			$exitcode = BconsoleError::ERROR_INVALID_DIRECTOR;
			$result = $this->prepareResult($output, $exitcode, ' ');
		} else {
			$dir = is_null($director) ? '': '-D ' . $director;
			$sudo = ($this->useSudo === true) ? self::SUDO . ' ' : '';
			$bconsoleCommand = implode(' ', $command);
			if(!is_null($this->bconsoleCfgCustomPath) && !is_null($user)) {
				$this->bconsoleCfgPath = str_replace(self::BCONSOLE_CFG_USER_KEYWORD, $user, $this->bconsoleCfgCustomPath);
			}
			$cmd = sprintf(self::BCONSOLE_COMMAND_PATTERN, $sudo, $this->bconsoleCmdPath, $this->bconsoleCfgPath, $dir, $bconsoleCommand);
			exec($cmd, $output, $exitcode);
			if($exitcode != 0) {
				$output = array(BconsoleError::MSG_ERROR_BCONSOLE_CONNECTION_PROBLEM, '');
				$exitcode = BconsoleError::ERROR_BCONSOLE_CONNECTION_PROBLEM;
				$result = $this->prepareResult($output, $exitcode, ' ');
			} else {
				$result = $this->prepareResult($output, $exitcode, $bconsoleCommand);
			}
		}
		$this->Application->getModule('logging')->log($cmd, $result, Logging::CATEGORY_EXECUTE, __FILE__, __LINE__);

		return $result;
	}

	public function getDirectors() {
		$sudo = ($this->useSudo === true) ? self::SUDO . ' ' : '';
		$cmd = sprintf(self::BCONSOLE_DIRECTORS_PATTERN, $sudo, $this->bconsoleCmdPath, $this->bconsoleCfgPath);
		exec($cmd, $output, $exitcode);
		if($exitcode != 0) {
			$output = array(BconsoleError::MSG_ERROR_BCONSOLE_CONNECTION_PROBLEM, '');
			$exitcode = BconsoleError::ERROR_BCONSOLE_CONNECTION_PROBLEM;
		}
		$result = (object)array('output' => $output, 'exitcode' => $exitcode);
		return $result;
	}

	private function isValidDirector($director) {
		$ret = in_array($director, $this->getDirectors()->output);
		return $ret;
	}

	public function testBconsoleCommand(array $command, $bconsoleCmdPath, $bconsoleCfgPath, $useSudo) {
		$this->setEnvironmentParams($bconsoleCmdPath, $bconsoleCfgPath, null, $useSudo);
		$director = array_shift($this->getDirectors()->output);
		return $this->bconsoleCommand($director, $command);		
	}
}
?>