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

Prado::using('Application.Class.Errors');

class API extends TModule {

	const API_VERSION = '0.1';

	protected $appCfg;

	private $allowedErrors = array(
		GenericError::ERROR_NO_ERRORS,
		BconsoleError::ERROR_INVALID_COMMAND
	);

	private function getConnection() {
		$ch = curl_init();
		curl_setopt($ch, CURLOPT_SSL_VERIFYHOST, false);
		curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, false);
		curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
		curl_setopt($ch, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
		curl_setopt($ch, CURLOPT_USERPWD, $this->appCfg['baculum']['login'] . ':' . $this->appCfg['baculum']['password']);
		return $ch;
	}

	private function getAPIHeader() {
		return 'X-Baculum-API: ' . self::API_VERSION;
	}

	public function init($config) {
		$this->appCfg = $this->Application->getModule('configuration')->getApplicationConfig();
	}

	private function getURL() {
		$protocol = !empty($_SERVER['HTTPS']) ? 'https' : 'http';
		$host = $_SERVER['SERVER_NAME'];
		$port = $_SERVER['SERVER_PORT'];
		$urlPrefix = $this->Application->getModule('friendly-url')->getUrlPrefix();
		$url = sprintf('%s://%s:%d%s/', $protocol, $host, $port, $urlPrefix);
		return $url;
	}

	private function setParamsToUrl(&$url) {
		$url .= (preg_match('/\?/', $url) === 1 ? '&' : '?' ) . 'director=' . ((array_key_exists('director', $_SESSION)) ? $_SESSION['director'] : '');
		/**
		 * If user is not equal admin user then it is added to URL,
		 * then will be used custom console for this user.
		 */
		if($this->User->getIsAdmin() === false) {
			$url .= '&user=' . $this->User->getName();
		}
		$this->Application->getModule('logging')->log(__FUNCTION__, PHP_EOL . PHP_EOL . 'EXECUTE URL ==> ' . $url . ' <==' . PHP_EOL . PHP_EOL, Logging::CATEGORY_APPLICATION, __FILE__, __LINE__);
	}

	/**
	 * API REQUESTS METHODS (get, set, create, delete)
	 */

	public function get(array $params) {
		$url = $this->getURL() . implode('/', $params);
		$this->setParamsToUrl($url);
		$ch = $this->getConnection();
		curl_setopt($ch, CURLOPT_URL, $url);
		curl_setopt($ch, CURLOPT_HTTPHEADER, array($this->getAPIHeader(), 'Accept: application/json'));
		$result = curl_exec($ch);
		curl_close($ch);
		return $this->preParseOutput($result);
	}

	public function set(array $params, array $options) {
		$url = $this->getURL() . implode('/', $params);
		$this->setParamsToUrl($url);
		$data = http_build_query(array('update' => $options));
		$ch = $this->getConnection();
		curl_setopt($ch, CURLOPT_URL, $url);
		curl_setopt($ch, CURLOPT_CUSTOMREQUEST, 'PUT');
		curl_setopt($ch, CURLOPT_HTTPHEADER, array($this->getAPIHeader(), 'Accept: application/json', 'X-HTTP-Method-Override: PUT', 'Content-Length: ' . strlen($data)));
		curl_setopt($ch, CURLOPT_POST, true);
		curl_setopt($ch, CURLOPT_POSTFIELDS, $data);
		$result = curl_exec($ch);
		curl_close($ch);
		return $this->preParseOutput($result);
	}

	public function create(array $params, array $options) {
		$url = $this->getURL() . implode('/', $params);
		$this->setParamsToUrl($url);
		$data = http_build_query(array('create' => $options));
		$ch = $this->getConnection();
		curl_setopt($ch, CURLOPT_URL, $url);
		curl_setopt($ch, CURLOPT_HTTPHEADER, array($this->getAPIHeader(), 'Accept: application/json'));
		curl_setopt($ch, CURLOPT_POST, true);
		curl_setopt($ch, CURLOPT_POSTFIELDS, $data);
		$result = curl_exec($ch);
		curl_close($ch);
		return $this->preParseOutput($result);
	}

	public function remove(array $params) {
		$url = $this->getURL() . implode('/', $params);
		$this->setParamsToUrl($url);
		$ch = $this->getConnection();
		curl_setopt($ch, CURLOPT_URL, $url);
		curl_setopt($ch, CURLOPT_CUSTOMREQUEST, 'DELETE');
		curl_setopt($ch, CURLOPT_HTTPHEADER, array($this->getAPIHeader(), 'Accept: application/json', 'X-HTTP-Method-Override: DELETE'));
		$result = curl_exec($ch);
		curl_close($ch);
		return $this->preParseOutput($result);
	}

	private function preParseOutput($result) {
		$this->Application->getModule('logging')->log(__FUNCTION__, $result, Logging::CATEGORY_APPLICATION, __FILE__, __LINE__);
		$resource = json_decode($result);
		$error = null;
		if(is_object($resource) && property_exists($resource, 'error')) {
			if(!in_array($resource->error, $this->allowedErrors)) {
				$error = $resource->error;
			}
		} else {
			$error = AuthorizationError::ERROR_AUTHORIZATION_TO_WEBGUI_PROBLEM;
		}

		$this->Application->getModule('logging')->log(__FUNCTION__, $resource, Logging::CATEGORY_APPLICATION, __FILE__, __LINE__);
		if(!is_null($error)) {
			// Note! Redirection to error page takes place here.
			$this->Response->redirect($this->Service->constructUrl('BaculumError',array('error' => $error), false));
		}

		return $resource;
	}
}
?>
