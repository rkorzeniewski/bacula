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
 
class Portlets extends TTemplateControl {

	const SORT_ASC = 'asc';
	const SORT_DESC = 'desc';

	protected function getUrlParams($section, $id) {
		$limit = $_SESSION['limit' . $id];
		if(is_numeric($limit)) {
			if(is_array($section)) {
				array_push($section, 'limit', $limit);
				$params = $section;
			} else {
				$params = array($section, 'limit', $limit);
			}
		} else {
			$params = (is_array($section)) ? $section : array($section);
		}
		return $params;
	}

	protected function getSortOrder($id) {
		$order = self::SORT_ASC;
		if(@$this->Request->Cookies['sort']->Value == $id) {
			$this->Response->Cookies[] = new THttpCookie('sort', null);
			$order = self::SORT_DESC;
		} else {
			$this->Response->Cookies[] = new THttpCookie('sort', $id);
			$order = self::SORT_ASC;
		}
		return $order;
	}
	
	protected function sortData($data, $key, $id) {
		if($this->getSortOrder($id) == self::SORT_DESC) {
			$compare = create_function('$a,$b','if ($a["'.$key.'"] == $b["'.$key.'"]) {return 0;}else {return ($a["'.$key.'"] < $b["'.$key.'"]) ? 1 : -1;}');
		} else {
			$compare = create_function('$a,$b','if ($a["'.$key.'"] == $b["'.$key.'"]) {return 0;}else {return ($a["'.$key.'"] > $b["'.$key.'"]) ? 1 : -1;}');
		}
		usort($data,$compare);
		return $data;
	}
}
?>