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

Prado::using('Application.Class.Database');

class ActiveRecord extends TActiveRecord
{
	public function getDbConnection() {
		$cfg = Database::getDBParams();
		$dsn = Database::getDsnByParams($cfg);
		$dbConnection = new TDbConnection($dsn, $cfg['login'], $cfg['password']);
		$dbConnection->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
		$dbConnection->setAttribute(PDO::ATTR_CASE, PDO::CASE_LOWER);
		return $dbConnection;
	}

	public function getColumnValue($columnName) {
		// column name to lower due to not correct working PDO::CASE_LOWER for SQLite database
		$columnName = strtolower($columnName);
		$value = parent::getColumnValue($columnName);
		return $value;
	}
}
?>