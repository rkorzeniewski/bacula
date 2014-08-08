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
 
class BaculumPage extends TPage
{

	public function onPreInit($param) {
		parent::onPreInit($param);
		$configuration = $this->getModule('configuration');
		$this->Application->getGlobalization()->Culture = $this->getLanguage();
		$this->setPrefixForSubdir();
	}

	public function getLanguage() {
		if(isset($this->Session['language']) && !empty($this->Session['language'])) {
			$language =  $this->Session['language'];
		} else {
			$language = $this->getModule('configuration')->getLanguage();
			$this->Session['language'] = $language;
		}
		return $language;
	}

	public function getModule($name) {
		return $this->Application->getModule($name);
	}

	public function goToPage($pagePath,$getParameters=null) {
		$this->Response->redirect($this->Service->constructUrl($pagePath,$getParameters,false));
	}

	public function goToDefaultPage() {
		$this->goToPage($this->Service->DefaultPage);
	}

	public function setPrefixForSubdir() {
		$fullDocumentRoot = preg_replace('#(\/)$#', '', $this->getFullDocumentRoot());
		$urlPrefix = str_replace($fullDocumentRoot, '', APPLICATION_DIRECTORY);
		if(!empty($urlPrefix)) {
			$this->Application->getModule('friendly-url')->setUrlPrefix($urlPrefix);
		}
	}

	private function getFullDocumentRoot() {
		$rootDir = array();
		$dirs = explode('/', $_SERVER['DOCUMENT_ROOT']);
		for($i = 0; $i < count($dirs); $i++) {
			$documentRootPart =  implode('/', $rootDir) . '/' . $dirs[$i];
			if(is_link($documentRootPart)) {
				$rootDir = array(readlink($documentRootPart));
			} else {
				$rootDir[] = $dirs[$i];
			}
		}

		$rootDir = implode('/', $rootDir);
		return $rootDir;
	}
}
?>
