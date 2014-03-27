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

Prado::using('Application.Class.Miscellaneous');

class ConfigurationManager extends TModule
{

	/**
	 * Location o application configuration file.
	 */
	const CONFIG_FILE = 'Application.Data.settings';

	/**
	 * PostgreSQL default params.
	 */
	const PGSQL = 'pgsql';
	const PGSQL_NAME = 'PostgreSQL';
	const PGSQL_PORT = 5432;

	/**
	 * MySQL default params.
	 */
	const MYSQL = 'mysql';
	const MYSQL_NAME = 'MySQL';
	const MYSQL_PORT = 3306;

	/**
	 * SQLite default params.
	 */
	const SQLITE = 'sqlite';
	const SQLITE_NAME = 'SQLite';
	const SQLITE_PORT = null;

	/**
	 * Default language for application.
	 */
	const DEFAULT_LANGUAGE = 'en';

	public function getDbNameByType($type) {
		switch($type) {
			case self::PGSQL: $dbName = self::PGSQL_NAME; break;
			case self::MYSQL: $dbName = self::MYSQL_NAME; break;
			case self::SQLITE: $dbName = self::SQLITE_NAME; break;
			default: $dbName = null; break;
		}
		return $dbName;
	}

	public function getPostgreSQLType() {
		return self::PGSQL;
	}

	public function getMySQLType() {
		return self::MYSQL;
	}

	public function getSQLiteType() {
		return self::SQLITE;
	}

	public function isPostgreSQLType($type) {
		return ($type === self::PGSQL);
	}

	public function isMySQLType($type) {
		return ($type === self::MYSQL);
	}

	public function isSQLiteType($type) {
		return ($type === self::SQLITE);
	}

	public function getLanguage() {
		return $this->Application->Parameters['language'];
	}

	public function setLanguage($language) {
		
	}

	/**
	 * Saving application configuration.
	 * 
	 * @access public
	 * @param array $config structure of config file params
	 * @return boolean true if config save is successfully, false if config save is failure
	 */
	public function setApplicationConfig(array $config) {
		$cfgFile = Prado::getPathOfNamespace(self::CONFIG_FILE, '.conf');
		return ($this->Application->getModule('misc')->writeINIFile($cfgFile, $config) != false);
	}

	/**
	 * Getting application configuration.
	 * 
	 * @access public
	 * @return array application configuration
	 */
	public static function getApplicationConfig() {
		$cfgFile = Prado::getPathOfNamespace(self::CONFIG_FILE, '.conf');
		return Miscellaneous::parseINIFile($cfgFile);
	}

	/**
	 * Checking if application configuration file exists.
	 * 
	 * @access public
	 * @return boolean true if file exists, otherwise false
	 */
	public function isApplicationConfig() {
		return file_exists(Prado::getPathOfNamespace(self::CONFIG_FILE, '.conf'));
	}
}
?>