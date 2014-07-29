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
 
Prado::using('System.Web.UI.ActiveControls.TActivePanel');
Prado::using('System.Web.UI.ActiveControls.TActiveButton');
Prado::using('System.Web.UI.ActiveControls.TActiveTextBox');
Prado::using('Application.Portlets.Portlets');

class Console extends Portlets {

	const MAX_CONSOLE_OUTPUT_BATCH = -1000;

	public function sendCommand($sender, $param) {
		$cmd = trim($this->CommandLine->Text);
		if(!empty($cmd)) {
			$command = explode(' ', $cmd);
			$out = $this->Application->getModule('api')->set(array('console'), $command)->output;
			if(is_array($out)) {
				$out = array_slice($out, self::MAX_CONSOLE_OUTPUT_BATCH);
				$output = $this->OutputListing->Text . PHP_EOL . implode(PHP_EOL, $out);
			} else {
				$output = $this->OutputListing->Text . PHP_EOL . $out;
			}
			$this->OutputListing->Text = $output;
			$this->CommandLine->Text = '';
		}
	}

	public function clearConsole($sender, $param) {
		$this->OutputListing->Text = '';
		$this->CommandLine->Text = '';
	}
}

?>