<?php

class BaculumError extends BaculumPage {
	public $error;

	public function onInit($param) {
		$this->error = intval($this->Request['error']);
	}
}
?>