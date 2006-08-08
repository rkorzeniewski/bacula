 var even_cell_color = "#FFFFFF";
 var odd_cell_color  = "#EEEEEE";
 var header_color    = "#E1E0DA";
 var rows_per_page   = 20;
 var up_icon         = "/bweb/up.gif";
 var down_icon       = "/bweb/down.gif";
 var prev_icon       = "/bweb/left.gif";
 var next_icon       = "/bweb/right.gif";
 var rew_icon        = "/bweb/first.gif";
 var fwd_icon        = "/bweb/last.gif";

 var jobstatus = {
 'C': 'created but not yet running',
 'R': 'running',
 'B': 'blocked',
 'T': 'terminated normally',
 'E': 'Job terminated in error',
 'e': 'Non-fatal error',
 'f': 'Fatal error',
 'D': 'Verify differences',
 'A': 'canceled by user',
 'F': 'waiting on File daemon',
 'S': 'waiting on the Storage daemon',
 'm': 'waiting for new media',
 'M': 'waiting for Mount',
 's': 'Waiting for storage resource',
 'j': 'Waiting for job resource',
 'c': 'Waiting for Client resource',
 'd': 'Waiting for maximum jobs',
 't': 'Waiting for start time',
 'p': 'Waiting for higher priority jobs to finish'
};

var joblevel = {
 'F': 'Full backup',
 'I': 'Incr (since last backup)',
 'D': 'Diff (since last full backup)',
 'C': 'verify from catalog',
 'V': 'verify save (init DB)',
 'O': 'verify Volume to catalog entries',
 'd': 'verify Disk attributes to catalog',
 'A': 'verify data on volume',
 'B': 'Base level job'
};


var refresh_time = 60000;

function bweb_refresh() {
  location.reload(true)
}
function bweb_add_refresh(){
	window.setInterval("bweb_refresh()",refresh_time);
}

