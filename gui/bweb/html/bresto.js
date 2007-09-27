
//   Bweb - A Bacula web interface
//   Bacula® - The Network Backup Solution
//
//   Copyright (C) 2000-2006 Free Software Foundation Europe e.V.
//
//   The main author of Bweb is Eric Bollengier.
//   The main author of Bacula is Kern Sibbald, with contributions from
//   many others, a complete list can be found in the file AUTHORS.
//
//   This program is Free Software; you can redistribute it and/or
//   modify it under the terms of version two of the GNU General Public
//   License as published by the Free Software Foundation plus additions
//   that are listed in the file LICENSE.
//
//   This program is distributed in the hope that it will be useful, but
//   WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
//   General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program; if not, write to the Free Software
//   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
//   02110-1301, USA.
//
//   Bacula® is a registered trademark of John Walker.
//   The licensor of Bacula is the Free Software Foundation Europe
//   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zurich,
//   Switzerland, email:ftf@fsfeurope.org.

// render if vol is online/offline
function rd_vol_is_online(val)
{
   return '<img src="/bweb/inflag' + val + '.png">';
}

// TODO: fichier ou rep
function rd_file_or_dir(val)
{
   if (val == 'F') {
      return '<img src="/bweb/A.png">';
   } else {
      return '<img src="/bweb/R.png">';
   }
}

Ext.namespace('Ext.brestore');

Ext.brestore.jobid=0;
Ext.brestore.client='';

function ext_init()
{
//////////////////////////////////////////////////////////////:
    var Tree = Ext.tree;
    var tree_loader = new Ext.tree.TreeLoader({
            baseParams:{}, //action:'list_dirs', pathid:103, jobid:9 },
            dataUrl:'/cgi-bin/bweb/bresto.pl'
        });

    var tree = new Ext.tree.TreePanel('div-tree', {
        animate:true, 
        loader: tree_loader,
        enableDD:true,
        enableDragDrop: true,
        containerScroll: true
    });

    // set the root node
    var root = new Ext.tree.AsyncTreeNode({
        text: 'Root',
        draggable:false,
        id:'source'
    });
    tree.setRootNode(root);

    // render the tree
    tree.render();
//    root.expand();

    tree.on('click', function(e) { 
        file_store.removeAll();
        file_store.load({params:{action: 'list_files',
                                 jobid:Ext.brestore.jobid, 
                                 node:e.id}
                       });
        return true;
    });

    tree.on('beforeload', function(e) {
        file_store.removeAll();
        return true;
    });


////////////////////////////////////////////////////////////////

  var file_store = new Ext.data.Store({
        proxy: new Ext.data.HttpProxy({
            url: '/cgi-bin/bweb/bresto.pl',
            method: 'GET',
            params:{action: 'list_files', offset:0, limit:50 }
        }),

        reader: new Ext.data.ArrayReader({
        }, Ext.data.Record.create([
   {name: 'fileid'    },
   {name: 'filenameid'},
   {name: 'pathid'    },
   {name: 'name'      },
   {name: 'size',     type: 'int'  },
   {name: 'mtime',    type: 'date', dateFormat: 'Y-m-d h:i:s'}
        ]))
   });

   var cm = new Ext.grid.ColumnModel([{
           id:        'name', // id assigned so we can apply custom css (e.g. .x-grid-col-topic b { color:#333 })
           header:    'File',
           dataIndex: 'name',
           width:     250,
           css:       'white-space:normal;'
        },{
           header:    "Size",
           dataIndex: 'size',
           renderer:  human_size,
           width:     50
        },{
           header:    "Date",
           dataIndex: 'mtime',
           width:     50
        },{
           dataIndex: 'pathid',
           hidden: true
        },{
           dataIndex: 'filenameid',
           hidden: true
        },{
           dataIndex: 'fileid',
           hidden: true
        }
        ]);

    // by default columns are sortable
   cm.defaultSortable = true;

    // create the grid
   var files_grid = new Ext.grid.Grid('div-files', {
        ds: file_store,
        cm: cm,
        ddGroup : 'TreeDD',
        enableDrag: true,
        enableDragDrop: true,
        selModel: new Ext.grid.RowSelectionModel(),
        loadMask: true,
        enableColLock:false
        
    });

    // when we reload the view,
    // we clear the file version box
    file_store.on('beforeload', function(e) {
        file_versions_store.removeAll();
        return true;
    });

    files_grid.selModel.on('rowselect', function(e,i,r) { 
        Ext.brestore.filename = r.json[3];
        file_versions_store.load({params:{action: 'list_versions',
                                          client: Ext.brestore.client,
                                          pathid: r.json[2],
                                          filenameid: r.json[1]
                                         }
                                 });
        return true;
    });
    files_grid.render();

//////////////////////////////////////////////////////////////:

  var file_selection_store = new Ext.data.Store({
        proxy: new Ext.data.MemoryProxy(),

        reader: new Ext.data.ArrayReader({
        }, Ext.data.Record.create([
   {name: 'jobid'     },
   {name: 'fileid'    },
   {name: 'filenameid'},
   {name: 'pathid'    },
   {name: 'size',     type: 'int'  },
   {name: 'mtime',    type: 'date', dateFormat: 'Y-m-d h:i:s'}
        ]))
   });

   var file_selection_cm = new Ext.grid.ColumnModel([{
           id:        'name', // id assigned so we can apply custom css (e.g. .x-grid-col-topic b { color:#333 })
           dataIndex: 'name',
           hidden: true
        },{
           header:    "JobId",
           dataIndex: 'jobid'
        },{
           header:    "Size",
           dataIndex: 'size',
           renderer:  human_size,
           width:     50
        },{
           header:    "Date",
           dataIndex: 'mtime',
           width:     50
        },{
           dataIndex: 'pathid',
           hidden: true
        },{
           dataIndex: 'filenameid',
           hidden: true
        },{
           dataIndex: 'fileid',
           hidden: true
        }
        ]);


    // create the grid
   var file_selection_grid = new Ext.grid.Grid('div-file-selection', {
        cm: file_selection_cm,
        ds: file_selection_store,
        ddGroup : 'TreeDD',
        enableDrag: false,
        enableDrop: true,
        selModel: new Ext.grid.RowSelectionModel(),
        loadMask: true,
        enableColLock:false
        
    });

    var file_selection_record = Ext.data.Record.create(
      {name: 'jobid'},
      {name: 'fileid'},
      {name: 'filenameid'},
      {name: 'pathid'},
      {name: 'size'},
      {name: 'mtime'}
    );
// data.selections[0].json[]
// data.node.id
// http://extjs.com/forum/showthread.php?t=12582&highlight=drag+drop
    var ddrow = new Ext.dd.DropTarget(file_selection_grid.container, {
        ddGroup : 'TreeDD',
        copy:false,
        notifyDrop : function(dd, e, data){
//		if (data.selections) {
//			alert("grid");
//		}
//
//		if (data.node) {
//			alert("tree");
//		}
//
		file_selection_store.add(
                 new file_selection_record({
			jobid:1,
			fileid:1,
			filenameid:1,
			pathid:1,
			size:1,
			mtime:'2007-01-01 01:00:00'
		 })
		);

		return true;
//              var sm=grid.getSelectionModel();
//              var rows=sm.getSelections();
//              var cindex=dd.getDragData(e).rowIndex;
//              for(i = 0; i < rows.length; i++) {
//                      rowData=ds.getById(rows[i].id);
//                      if(!this.copy) 
//                              ds.remove(ds.getById(rows[i].id));
//                      ds.insert(cindex,rowData);
//              };
        }
    });


   file_selection_grid.on('enddrag', function(dd,e) { 
        alert(e) ; return true;
    });
   file_selection_grid.on('notifyDrop', function(dd,e) { 
        alert(e) ; return true;
    });
   file_selection_grid.render();

///////////////////////////////////////////////////////

  var file_versions_store = new Ext.data.Store({
        proxy: new Ext.data.HttpProxy({
            url: '/cgi-bin/bweb/bresto.pl',
            method: 'GET',
            params:{offset:0, limit:50 }
        }),

        reader: new Ext.data.ArrayReader({
        }, Ext.data.Record.create([
   {name: 'fileid'    },
   {name: 'filenameid'},
   {name: 'pathid'    },
   {name: 'jobid'     },
   {name: 'volume'    },
   {name: 'inchanger' },
   {name: 'md5'       },
   {name: 'size',     type: 'int'  },
   {name: 'mtime'} //,    type: 'date', dateFormat: 'Y-m-d h:i:s'}
        ]))
   });

   var file_versions_cm = new Ext.grid.ColumnModel([{
           id:        'name', // id assigned so we can apply custom css (e.g. .x-grid-col-topic b { color:#333 })
           dataIndex: 'name',
           hidden: true
        },{
           header:    "InChanger",
           dataIndex: 'inchanger',
           renderer:  rd_vol_is_online
        },{
           header:    "Volume",
           dataIndex: 'volume'
        },{
           header:    "JobId",
           dataIndex: 'jobid'
        },{
           header:    "Size",
           dataIndex: 'size',
           renderer:  human_size,
           width:     50
        },{
           header:    "Date",
           dataIndex: 'mtime',
           width:     50
        },{
           header:    "MD5",
           dataIndex: 'md5',
           width:     50
        },{
           dataIndex: 'pathid',
           hidden: true
        },{
           dataIndex: 'filenameid',
           hidden: true
        },{
           dataIndex: 'fileid',
           hidden: true
        }
   ]);

    // by default columns are sortable
   file_versions_cm.defaultSortable = true;

    // create the grid
   var file_versions_grid = new Ext.grid.Grid('div-file-versions', {
        ds: file_versions_store,
        cm: file_versions_cm,
        ddGroup : 'TreeDD',
        enableDrag: true,
        enableDrop: false,
        selModel: new Ext.grid.RowSelectionModel(),
        loadMask: true,
        enableColLock:false
        
    });

    file_versions_grid.on('rowdblclick', function(e) { 
        alert(e) ; file_versions_store.removeAll(); return true;
    });
    file_versions_grid.render();

//////////////////////////////////////////////////////////////:


    var client_store = new Ext.data.Store({
        proxy: new Ext.data.HttpProxy({
            url: '/cgi-bin/bweb/bresto.pl',
            method: 'GET',
            params:{action:'list_client'}
        }),

        reader: new Ext.data.ArrayReader({
        }, Ext.data.Record.create([
           {name: 'name' }
        ]))
    });

    var client_combo = new Ext.form.ComboBox({
        fieldLabel: 'Clients',
        store: client_store,
        displayField:'name',
        typeAhead: true,
        mode: 'local',
        triggerAction: 'all',
        emptyText:'Select a client...',
        selectOnFocus:true,
        forceSelection: true,
        width:135
    });

    client_combo.on('valid', function(e) { 
        Ext.brestore.client = e.getValue();
        job_store.load( {params:{action: 'list_job',
                                 client:Ext.brestore.client}});
        return true;
    });

//////////////////////////////////////////////////////////////:

    var job_store = new Ext.data.Store({
        proxy: new Ext.data.HttpProxy({
            url: '/cgi-bin/bweb/bresto.pl',
            method: 'GET',
            params:{offset:0, limit:50 }
        }),

        reader: new Ext.data.ArrayReader({
        }, Ext.data.Record.create([
           {name: 'jobid' },
           {name: 'date'  },
           {name: 'jobname' }
        ]))
    });

    var job_combo = new Ext.form.ComboBox({
        fieldLabel: 'Jobs',
        store: job_store,
        displayField:'jobname',
        typeAhead: true,
        mode: 'local',
        triggerAction: 'all',
        emptyText:'Select a job...',
        selectOnFocus:true,
        forceSelection: true,
        width:300
    });

    job_combo.on('select', function(e,c) {
        // TODO: choose between date and jobid here (with a toolbar bp ?)
        Ext.brestore.jobid = c.json[0];
        root.setText("Root");
        tree_loader.baseParams = { action:'list_dirs',
                                   jobid:Ext.brestore.jobid };
        root.reload();
    });

////////////////////////////////////////////////////////////////:

    // create the primary toolbar
    var tb2 = new Ext.Toolbar('div-tb-sel');
    tb2.add({
        id:'save',
        text:'Save',
        disabled:true,
//        handler:save,
        cls:'x-btn-text-icon save',
        tooltip:'Saves all components to the server'
    },'-', {
        id:'add',
        text:'Component',
//        handler:addComponent,
        cls:'x-btn-text-icon add-cmp',
        tooltip:'Add a new Component to the dependency builder'
    }, {
        id:'option',
        text:'Option',
        disabled:true,
//        handler:addOption,
        cls:'x-btn-text-icon add-opt',
        tooltip:'Add a new optional dependency to the selected component'
    },'-',{
        id:'remove',
        text:'Remove',
        disabled:true,
//        handler:removeNode,
        cls:'x-btn-text-icon remove',
        tooltip:'Remove the selected item'
    });

    var where_field = new Ext.form.TextField({
            fieldLabel: 'Location',
            name: 'where',
            width:175,
            allowBlank:false
    });

    var tb = new Ext.Toolbar('div-toolbar', [
        client_combo,
        job_combo,
        '-',
        {
          id: 'tb_home',
//        icon: '/bweb/up.gif',
          text: 'Change location',
          cls:'x-btn-text-icon',
          handler: function() { 
                var where = where_field.getValue();
                root.setText(where);
                tree_loader.baseParams = { action:'list_dirs',
                                           jobid:Ext.brestore.jobid,
                                           path: where };
                root.reload();
          }
        },
        where_field
    ]);

////////////////////////////////////////////////////////////////

    var layout = new Ext.BorderLayout(document.body, {
        north: {
//            split: true
        },
        south: {
            split: true, initialSize: 300
        },
        east: {
            split: true, initialSize: 500
        },
        west: {
            split: true, initialSize: 300
        },
        center: {
            initialSize: 600
        }        
        
    });

layout.beginUpdate();
  layout.add('north', new Ext.ContentPanel('div-toolbar', {
      fitToFrame: true, autoCreate:true,closable: false 
  }));
  layout.add('south', new Ext.ContentPanel('div-file-selection', {
      toolbar: tb2,resizeEl:'div-container',
      fitToFrame: true, autoCreate:true,closable: false
  }));
  layout.add('east', new Ext.ContentPanel('div-file-versions', {
      fitToFrame: true, autoCreate:true,closable: false
  }));
  layout.add('west', new Ext.ContentPanel('div-tree', {
      autoScroll:true, fitToFrame: true, 
      autoCreate:true,closable: false
  }));
  layout.add('center', new Ext.ContentPanel('div-files', {
      autoScroll:true,autoCreate:true,fitToFrame: true
  }));
layout.endUpdate();     


////////////////////////////////////////////////////////////////

//    job_store.load();
    client_store.load({params:{action: 'list_client'}});
//    file_store.load({params:{offset:0, limit:50}});
//    file_versions_store.load({params:{offset:0, limit:50}});
//    file_selection_store.load();

}
Ext.onReady( ext_init );
