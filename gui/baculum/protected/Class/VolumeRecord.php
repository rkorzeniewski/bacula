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

class VolumeRecord extends ActiveRecord {
	const TABLE = 'Media';

	public $mediaid;
	public $volumename;
	public $slot;
	public $poolid;
	public $mediatype;
	public $mediatypeid;
	public $labeltype;
	public $firstwritten;
	public $lastwritten;
	public $labeldate;
	public $voljobs;
	public $volfiles;
	public $volblocks;
	public $volmounts;
	public $volbytes;
	public $volabytes;
	public $volapadding;
	public $volholebytes;
	public $volholes;
	public $volparts;
	public $volerrors;
	public $volwrites;
	public $maxvolbytes;
	public $volcapacitybytes;
	public $volstatus;
	public $enabled;
	public $recycle;
	public $actiononpurge;
	public $volretention;
	public $voluseduration;
	public $maxvoljobs;
	public $maxvolfiles;
	public $inchanger;
	public $storageid;
	public $deviceid;
	public $mediaaddressing;
	public $volreadtime;
	public $volwritetime;
	public $endfile;
	public $endblock;
	public $locationid;
	public $recyclecount;
	public $initialwrite;
	public $scratchpoolid;
	public $recyclepoolid;
	public $comment;

	public $pool;
	public $whenexpire;

	public static function finder($className = __CLASS__) {
		return parent::finder($className);
	}
}
?>