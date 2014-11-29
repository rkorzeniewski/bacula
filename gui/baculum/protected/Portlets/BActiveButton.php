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
 
Prado::using('System.Web.UI.ActiveControls.TActiveControlAdapter');

class BActiveButton extends TButton implements ICallbackEventHandler, IActiveControl
{
	public function __construct()
	{
		parent::__construct();
		$this->setAdapter(new TActiveControlAdapter($this));
	}

	public function getActiveControl()
	{
		return $this->getAdapter()->getBaseActiveControl();
	}

	public function getClientSide()
	{
		return $this->getAdapter()->getBaseActiveControl()->getClientSide();
	}

	public function raiseCallbackEvent($param)
	{
		$this->raisePostBackEvent($param);
		$this->onCallback($param);
	}

	public function onCallback($param)
	{
		$this->raiseEvent('OnCallback', $this, $param);
	}

	public function setText($value)
	{
		parent::setText($value);
		if($this->getActiveControl()->canUpdateClientSide())
			$this->getPage()->getCallbackClient()->setAttribute($this, 'value', $value);
	}

	protected function renderClientControlScript($writer)
	{
		$this->CssClass = "bbutton";
	}

	protected function addAttributesToRender($writer)
	{
		parent::addAttributesToRender($writer);
		$writer->addAttribute('id',$this->getClientID());
		$this->getActiveControl()->registerCallbackClientScript(
			$this->getClientClassName(), $this->getPostBackOptions());
	}

	protected function getClientClassName()
	{
		return 'Prado.WebUI.TActiveButton';
	}

	public function setActionClass($param) {
	}


}

?>
