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
 
Prado::using('System.Exceptions.TException');
Prado::using('Application.Class.Errors');

abstract class BaculumAPI extends TPage
{
	protected $output;
	protected $error;

	protected $director;

	/**
	 * Actions methods.
	 */
	const GET_METHOD = 'GET';
	const POST_METHOD = 'POST';
	const PUT_METHOD = 'PUT';
	const DELETE_METHOD = 'DELETE';

	public function onInit($params) {
		parent::onInit($params);
		$this->director = isset($this->Request['director']) ? $this->Request['director'] : null;
		switch($_SERVER['REQUEST_METHOD']) {
			case self::PUT_METHOD: {
				try {
					$this->put();
				} catch(TDbException $e) {
					$this->Application->getModule('logging')->log(__FUNCTION__, $e, Logging::CATEGORY_APPLICATION, __FILE__, __LINE__);
					$this->output = DatabaseError::MSG_ERROR_DB_CONNECTION_PROBLEM;
					$this->error = DatabaseError::ERROR_DB_CONNECTION_PROBLEM;
				}
				break;
			}
			case self::GET_METHOD: {
				try {
					$this->get();
				} catch(TDbException $e) {
					$this->Application->getModule('logging')->log(__FUNCTION__, $e, Logging::CATEGORY_APPLICATION, __FILE__, __LINE__);
					$this->output = DatabaseError::MSG_ERROR_DB_CONNECTION_PROBLEM;
					$this->error = DatabaseError::ERROR_DB_CONNECTION_PROBLEM;
				}
				break;
			}
			case self::POST_METHOD: {
				try {
					$this->post();
				} catch(TDbException $e) {
					$this->Application->getModule('logging')->log(__FUNCTION__, $e, Logging::CATEGORY_APPLICATION, __FILE__, __LINE__);
					$this->output = DatabaseError::MSG_ERROR_DB_CONNECTION_PROBLEM;
					$this->error = DatabaseError::ERROR_DB_CONNECTION_PROBLEM;
				}
				break;
			}
			case self::DELETE_METHOD: {
				try {
					$this->delete();
				} catch(TDbException $e) {
					$this->Application->getModule('logging')->log(__FUNCTION__, $e, Logging::CATEGORY_APPLICATION, __FILE__, __LINE__);
					$this->output = DatabaseError::MSG_ERROR_DB_CONNECTION_PROBLEM;
					$this->error = DatabaseError::ERROR_DB_CONNECTION_PROBLEM;
				}
				break;
			}
		}
	}

	private function getOutput() {
		$output = array('output' => $this->output, 'error' => $this->error);
		return json_encode($output);
	}

	public function onLoad($params) {
		parent::onLoad($params);
		echo $this->getOutput();
	}

	abstract protected function get();

	private function put() {
		$id = isset($this->Request['id']) ? $this->Request['id'] : null;
		if(is_array($this->Request['update']) && count($this->Request['update']) > 0) {
			$params = (object)$this->Request['update'];
			$this->set($id, $params);
		} else {
			parse_str(file_get_contents("php://input"),$responseData);
			if(is_array($responseData) && array_key_exists('update', $responseData)) {
				$params = (object)$responseData['update'];
				$this->set($id, $params);
			} else {
				$this->set($id, array()); //@TOVERIFY
				//$this->output = GenericError::MSG_ERROR_INVALID_COMMAND;
				//$this->error = GenericError::ERROR_INVALID_COMMAND;
			}
		}
	}
	
	private function post() {
		if(is_array($this->Request['create']) && count($this->Request['create']) > 0) {
			$params = (object)$this->Request['create'];
			$this->create($params);
		}
	}

	private function delete() {
		if(isset($this->Request['id'])) {
			$id = intval($this->Request['id']);
			$this->remove($id);
		}
	}

	public function getModule($name) {
		return $this->Application->getModule($name);
	}
}
?>