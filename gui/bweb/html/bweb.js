// Bweb - A Bacula web interface
// Bacula® - The Network Backup Solution
//
// Copyright (C) 2000-2006 Free Software Foundation Europe e.V.
//
// The main author of Bweb is Eric Bollengier.
// The main author of Bacula is Kern Sibbald, with contributions from
// many others, a complete list can be found in the file AUTHORS.
//
// This program is Free Software; you can redistribute it and/or
// modify it under the terms of version two of the GNU General Public
// License as published by the Free Software Foundation plus additions
// that are listed in the file LICENSE.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
// 02110-1301, USA.
//
// Bacula® is a registered trademark of John Walker.
// The licensor of Bacula is the Free Software Foundation Europe
// (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zurich,
// Switzerland, email:ftf@fsfeurope.org.

 var even_cell_color = "#FFFFFF";
 var odd_cell_color  = "#EEEEEE";
 var header_color    = "#E1E0DA";
 var rows_per_page   = 20;
 var bweb_root       = "/bweb/";
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

var joblevelname = {
 'F': 'Full',
 'I': 'Incremental',
 'D': 'Differential',
};


var refresh_time = 60000;

function bweb_refresh() {
  location.reload(true)
}
function bweb_add_refresh(){
	window.setInterval("bweb_refresh()",refresh_time);
}

function human_size(val)
{   
   if (!val) {
      return '';
   }
   var unit = ['B', 'KB', 'MB', 'GB', 'TB'];
   var i=0;
   var format;
   while (val /1024 > 1) {
      i++;
      val /= 1024;
   }

   var format = (i>0)?1:0;
   return val.toFixed(format) + ' ' + unit[i];
}

function human_sec(val)
{
   if (!val) {
      val = 0;
   }
   val /= 60;			// sec -> min
   
   if ((val / 60) <= 1) {
      return val.toFixed(0) + ' mins';
   }

   val /= 60;			// min -> hour

   if ((val / 24) <= 1) { 
      return val.toFixed(0) + ' hours';
   }

   val /= 24;                   // hour -> day

   if ((val / 365) < 2) { 
      return val.toFixed(0) + ' days';
   }

   val /= 365;

   return val.toFixed(0) + ' years';
}


//
// percent_display("row2", [ { nb: 1, name: "Full"   },
//			   { nb: 2, name: "Error"  },
//			   { nb: 5, name: "Append" },
//			   { nb: 2, name: "Purged" },
//                         {}                               # last element must be {}
//		         ]);

function percent_get_img(type)
{
   var img=document.createElement('img');
   if (type) {
      img.className="pSlice" + type ;
   } else {
      img.className="pSlice";
   }
   img.src="/bweb/pix.png";
   img.alt="";
   return img;
}

var percent_display_nb_slice = 20;
var percent_usage_nb_slice = 5;

function percent_display(hash_values, parent)
{
   var nb_elt=percent_display_nb_slice;
   var tips= "";

   if (!parent) {
      parent = document.createElement('DIV');
   }

   if (typeof parent != "object") {
      parent = document.getElementById(parent);
   } 

   if (!parent) {
       alert("E : display_percent(): Can't find parent " + parent);
       return;
   }

   hash_values.pop(); // drop last element {}

   var nb_displayed = 0;
   var nb_max = 0;

   for(var i=0;i<hash_values.length;i++) {
        nb_max += hash_values[i]['nb'];
   }

   for(var i=0;i<hash_values.length;i++) {
        var elt = hash_values[i];
        var cur_nb = (elt['nb'] * nb_elt)/nb_max;
        var cur_name = elt['name'];
        cur_name.replace(/-/,"_");

        tips = tips + " " + elt['nb'] + " " + cur_name;

        while ((nb_displayed < nb_elt) && (cur_nb >=1)) {
            nb_displayed++;
            cur_nb--;

            var img= percent_get_img(cur_name);
            parent.appendChild(img);
        }       
   }

   while (nb_displayed < nb_elt) {
      nb_displayed++;
      var img= percent_get_img();
      parent.appendChild(img);
  }     

  parent.title = tips;

  return parent;
}

function percent_usage(value, parent)
{
   var nb_elt=percent_usage_nb_slice;
   var type;
  
   if (!parent) {
      parent = document.createElement('DIV');
   }   

   if (typeof parent != "object") {
      parent = document.getElementById(parent);
   } 

   if (!parent) {
       alert("E : display_percent(): Can't find parent " + parent);
       return;
   }

   if (value >= 500) {
      return;
   } else if (value <= 0.001) {
      type = "Empty";
      value = 0;      
   } else if (value <= 40) {
      type = "Ok";
   } else if (value <= 75) {
      type = "Warn";
   } else if (value <= 85) {
      type = "Crit";
   } else {
      type = "Crit";
   }

   var nb = parseInt(value*nb_elt/100, 10);
   parent.title = parseInt(value*100,10)/100 + "% used (approximate)";

   for(var i=0; i<nb; i++) {
      var img= percent_get_img(type);
      parent.appendChild(img);
   }

   for(nb;nb < nb_elt;nb++) {
      var img= percent_get_img("Empty");
      parent.appendChild(img);       
   } 

   return parent;
}

function bweb_get_job_img(status, errors)
{
  var ret;

  if (status == "T") {
     if (errors > 0) {
        ret = "W.png";

     } else {
        ret = "T.png";
     }

  } else {
     ret = status + ".png";
  }

  return bweb_root + ret;
}

function search_media()
{
 var what = document.getElementById('searchbox').value;
 if (what) {
   document.search.action.value='media';
   document.search.re_media.value=what;
   document.search.submit();
 }
}

function search_client()
{
 var what = document.getElementById('searchbox').value;
 if (what) {
   document.search.action.value='client';
   document.search.re_client.value=what;
   document.search.submit();
 }
}

sfHover = function() {
 var sfEls = document.getElementById("menu").getElementsByTagName("LI");
 for (var i=0; i<sfEls.length; i++) {
    sfEls[i].onmouseover=function() {
       this.className+=" sfhover";
    }
    sfEls[i].onmouseout=function() {
       this.className=this.className.replace(new RegExp(" sfhover\\b"), "");
    }
 }
}

if (window.attachEvent) window.attachEvent("onload", sfHover);
