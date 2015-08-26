<?php

// small sorting callback function to sort by name
function sortListByName($a, $b) {
	return strcasecmp($a['name'], $b['name']);
}

class Miscellaneous extends TModule {

	const LICENCE_FILE = 'LICENSE';

	private $jobTypes = array(
		'B' => 'Backup',
		'M' => 'Migrated',
		'V' => 'Verify',
		'R' => 'Restore',
		'I' => 'Internal',
		'D' => 'Admin',
		'A' => 'Archive',
		'C' => 'Copy',
		'g' => 'Migration'
	);

	private $jobLevels = array(
		'F' => 'Full',
		'I' => 'Incremental',
		'D' => 'Differential',
		'B' => 'Base',
		'f' => 'VirtualFull',
		'V' => 'InitCatalog',
		'C' => 'Catalog',
		'O' => 'VolumeToCatalog',
		'd' => 'DiskToCatalog'
	);

	private $jobStates =  array(
		'C' => array('value' => 'Created', 'description' =>'Created but not yet running'),
		'R' => array('value' => 'Running', 'description' => 'Running'),
		'B' => array('value' => 'Blocked', 'description' => 'Blocked'),
		'T' => array('value' => 'Terminated', 'description' =>'Terminated normally'),
		'W' => array('value' => 'Terminated with warnings', 'description' =>'Terminated normally with warnings'),
		'E' => array('value' => 'Error', 'description' =>'Terminated in Error'),
		'e' => array('value' => 'Non-fatal error', 'description' =>'Non-fatal error'),
		'f' => array('value' => 'Fatal error', 'description' =>'Fatal error'),
		'D' => array('value' => 'Verify Diff.', 'description' =>'Verify Differences'),
		'A' => array('value' => 'Canceled by user', 'description' =>'Canceled by the user'),
		'I' => array('value' => 'Incomplete', 'description' =>'Incomplete Job'),
		'F' => array('value' => 'Waiting on FD', 'description' =>'Waiting on the File daemon'),
		'S' => array('value' => 'Waiting on SD', 'description' =>'Waiting on the Storage daemon'),
		'm' => array('value' => 'Waiting for new vol.', 'description' =>'Waiting for a new Volume to be mounted'),
		'M' => array('value' => 'Waiting for mount', 'description' =>'Waiting for a Mount'),
		's' => array('value' => 'Waiting for storage', 'description' =>'Waiting for Storage resource'),
		'j' => array('value' => 'Waiting for job', 'description' =>'Waiting for Job resource'),
		'c' => array('value' => 'Waiting for client', 'description' =>'Waiting for Client resource'),
		'd' => array('value' => 'Waiting for Max. jobs', 'description' =>'Wating for Maximum jobs'),
		't' => array('value' => 'Waiting for start', 'description' =>'Waiting for Start Time'),
		'p' => array('value' => 'Waiting for higher priority', 'description' =>'Waiting for higher priority job to finish'),
		'i' => array('value' => 'Batch insert', 'description' =>'Doing batch insert file records'),
		'a' => array('value' => 'Despooling attributes', 'description' =>'SD despooling attributes'),
		'l' => array('value' => 'Data despooling', 'description' =>'Doing data despooling'),
		'L' => array('value' => 'Commiting data', 'description' =>'Committing data (last despool)')
	);

	private $runningJobStates = array('C', 'R');

	/**
	 * Getting the licence from file.
	 * 
	 * @access public
	 * @return string licence text
	 */
	public function getLicence() {
		return nl2br(htmlspecialchars(file_get_contents(self::LICENCE_FILE)));
	}

	public function getJobLevels() {
		return $this->jobLevels;
	}

	public function getJobState($jobStateLetter = null) {
		$state;
		if(is_null($jobStateLetter)) {
			$state = $this->jobStates;
		} else {
			$state = array_key_exists($jobStateLetter, $this->jobStates) ? $this->jobStates[$jobStateLetter] : null;
		}
		return $state;
	}

	public function getRunningJobStates() {
		return $this->runningJobStates;
	}


	public function getJobType($jobTypeLetter = null) {
		$type;
		if(is_null($jobTypeLetter)) {
			$type = $this->jobTypes;
		} else {
			$type = array_key_exists($jobTypeLetter, $this->jobTypes) ? $this->jobTypes[$jobTypeLetter] : null;
		}
		return $type;

	}

	public function isValidJobLevel($jobLevel) {
		return array_key_exists($jobLevel, $this->getJobLevels());
	}

	/**
	 * Writing INI-style configuration file.
	 * 
	 * Functions has been got from StackOverflow.com service (http://stackoverflow.com/questions/4082626/save-ini-file-with-comments).
	 * 
	 * @access public
	 * @param string $file file localization
	 * @param array $options structure of config file params
	 * @return mixed if success then returns the number of bytes that were written to the file as the integer type, if failure then returns false
	 */
	public function writeINIFile($file, array $options){
		$tmp = '';
		foreach($options as $section => $values){
			$tmp .= "[$section]\n";
				foreach($values as $key => $val){
					if(is_array($val)){
						foreach($val as $k =>$v){
							$tmp .= "{$key}[$k] = \"$v\"\n";
						}
					} else {
						$tmp .= "$key = \"$val\"\n";
					}
				}
			$tmp .= "\n";
		}
		return file_put_contents($file, $tmp);
	}

	/**
	 * Parse INI-style configuration file.
	 * 
	 * @access public
	 * @param string $file file localization
	 * @return array data of configuration file
	 */
	public static function parseINIFile($file) {
		return file_exists($file) ? parse_ini_file($file, true) : array();
	}


	/**
	 * This method is copied from http://stackoverflow.com/questions/4345554/convert-php-object-to-associative-array
	 */
	public function objectToArray($data) {
		if (is_array($data) || is_object($data)) {
			$result = array();
			foreach ($data as $key => $value) {
				$result[$key] = $this->objectToArray($value);
			}
			return $result;
		}
		return $data;
	}

	public function decode_bacula_lstat($lstat) {
		$base64 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
		$lstat = trim($lstat);
		$lstat_fields = explode(' ', $lstat);

		if(count($lstat_fields) !== 16) {
			die('Błąd! Niepoprawna ilość pól wartości LStat. Proszę upewnić się, że podany ciąg znaków jest poprawną wartością LStat');
		}

		list($dev, $inode, $mode, $nlink, $uid, $gid, $rdev, $size, $blocksize, $blocks, $atime, $mtime, $ctime, $linkfi, $flags, $data) = $lstat_fields;
		$encoded_values = array('dev' => $dev, 'inode' => $inode, 'mode' => $mode, 'nlink' => $nlink, 'uid' => $uid, 'gid' => $gid, 'rdev' => $rdev, 'size' => $size, 'blocksize' => $blocksize, 'blocks' => $blocks, 'atime' => $atime, 'mtime' => $mtime, 'ctime' => $ctime, 'linkfi' => $linkfi, 'flags' => $flags, 'data' => $data);

		$ret = array();
		foreach($encoded_values as $key => $val) {
			$result = 0;
			$is_minus = false;
			$start = 0;

			if(substr($val, 0, 1) === '-') {
				$is_minus = true;
				$start++;
			}

			for($i = $start; $i < strlen($val); $i++) {
				$result = bcmul($result, bcpow(2,6));
				$result +=  strpos($base64, substr($val, $i , 1));
			}
			$ret[$key] = ($is_minus === true) ? -$result : $result;
		}
		return $ret;
	}

	public function parseBvfsList($list) {
		$elements = array();
		for($i = 0; $i < count($list); $i++) {
			if(preg_match('/^(?P<pathid>\d+)\t(?P<filenameid>\d+)\t(?P<fileid>\d+)\t(?P<jobid>\d+)\t(?P<lstat>[a-zA-z0-9\+\/\ ]+)\t(?P<name>.*)\/$/', $list[$i], $match) == 1 || preg_match('/^(?P<pathid>\d+)\t(?P<filenameid>\d+)\t(?P<fileid>\d+)\t(?P<jobid>\d+)\t(?P<lstat>[a-zA-z0-9\+\/\ ]+)\t(?P<name>\.{2})$/', $list[$i], $match) == 1) {
				if($match['name'] == '.') {
					continue;
				} elseif($match['name'] != '..') {
					$match['name'] .= '/';
				}
				$elements[] = array('pathid' => $match['pathid'], 'filenameid' => $match['filenameid'], 'fileid' => $match['fileid'], 'jobid' => $match['jobid'], 'lstat' => $match['lstat'], 'name' => $match['name'], 'type' => 'dir');
			} elseif(preg_match('/^(?P<pathid>\d+)\t(?P<filenameid>\d+)\t(?P<fileid>\d+)\t(?P<jobid>\d+)\t(?P<lstat>[a-zA-z0-9\+\/\ ]+)\t(?P<name>[^\/]+)$/', $list[$i], $match) == 1) {
				if($match['name'] == '.') {
					continue;
				}
				$elements[] = array('pathid' => $match['pathid'], 'filenameid' => $match['filenameid'], 'fileid' => $match['fileid'], 'jobid' => $match['jobid'], 'lstat' => $match['lstat'], 'name' => $match['name'], 'type' => 'file'); 
			}
		}
		usort($elements, 'sortListByName');
		return $elements;
	}

	public function parseFileVersions($filename, $list) {
		$elements = array();
		for($i = 0; $i < count($list); $i++) {
			if(preg_match('/^(?P<pathid>\d+)\t(?P<filenameid>\d+)\t(?P<fileid>\d+)\t(?P<jobid>\d+)\t(?P<lstat>[a-zA-z0-9\+\/\ ]+)\t(?P<md5>.+)\t(?P<volname>.+)\t(?P<inchanger>\d+)$/', $list[$i], $match) == 1) {
				$elements[$match['fileid']] = array('name' => $filename, 'pathid' => $match['pathid'], 'filenameid' => $match['filenameid'], 'fileid' => $match['fileid'], 'jobid' => $match['jobid'], 'lstat' => $this->decode_bacula_lstat($match['lstat']), 'md5' => $match['md5'], 'volname' => $match['volname'], 'inchanger' => $match['inchanger'], 'type' => 'file');
			}
		}
		return $elements;
	}
}
?>
