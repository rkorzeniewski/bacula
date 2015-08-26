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

Prado::using('System.Security.IUserManager');
Prado::using('Application.Class.BaculumUser');

class BaculumUsersManager extends TModule implements IUserManager {

	private $config;

	public function init($config) {
		$this->config = $this->Application->getModule('configuration')->isApplicationConfig() ? $this->Application->getModule('configuration')->getApplicationConfig() : null;
	}

	public function getGuestName() {
		return 'guest';
	}

	public function validateUser($username, $password) {
		return !empty($username);
	}

	public function getUser($username = null) {
		$user = new BaculumUser($this);
		$id = sha1(time());
		$user->setID($id);
		$user->setName($_SERVER['PHP_AUTH_USER']);
		$user->setIsGuest(false);
		if($this->config['baculum']['login'] == $_SERVER['PHP_AUTH_USER'] || is_null($this->config)) {
			$user->setRoles('admin');
		} else {
			$user->setRoles('user');
		}
		return $user;
	}

	public function getUserFromCookie($cookie) {
		return;
	}

	public function saveUserToCookie($cookie) {
		return;
	}
}
?>