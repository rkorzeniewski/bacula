<?php
 
class Miscellaneous extends TModule {

	const LICENCE_FILE = 'LICENSE';

	private $jobLevels = array('F' => 'Full', 'I' => 'Incremental', 'D'=> 'Differential', 'B' => 'Base', 'f' => 'VirtualFull');

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