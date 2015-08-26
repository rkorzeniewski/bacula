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

class PoolRecord extends ActiveRecord {
	const TABLE = 'Pool';

	public $poolid;
	public $name;
	public $numvols;
	public $maxvols;
	public $useonce;
	public $usecatalog;
	public $acceptanyvolume;
	public $volretention;
	public $voluseduration;
	public $maxvoljobs;
	public $maxvolfiles;
	public $maxvolbytes;
	public $autoprune;
	public $recycle;
	public $actiononpurge;
	public $pooltype;
	public $labeltype;
	public $labelformat;
	public $enabled;
	public $scratchpoolid;
	public $recyclepoolid;
	public $nextpoolid;
	public $migrationhighbytes;
	public $migrationlowbytes;
	public $migrationtime;

	public static function finder($className = __CLASS__) {
		return parent::finder($className);
	}
}
?>