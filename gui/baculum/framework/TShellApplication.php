<?php
/**
 * TShellApplication class file
 *
 * @author Qiang Xue <qiang.xue@gmail.com>
 * @link http://www.pradosoft.com/
 * @copyright Copyright &copy; 2005-2013 PradoSoft
 * @license http://www.pradosoft.com/license/
 * @version $Id: TShellApplication.php 3245 2013-01-07 20:23:32Z ctrlaltca $
 * @package System
 */

/**
 * TShellApplication class.
 *
 * TShellApplication is the base class for developing command-line PRADO
 * tools that share the same configurations as their Web application counterparts.
 *
 * A typical usage of TShellApplication in a command-line PHP script is as follows:
 * <code>
 * require_once('path/to/prado.php');
 * $application=new TShellApplication('path/to/application.xml');
 * $application->run();
 * // perform command-line tasks here
 * </code>
 *
 * Since the application instance has access to all configurations, including
 * path aliases, modules and parameters, the command-line script has nearly the same
 * accessibility to resources as the PRADO Web applications.
 *
 * @author Qiang Xue <qiang.xue@gmail.com>
 * @version $Id: TShellApplication.php 3245 2013-01-07 20:23:32Z ctrlaltca $
 * @package System
 * @since 3.1.0
 */
class TShellApplication extends TApplication
{
	/**
	 * Runs the application.
	 * This method overrides the parent implementation by initializing
	 * application with configurations specified when it is created.
	 */
	public function run()
	{
		$this->initApplication();
	}
}

