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
 
class GenericError {
	const ERROR_NO_ERRORS = 0;
	const ERROR_INVALID_COMMAND = 1;
	const ERROR_INTERNAL_ERROR = 100;

	const MSG_ERROR_NO_ERRORS = '';
	const MSG_ERROR_INVALID_COMMAND = 'Invalid command.';
	const MSG_ERROR_INTERNAL_ERROR = 'Internal error.';
}

class DatabaseError extends GenericError {
	const ERROR_DB_CONNECTION_PROBLEM = 2;
	const ERROR_WRITE_TO_DB_PROBLEM = 3;
	
	const MSG_ERROR_DB_CONNECTION_PROBLEM = 'Problem with connection to database.';
	const MSG_ERROR_WRITE_TO_DB_PROBLEM = 'Error during write to database.';
}

class BconsoleError extends GenericError {

	const ERROR_BCONSOLE_CONNECTION_PROBLEM = 4;
	const ERROR_INVALID_DIRECTOR = 5;

	const MSG_ERROR_BCONSOLE_CONNECTION_PROBLEM = 'Problem with connection to bconsole.';
	const MSG_ERROR_INVALID_DIRECTOR = 'Invalid director.';
}

class AuthorizationError extends GenericError {

	const ERROR_AUTHORIZATION_TO_WEBGUI_PROBLEM = 6;

	const MSG_ERROR_AUTHORIZATION_TO_WEBGUI_PROBLEM = 'Problem with authorization to Baculum WebGUI.';
}

class ClientError extends GenericError {
	const ERROR_CLIENT_DOES_NOT_EXISTS = 10;

	const MSG_ERROR_CLIENT_DOES_NOT_EXISTS = 'Client with inputed clientid does not exist.';
}

class StorageError extends GenericError {
	const ERROR_STORAGE_DOES_NOT_EXISTS = 20;

	const MSG_ERROR_STORAGE_DOES_NOT_EXISTS = 'Storage with inputed storageid does not exist.';
}

class VolumeError extends GenericError {
	const ERROR_VOLUME_DOES_NOT_EXISTS = 30;

	const MSG_ERROR_VOLUME_DOES_NOT_EXISTS = 'Volume with inputed mediaid does not exist.';
}

class PoolError extends GenericError {
	const ERROR_POOL_DOES_NOT_EXISTS = 40;
	const ERROR_NO_VOLUMES_IN_POOL_TO_UPDATE = 41;

	const MSG_ERROR_POOL_DOES_NOT_EXISTS = 'Pool with inputed poolid does not exist.';
	const MSG_NO_VOLUMES_IN_POOL_TO_UPDATE= 'Pool with inputed poolid does not contain any volume to update.';
}

class JobError extends GenericError {
	const ERROR_JOB_DOES_NOT_EXISTS = 50;
	const ERROR_INVALID_JOBLEVEL = 51;
	const ERROR_FILESETID_DOES_NOT_EXISTS = 52;
	const ERROR_CLIENTID_DOES_NOT_EXISTS = 53;
	const ERROR_STORAGEID_DOES_NOT_EXISTS = 54;
	const ERROR_POOLID_DOES_NOT_EXISTS = 55;
	const ERROR_INVALID_RPATH = 56;
	const ERROR_INVALID_WHERE_OPTION = 57;
	const ERROR_INVALID_REPLACE_OPTION = 58;

	const MSG_ERROR_JOB_DOES_NOT_EXISTS = 'Job with inputed jobid does not exist.';
	const MSG_ERROR_INVALID_JOBLEVEL = 'Inputed job level is invalid.';
	const MSG_ERROR_FILESETID_DOES_NOT_EXISTS = 'FileSet resource with inputed filesetid does not exist.';
	const MSG_ERROR_CLIENTID_DOES_NOT_EXISTS = 'Client with inputed clientid does not exist.';
	const MSG_ERROR_STORAGEID_DOES_NOT_EXISTS = 'Storage with inputed storageid does not exist.';
	const MSG_ERROR_POOLID_DOES_NOT_EXISTS = 'Pool with inputed poolid does not exist.';
	const MSG_ERROR_INVALID_RPATH = 'Inputed rpath for restore is invalid. Proper format is b2[0-9]+.';
	const MSG_ERROR_INVALID_WHERE_OPTION = 'Inputed "where" option is invalid.';
	const MSG_ERROR_INVALID_REPLACE_OPTION = 'Inputed "replace" option is invalid.';
}

class FileSetError extends GenericError {
	const ERROR_FILESET_DOES_NOT_EXISTS = 60;

	const MSG_ERROR_FILESET_DOES_NOT_EXISTS = 'FileSet with inputed filesetid does not exist.';
}

class BVFSError extends GenericError {
	const ERROR_JOB_DOES_NOT_EXISTS = 70;
	const ERROR_INVALID_PATH = 71;

	const MSG_ERROR_JOB_DOES_NOT_EXISTS = 'Job with inputed jobid does not exist.';
	const MSG_ERROR_INVALID_PATH = 'Inputed path for restore is invalid. Proper format is b2[0-9]+.';
}
?>