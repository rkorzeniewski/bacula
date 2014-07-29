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
 
Prado::using('System.Data.TDbConnection');
Prado::using('Application.Class.ConfigurationManager');

class Database extends TModule {

	const DB_PARAMS_FILE = 'Application.Data.settings';

	public static function getDBParams() {
		$cfg = ConfigurationManager::getApplicationConfig();
		$dbParams = array();
		return array('type' => $cfg['db']['type'], 'name' => $cfg['db']['name'], 'path' => $cfg['db']['path'], 'host' => $cfg['db']['ip_addr'], 'port' => $cfg['db']['port'], 'login' => $cfg['db']['login'], 'password' => $cfg['db']['password']);
	}

	/**
	 * Get Data Source Name (DSN).
	 * 
	 * For SQLite params are:
	 * 	array('dbtype' => 'type', 'dbpath' => 'localization');
	 * For others params are:
	 * 	array('dbType' => 'type', 'dbname' => 'name', 'host' => 'IP or hostname', 'port' => 'database port');
	 * 
	 * @access public
	 * @param array $dbParams params of Data Source Name (DSN)
	 * @return string Data Source Name (DSN)
	 */
	public static function getDsnByParams(array $dbParams) {
		$dsn = array();

		if(array_key_exists('path', $dbParams) && !empty($dbParams['path'])) {
			$dsn[] = $dbParams['type'] . ':' . $dbParams['path'];
		} else {
			$dsn[] =  $dbParams['type'] . ':' . 'dbname=' . $dbParams['name'];

			if(array_key_exists('host', $dbParams)) {
				$dsn[] = 'host=' . $dbParams['host'];
			}

			if(array_key_exists('port', $dbParams)) {
				$dsn[] = 'port=' . $dbParams['port'];
			}
		}

		$dsnFull = implode(';', $dsn);
		return $dsnFull;
	}

	/**
	 * Checking connection to database.
	 * 
	 * @access public
	 * @param array $dbParams params to database connection
	 * @return bool if connection established then returns true, otherwise returns false.
	 */
	public function testDbConnection(array $dbParams) {
		$dsn = self::getDsnByParams($dbParams);
		$isDatabase = false;
		$tablesFormat = null;

		try {
			if(array_key_exists('login', $dbParams) && array_key_exists('password', $dbParams)) {
				$connection = new TDBConnection($dsn, $dbParams['login'], $dbParams['password']);
			} else {
				$connection = new TDBConnection($dsn);
			}

			$connection->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
			$connection->setAttribute(PDO::ATTR_CASE, PDO::CASE_LOWER);
			$connection->Active = true;

			$tablesFormat = $this->getTablesFormat($connection);
			$isDatabase = (is_numeric($tablesFormat) === true && $tablesFormat > 0);
		} catch (TDbException $e) {
			$isDatabase = false;
		}

		if(array_key_exists('password', $dbParams)) {
			// masking database password.
			$dbParams['password'] = preg_replace('/.{1}/', '*', $dbParams['password']);
		}

		Prado::trace(sprintf('DBParams=%s, Connection=%s, TablesFormat=%s', print_r($dbParams, true), var_export($isDatabase, true), var_export($tablesFormat, true)), 'Execute');

		return $isDatabase;
	}

	/**
	 * Get number of Bacula database tables format.
	 * 
	 * @access private
	 * @param TDBConnection $connection handler to database connection
	 * @return integer number of Bacula database tables format
	 */
	private function getTablesFormat(TDBConnection $connection) {
		$query = 'SELECT versionid FROM Version';
		$command = $connection->createCommand($query);
		$result = $command->queryRow();
		$ret = (array_key_exists('versionid', $result) === true) ? $result['versionid'] : false;
		return $ret;
	}
}
?>