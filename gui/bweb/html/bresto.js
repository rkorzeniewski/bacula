//   Bweb - A Bacula web interface
//   Bacula® - The Network Backup Solution
//
//   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
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
//   Bacula® is a registered trademark of Kern Sibbald.
//   The licensor of Bacula is the Free Software Foundation Europe
//   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zurich,
//   Switzerland, email:ftf@fsfeurope.org.

// render if vol is online/offline
function rd_vol_is_online(val)
{
   return '<img src="inflag' + val + '.png">';
}

// TODO: fichier ou rep
function rd_file_or_dir(val)
{
   if (val > 0) {
      return '<img src="file_f.png">';
   } else {
      return '<img src="file_d.png">';
   }
}

function push_path()
{
    Ext.brestore.path_stack.push(Ext.brestore.path);
    Ext.brestore.pathid_stack.push(Ext.brestore.pathid);
    Ext.ComponentMgr.get('prevbp').enable();
}
function pop_path()
{
    var pathid = Ext.brestore.pathid_stack.pop();
    var path = Ext.brestore.path_stack.pop();
    if (!pathid) {
        Ext.ComponentMgr.get('prevbp').disable();
        return false;
    }
    Ext.ComponentMgr.get('wherefield').setValue(path);

    Ext.brestore.pathid = pathid;
    Ext.brestore.path = path;
    return true;
}

function dirname(path) {
    var a = path.match( /(^.*\/).+\// );
    if (a) {
        return a[1];
    }
    return path;
}

Ext.namespace('Ext.brestore');
Ext.brestore.jobid=0;            // selected jobid
Ext.brestore.jobdate='';         // selected date
Ext.brestore.client='';          // selected client
Ext.brestore.rclient='';         // selected client for resto
Ext.brestore.storage='';         // selected storage for resto
Ext.brestore.path='';            // current path (without user location)
Ext.brestore.pathid=0;           // current pathid
Ext.brestore.root_path='';       // user location
Ext.brestore.media_store;        // media store 
Ext.brestore.option_vosb = false;
Ext.brestore.option_vafv = false;
Ext.brestore.option_vcopies = false;
Ext.brestore.dlglaunch;
Ext.brestore.fpattern;
Ext.brestore.use_filerelocation=false;
Ext.brestore.limit = 500;
Ext.brestore.offset = 0;
Ext.brestore.force_reload = 0;
Ext.brestore.path_stack = Array();
Ext.brestore.pathid_stack = Array();

function get_node_path(node)
{
   var temp='';
   for (var p = node; p; p = p.parentNode) {
       if (p.parentNode) {
          if (p.text == '/') {
             temp = p.text + temp;
          } else {
          temp = p.text + '/' + temp;
          }
       }
   }
   return Ext.brestore.root_path + temp;
}

function init_params(baseParams)
{
    baseParams['client']= Ext.brestore.client;
    
    if (Ext.brestore.option_vosb) {         
        baseParams['jobid'] = Ext.brestore.jobid;
    } else {
        baseParams['date'] = Ext.brestore.jobdate;
    }
    baseParams['offset'] = Ext.brestore.offset;
    baseParams['limit'] = Ext.brestore.limit;
    if (Ext.brestore.fpattern) {
        if (RegExp(Ext.brestore.fpattern)) {
            baseParams['pattern'] = Ext.brestore.fpattern;
        }
    }
    return baseParams;
}

function captureEvents(observable) {
    Ext.util.Observable.capture(
        observable,
        function(eventName) {
            console.info(eventName);
        },
        this
    );		
}

////////////////////////////////////////////////////////////////

Ext.BLANK_IMAGE_URL = 'ext/resources/images/default/s.gif';
Ext.onReady(function(){
    if (!Ext.version || Ext.version < 2.2) {
        alert("You must upgrade your extjs version to 2.2");
        return;
    }

    Ext.QuickTips.init();

    // Init tree for directory selection
    var tree_loader = new Ext.tree.TreeLoader({
            baseParams:{}, 
            dataUrl:'/cgi-bin/bweb/bresto.pl'
        });

    var tree = new Ext.tree.TreePanel({
        animate:true, 
        loader: tree_loader,
        enableDD:true,
        enableDragDrop: true,
        containerScroll: true,
        title: 'Directories',
        minSize: 100      
    });

    // set the root node
    var root = new Ext.tree.AsyncTreeNode({
        text: 'Select a client then a job',
        draggable:false,
        id:'source'
    });
    tree.setRootNode(root);
    Ext.brestore.tree = root;   // shortcut

    var click_cb = function(node, event) { 
        push_path();

        Ext.brestore.path = get_node_path(node);
        Ext.brestore.pathid = node.id;
        Ext.brestore.offset=0;
        Ext.brestore.fpattern = Ext.get('txt-file-pattern').getValue();
        where_field.setValue(Ext.brestore.path);
        update_limits();       
        file_store.load({params:init_params({action: 'list_files_dirs',
                                             path:Ext.brestore.path,
                                             node:node.id})
                        });
        return true;
    };
 
    tree.on('click', click_cb);

    tree.on('beforeload', function(e,b) {
//      console.info(b);
//      file_store.removeAll();
        return true;
    });
    tree.on('load', function(n,e) {
        if (!n.firstChild) {
           click_cb(n, e);
        }
        return true;
    });

    ////////////////////////////////////////////////////////////////
    
    var file_store = new Ext.data.Store({
        proxy: new Ext.data.HttpProxy({
            url: '/cgi-bin/bweb/bresto.pl',
            method: 'GET',
            params:{}
        }),
        
        reader: new Ext.data.ArrayReader({},
         Ext.data.Record.create([
          {name: 'fileid'    },
          {name: 'filenameid'},
          {name: 'pathid'    },
          {name: 'jobid'     },
          {name: 'name'      },
          {name: 'size',     type: 'int'  },
          {name: 'mtime',    type: 'date', dateFormat: 'Y-m-d h:i:s'},
          {name: 'type'}
        ]))
    });
    
    Ext.brestore.file_store=file_store;
    var cm = new Ext.grid.ColumnModel([{
        id:        'type',
        header:    'Type',
        dataIndex: 'filenameid',
        width:     30,
        css:       'white-space:normal;',
        renderer:   rd_file_or_dir
    },{
        id:        'cm-id', // id assigned so we can apply custom css 
                           // (e.g. .x-grid-col-topic b { color:#333 })
        header:    'File',
        dataIndex: 'name',
        width:     200,
        css:       'white-space:normal;'
    },{
        header:    "Size",
        dataIndex: 'size',
        renderer:  human_size,
        width:     60
    },{
        header:    "Date",
        dataIndex: 'mtime',
        renderer: Ext.util.Format.dateRenderer('Y-m-d h:i'),
        width:     100
    },{
        header:    "PathId",
        dataIndex: 'pathid',
        hidden: true
    },{
        header:    "FilenameId",
        dataIndex: 'filenameid',
        hidden: true
    },{
        header:    "FileId",
        dataIndex: 'fileid',
        hidden: true
    },{
        header:    "JobId",
        dataIndex: 'jobid',
        hidden: true
    }]);

    // by default columns are sortable
    cm.defaultSortable = true;

    // reset limits
    function update_limits() {
        Ext.get('txt-file-offset').dom.value = Ext.brestore.offset;
        Ext.get('txt-file-limit').dom.value = Ext.brestore.offset + Ext.brestore.limit;
    }

    // get limits from user input
    function update_user_limits() {
        var off = parseInt(Ext.get('txt-file-offset').getValue());
        var lim = parseInt(Ext.get('txt-file-limit').getValue());
        if (off >= 0 && lim >= 0 && off < lim) {
            Ext.brestore.offset = off;
            Ext.brestore.limit = lim - off;
        } else {
            update_limits();
        }
        Ext.brestore.fpattern = Ext.get('txt-file-pattern').getValue();
    }

    var file_paging_next = new Ext.Toolbar.Button({
        id: 'bp-file-next',
        icon: 'ext/resources/images/default/grid/page-next.gif',
        cls: '.x-btn-icon',
        tooltip: 'Next',
        handler: function(a,b,c) {
            update_user_limits();
            if (file_store.getCount() >= Ext.brestore.limit) {
                Ext.brestore.offset += Ext.brestore.limit;
                file_store.removeAll();
                file_versions_store.removeAll();
                file_store.load({params:init_params({action: 'list_files_dirs',
                                                     path:Ext.brestore.path,
                                                     node:Ext.brestore.pathid})
                                });
            }
        }
    });
    var file_paging_prev = new Ext.Toolbar.Button({
        id: 'bp-file-prev',
        icon: 'ext/resources/images/default/grid/page-prev.gif',
        cls: '.x-btn-icon',
        tooltip: 'Last',
        handler: function() {
            update_user_limits();
            if (Ext.brestore.offset > 0) {
                Ext.brestore.offset -= Ext.brestore.limit;
                if (Ext.brestore.offset < 0) {
                    Ext.brestore.offset=0;
                }
                file_store.removeAll();
                file_versions_store.removeAll();
                file_store.load({params:init_params({action: 'list_files_dirs',
                                                     path:Ext.brestore.path,
                                                     node:Ext.brestore.pathid})
                                });                        
            }
        }
    });
    var file_paging_pattern = new Ext.form.TextField({
        enableKeyEvents: true,           
        id: 'txt-file-pattern',
        text: 'pattern...'
    });
    file_paging_pattern.on('keyup', function(a, e) {
        if (e.getKey() == e. ENTER) {
            var re;
            var pattern = file_paging_pattern.getValue();
            if (pattern) {
                re = new RegExp(pattern, "i");                
            }
            if (re) {
                file_store.filter('name', re);
            } else {
                file_store.clearFilter(false);
            }
        }
    });
    var file_paging = new Ext.Toolbar({
        items: [file_paging_prev, {
            id: 'txt-file-offset',
            xtype: 'numberfield',
            width: 60,
            value: Ext.brestore.offset
        }, {
            xtype: 'tbtext',
            text: '-'
        }, {
            id: 'txt-file-limit',
            xtype: 'numberfield',
            width: 60,
            value: Ext.brestore.limit          
        }, file_paging_next, '->', file_paging_pattern, {
            id: 'bp-file-match',
            icon: 'ext/resources/images/default/grid/refresh.gif',
            cls: '.x-btn-icon',
            tooltip: 'Refresh',
            handler: function(a,b,c) {
                update_user_limits();
                file_store.removeAll();
                file_versions_store.removeAll();
                file_store.load({params:init_params({action: 'list_files_dirs',
                                                     path:Ext.brestore.path,
                                                     node:Ext.brestore.pathid})
                                });
            }
        }]
    });
    var file_grid = new Ext.grid.GridPanel({
        id: 'div-files',
        store: file_store,
        colModel: cm,
        ddGroup: 'TreeDD',
        enableDragDrop: true,
        selModel: new Ext.grid.RowSelectionModel(),
        loadMask: true,
        autoSizeColumns: true,
        autoShow: true,
        autoHeight: true,
        title: 'Directory content',
	cmargins: '5 0 0 0'
    });
    Ext.brestore.file_grid=file_grid;
    // when we reload the view,
    // we clear the file version box
    file_store.on('beforeload', function(e) {
        file_store.removeAll();
        file_versions_store.removeAll();
        return true;
    });

    // If we have more than limit elements, we can
    // display the next button
    file_store.on('load', function(e,o) {
        update_limits();
        if (e.getCount() >= Ext.brestore.limit) {
            file_paging_next.enable();
        } else {
            file_paging_next.disable();
        }

        if (Ext.brestore.offset) {
            file_paging_prev.enable();
        } else {
            file_paging_prev.disable();
        }

    });
    
/*
 *   file_store.on('loadexception', function(obj, options, response, e){
 *         console.info('store loadexception, arguments:', arguments);
 *         console.info('error = ', e);
 *   });
 *
 *   file_store.on('load', function(store, records, options){
 *         //store is loaded, now you can work with it's records, etc.
 *         console.info('store load, arguments:', arguments);
 *         console.info('Store count = ', store.getCount());
 *   });
 */
    file_grid.on('celldblclick', function(e) {
        var r = e.selModel.getSelected();

        if (r.json[1] == 0) {   // select a directory
           push_path();

           if (r.json[4] == '..') {
              Ext.brestore.path = dirname(Ext.brestore.path);
           } else if (r.json[4] == '/') {
              Ext.brestore.path = '/';
           } else if (r.json[4] != '.') {
              Ext.brestore.path = Ext.brestore.path + r.json[4] + '/';
           }
           Ext.brestore.pathid = r.json[2];
           Ext.brestore.filename = '';
           Ext.brestore.offset=0;
           Ext.brestore.fpattern = Ext.get('txt-file-pattern').getValue();
           where_field.setValue(Ext.brestore.path);
           update_limits();
           file_store.load({params:init_params({action: 'list_files_dirs',
                          path:Ext.brestore.path,
                          node:Ext.brestore.pathid})
                    });
        }
        return true;
    });
    // TODO: selection only when using dblclick
    file_grid.selModel.on('rowselect', function(e,i,r) { 
        if (r.json[1] == 0) {   // select a directory
           return true;
        }

        Ext.brestore.filename = r.json[4];
        file_versions_store.load({
            params:init_params({action: 'list_versions',
                                vafv:   Ext.brestore.option_vafv,
                                vcopies: Ext.brestore.option_vcopies,
                                pathid: r.json[2],
                                filenameid: r.json[1]
                               })
        });
        return true;
    });

    ////////////////////////////////////////////////////////////////

    var file_selection_store = new Ext.data.Store({
        proxy: new Ext.data.MemoryProxy(),
        
        reader: new Ext.data.ArrayReader({},
          Ext.data.Record.create([
              {name: 'jobid'     },
              {name: 'fileid'    },
              {name: 'filenameid'},
              {name: 'pathid'    },
              {name: 'name'      },
              {name: 'size',     type: 'int'  },
              {name: 'mtime'}//,    type: 'date', dateFormat: 'Y-m-d h:i:s'}
          ]))
    });

    var file_selection_cm = new Ext.grid.ColumnModel([
        {
            id:        'file-selection-id', 
            header:    "Name",
            dataIndex: 'name',
            width:     250
        },{
            header:    "JobId",
            width:     50,
            dataIndex: 'jobid'
        },{
            header:    "Size",
            dataIndex: 'size',
            renderer:  human_size,
            width:     50
        },{
            header:    "Date",
            dataIndex: 'mtime',
//            renderer: Ext.util.Format.dateRenderer('Y-m-d h:i'),
            width:     120
        },{
            header: 'PathId',
            dataIndex: 'pathid',
            hidden: true
        },{
            header: 'Filenameid',
            dataIndex: 'filenameid',
            hidden: true
        },{
            header: 'FileId',
            dataIndex: 'fileid',
            hidden: true
        }
    ]);

    // create the grid
    var file_selection_grid = new Ext.grid.GridPanel({
        colModel: file_selection_cm,
        store: file_selection_store,
        ddGroup : 'TreeDD',
        enableDragDrop: false,
        selModel: new Ext.grid.RowSelectionModel(),
        loadMask: true,
        height: 200,
//        autoHeight: true,
        minSize: 75,
        maxSize: 250,
	cmargins: '5 0 0 0'
    });

    var file_selection_record = Ext.data.Record.create(
      {name: 'jobid'},
      {name: 'fileid'},
      {name: 'filenameid'},
      {name: 'pathid'},
      {name: 'size'},
      {name: 'mtime'}
    );
//    captureEvents(file_selection_grid);
//    captureEvents(file_selection_store);

    func1 = function(e,b,c) { 
        if (e.browserEvent.keyCode == 46) {
            var m = file_selection_grid.getSelections();
            if(m.length > 0) {
                for(var i = 0, len = m.length; i < len; i++){                    
                    file_selection_store.remove(m[i]); 
                }
            }
        }
    };
    file_selection_grid.on('keypress', func1);
    
    ////////////////////////////////////////////////////////////////

    var file_versions_store = new Ext.data.Store({
        proxy: new Ext.data.HttpProxy({
            url: '/cgi-bin/bweb/bresto.pl',
            method: 'GET',
            params:{offset:0, limit:50 }
        }),
        
        reader: new Ext.data.ArrayReader({},
           Ext.data.Record.create([
               {name: 'fileid'    },
               {name: 'filenameid'},
               {name: 'pathid'    },
               {name: 'jobid'     },
               {name: 'volume'    },
               {name: 'inchanger' },
               {name: 'md5'       },
               {name: 'size',     type: 'int'  },
               {name: 'mtime',    type: 'date', dateFormat: 'Y-m-d h:i:s'}
           ]))
    });

    var file_versions_cm = new Ext.grid.ColumnModel([{
        id:        'file-version-id',
        dataIndex: 'name',
        hidden: true
    },{
        header:    "InChanger",
        dataIndex: 'inchanger',
        width:     60,
        renderer:  rd_vol_is_online
    },{
        header:    "Volume",
        dataIndex: 'volume',
        width:     128
    },{
        header:    "JobId",
        width:     50,
        dataIndex: 'jobid'
    },{
        header:    "Size",
        dataIndex: 'size',
        renderer:  human_size,
        width:     50
    },{
        header:    "Date",
        dataIndex: 'mtime',
        renderer: Ext.util.Format.dateRenderer('Y-m-d h:i'),
        width:     100
    },{
        header:    "CheckSum",
        dataIndex: 'md5',
        width:     160
    },{
        header:    "pathid",
        dataIndex: 'pathid',
        hidden: true
    },{
        header:    "filenameid",
        dataIndex: 'filenameid',
        hidden: true
    },{
        header:    "fileid",
        dataIndex: 'fileid',
        hidden: true
    }]);

    // by default columns are sortable
    file_versions_cm.defaultSortable = true;

    // create the grid
    var file_versions_grid = new Ext.grid.GridPanel({
        id: 'div-file-versions',
        store: file_versions_store,
        colModel: file_versions_cm,
        ddGroup : 'TreeDD',
        enableDragDrop: true,
        selModel: new Ext.grid.RowSelectionModel(),
        loadMask: true,
        autoHeight: true,
        autoScroll: true,
        title: 'File version',
	cmargins: '5 0 0 0'
    });

    file_versions_grid.on('rowdblclick', function(e) { 
        alert(e) ; file_versions_store.removeAll(); return true;
    });


    ////////////////////////////////////////////////////////////////

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

    var replace_store = new Ext.data.SimpleStore({
        fields: ['value', 'text'],
        data : [['never', 'Never'],['always', 'Always']]
    });

    var replace_combo = new Ext.form.ComboBox({
        fieldLabel: 'Replace',
        store: replace_store,
        displayField:'text',
        typeAhead: true,
        mode: 'local',
        triggerAction: 'all',
        emptyText:'Replace mode...',
        selectOnFocus:true,
        forceSelection: true,
        width:135
    });

    client_combo.on('valid', function(e) { 
        Ext.brestore.client = e.getValue();
        Ext.brestore.jobid=0;
        Ext.brestore.jobdate = '';
        Ext.brestore.root_path='';
        Ext.brestore.offset=0;
        job_combo.clearValue();
        file_store.removeAll();
        file_versions_store.removeAll();
        root.collapse(false, false);
        while(root.firstChild){
            root.removeChild(root.firstChild);
        }
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
        store:           job_store,
        fieldLabel:      'Jobs',
        displayField:    'jobname',
        mode:            'local',
        triggerAction:   'all',
        emptyText:       'Select a job...',
        typeAhead:       true,
        selectOnFocus:   true,
        loadMask:        true,
        forceSelection:  true,
        width:           350
    });

    job_combo.on('select', function(e,c) {
        Ext.brestore.jobid = c.json[0];
        Ext.brestore.jobdate = c.json[1];
        root.setText("Root");
        tree_loader.baseParams = init_params({init:1, action: 'list_dirs'});
        root.reload();

        file_store.load({params:init_params({action: 'list_files_dirs',
                                             path:Ext.brestore.path,
                                             node:Ext.brestore.pathid})
                        });
    });

    var where_field = new Ext.form.TextField({
            fieldLabel: 'Location',
            id: 'wherefield',
            name: 'where',
            width:275,
            allowBlank:false
    });

    function sel_option(item, check)
    {
        if (item.id == 'id_vosb') {
           Ext.brestore.option_vosb = check;
        }
        if (item.id == 'id_vafv') {
           Ext.brestore.option_vafv = check;
        }
        if (item.id == 'id_vcopies') {
           Ext.brestore.option_vcopies = check;
        }
    }

    var menu = new Ext.menu.Menu({
        id: 'div-main-menu',
        items: [ 
           new Ext.menu.CheckItem({
                id: 'id_vosb',
                text: 'View only selected backup',
                checked: Ext.brestore.option_vosb,
                checkHandler: sel_option
            }),
           new Ext.menu.CheckItem({
                id: 'id_vafv',
                text: 'View all file versions',
                checked: Ext.brestore.option_vafv,
                checkHandler: sel_option
              }),
           new Ext.menu.CheckItem({
                id: 'id_copies',
                text: 'View copies version',
                checked: Ext.brestore.option_vcopies,
                checkHandler: sel_option
            })
        ]
    });

    var tb = new Ext.Toolbar({
        items: [
            client_combo,
            job_combo,
            '-',
            {
                id: 'prevbp',
                cls: 'x-btn-icon',
                icon: 'prev.png',
                disabled: true,
                handler: function() { 
                    if (!pop_path()) {
                        return;
                    }
                    Ext.brestore.offset=0;
                    update_limits();       
                    file_store.load({params:init_params({action: 'list_files_dirs',
                                                         node:Ext.brestore.pathid})
                                    });
                    
                }

            },
            { 
              text: 'Change location',
              cls:'x-btn-text-icon',
              handler: function() { 
                  var where = where_field.getValue();
                  if (!where) {
                      Ext.MessageBox.show({
                          title: 'Bad parameter',
                          msg: 'Location is empty!',
                          buttons: Ext.MessageBox.OK,
                          icon: Ext.MessageBox.ERROR
                      });
                      return;
                  }
                  Ext.brestore.root_path=where;
                  root.setText(where);
                  tree_loader.baseParams = init_params({ 
                      action:'list_dirs', path: where
                  });
                  root.reload();
              }
            },
            where_field,
            {
                text:'Options',
                iconCls: 'bmenu',  // <-- icon
                menu: menu  // assign menu by instance
            },
            '->',                     // Fill
            {                         // TODO: put this button on south panel
                icon: 'mR.png', // icons can also be specified inline
                cls: 'x-btn-text-icon',
                tooltip: 'Run restore',
                text: 'Run restore',
                handler: function() {
                    if (!file_selection_store.data.items.length) {
                        Ext.MessageBox.show({
                            title: 'Empty selection',
                            msg: 'Your object selection list is empty!',
                            buttons: Ext.MessageBox.OK,
                            icon: Ext.MessageBox.ERROR
                        });
                    } else {
                        display_run_job();
                    }
                }
            }]});
    tb.render('tb-div');

    ////////////////////////////////////////////////////////////////
    
    // Define Main interface
    var MainView = new Ext.Viewport({
        id:'mainview-panel',
        title: 'Bacula Web Restore',
        layout:'border',
        bodyBorder: false,
        renderTo: Ext.getBody(),
        defaults: {
	    collapsible: false,
            split: true,
	    animFloat: false,
	    autoHide: false,
            autoScroll: true,
	    useSplitTips: true
        },
        tbar: tb,
        items: [
            {
                title: 'Directories',
                region: 'west',
                width: '25%',
                minSize: 120,
                items: tree
            }, {
                title: 'Directory content',
                region: 'center',
                minSize: '33%',
                items: file_grid,
                bbar: file_paging
            }, {
                title: 'File version',
                region: 'east',
                width: '42%',
                minSize: 100,
                items: file_versions_grid
              
            }, {
                title: 'Restore selection',
                region: 'south',
                height: 200,
                autoScroll: false,
                items: file_selection_grid
            }
        ]});
    client_store.load({params:{action: 'list_client'}});

    // data.selections[0].json[]
    // data.node.id
    // http://extjs.com/forum/showthread.php?t=12582&highlight=drag+drop

    var horror_show1 = file_selection_grid.getView().el.dom;
    var ddrow = new Ext.dd.DropTarget(horror_show1, {
        ddGroup : 'TreeDD',
        copy:false,
        notifyDrop : function(dd, e, data){
            var r;
            if (data.selections) {
                if (data.grid.id == 'div-files') {
                    for(var i=0;i<data.selections.length;i++) {
                        var name = data.selections[i].json[4];
                        var fnid = data.selections[i].json[1];
                        if (fnid == 0 && name != '..') {
                            name = name + '/';
                        }
                        r = new file_selection_record({
                            jobid:     data.selections[0].json[3],
                            fileid:    data.selections[i].json[0],
                            filenameid:data.selections[i].json[1],
                            pathid:    data.selections[i].json[2],
                            name: Ext.brestore.path + ((name=='..')?'':name),
                            size:      data.selections[i].json[5],
                            mtime:     data.selections[i].json[6]
                        });
                        file_selection_store.add(r);
                    }
                }
                
                if (data.grid.id == 'div-file-versions') {
                    r = new file_selection_record({
                        jobid:     data.selections[0].json[3],
                        fileid:    data.selections[0].json[0],
                        filenameid:data.selections[0].json[1],
                        pathid:    data.selections[0].json[2],
                        name: Ext.brestore.path + Ext.brestore.filename,
                        size:      data.selections[0].json[7],
                        mtime:     data.selections[0].json[8]     
                    });
                    file_selection_store.add(r)
                }
            }
            
            if (data.node) {
                var path= get_node_path(data.node);
                r = new file_selection_record({
                    jobid:     data.node.attributes.jobid,
                    fileid:    0,
                    filenameid:0,
                    pathid:    data.node.id,
                    name:      path,
                    size:      4096,
                    mtime:     0
                });
                file_selection_store.add(r)
            }
            
            return true;
        }});

    function force_reload_media_store() {
       Ext.brestore.force_reload = 1;
       reload_media_store();
       Ext.brestore.force_reload = 0;
    }

    function reload_media_store() {
        Ext.brestore.media_store.removeAll();
        var items = file_selection_store.data.items;
        var tab_fileid=new Array();
        var tab_dirid=new Array();
        var tab_jobid=new Array();
        var enable_compute=false;
        for(var i=0;i<items.length;i++) {
            if (items[i].data['fileid']) {
               tab_fileid.push(items[i].data['fileid']);
            } else {
               tab_dirid.push(items[i].data['pathid']);
               enable_compute=true;
            }
            tab_jobid.push(items[i].data['jobid']);
        }
        var res = tab_fileid.join(",");
        var res2 = tab_jobid.join(",");
        var res3 = tab_dirid.join(",");

        Ext.brestore.media_store.baseParams = init_params({
            action: 'get_media', 
            jobid: res2, 
            fileid: res,
            dirid: res3,
            force: Ext.brestore.force_reload
        });
        Ext.brestore.media_store.load();
        if (enable_compute) {
            Ext.get('reload_media').show();
        } else {
            Ext.get('reload_media').hide();
        }
    };

    function display_run_job() {
        if (Ext.brestore.dlglaunch) {
            reload_media_store();
            Ext.brestore.dlglaunch.show();
            return 0;
        }
        var rclient_combo = new Ext.form.ComboBox({
            value: Ext.brestore.client,
            fieldLabel: 'Client',
            hiddenName:'client',
            store: client_store,
            displayField:'name',
            typeAhead: true,
            mode: 'local',
            triggerAction: 'all',
            emptyText:'Select a client...',
            forceSelection: true,
            value:  Ext.brestore.client,
            selectOnFocus:true
        });
        var where_text = new Ext.form.TextField({
            fieldLabel: 'Where',
            name: 'where',
            value: '/tmp/bacula-restore'
        });
        var stripprefix_text = new Ext.form.TextField({
            fieldLabel: 'Strip prefix',
            name: 'strip_prefix',
            value: '',
            disabled: 1
        });
        var addsuffix_text = new Ext.form.TextField({
            fieldLabel: 'Add suffix',
            name: 'add_suffix',
            value: '',
            disabled: 1
        });
        var addprefix_text = new Ext.form.TextField({
            fieldLabel: 'Add prefix',
            name: 'add_prefix',
            value: '',
            disabled: 1
        });
        var rwhere_text = new Ext.form.TextField({
            fieldLabel: 'Where regexp',
            name: 'regexp_where',
            value: '',
            disabled: 1
        });
        var useregexp_bp = new Ext.form.Checkbox({
            fieldLabel: 'Use regexp',
            name: 'use_regexp',
            disabled: 1,
            checked: 0,
            handler: function(bp,state) {
                if (state) {
                    addsuffix_text.disable();
                    addprefix_text.disable();
                    stripprefix_text.disable();
                    rwhere_text.enable();
                } else {
                    addsuffix_text.enable();
                    addprefix_text.enable();
                    stripprefix_text.enable();
                    rwhere_text.disable();
                }
            }
        }); 

        var usefilerelocation_bp = new Ext.form.Checkbox({
            fieldLabel: 'Use file relocation',
            name: 'use_relocation',
            checked: 0
        });
       
        usefilerelocation_bp.on('check', function(bp,state) {
            if (state) {
                where_text.disable();
                useregexp_bp.enable();
                if (useregexp_bp.getValue()) {
                    addsuffix_text.disable();
                    addprefix_text.disable();
                    stripprefix_text.disable();
                    rwhere_text.enable();
                } else {
                    addsuffix_text.enable();
                    addprefix_text.enable();
                    stripprefix_text.enable();
                    rwhere_text.disable();
                }
            } else {
                where_text.enable();
                addsuffix_text.disable();
                addprefix_text.disable();
                stripprefix_text.disable();
                useregexp_bp.disable();
                rwhere_text.disable();
            }
            Ext.brestore.use_filerelocation = state;
        }); 

        useregexp_bp.on('check', function(bp,state) {
            if (state) {
                addsuffix_text.disable();
                addprefix_text.disable();
                stripprefix_text.disable();
                rwhere_text.enable();
            } else {
                addsuffix_text.enable();
                addprefix_text.enable();
                stripprefix_text.enable();
                rwhere_text.disable();
            }
        }); 
        
        var media_store = Ext.brestore.media_store = new Ext.data.Store({
            proxy: new Ext.data.HttpProxy({
                url: '/cgi-bin/bweb/bresto.pl',
                method: 'GET',
                params:{offset:0, limit:50 }
            }),
            
            reader: new Ext.data.ArrayReader({
            }, Ext.data.Record.create([
                {name: 'volumename'},
                {name: 'enabled'   },
                {name: 'inchanger' }
            ]))
        });
        
        var media_cm = new Ext.grid.ColumnModel(
            [{
                header:    "InChanger",
                dataIndex: 'inchanger',
                width:     60,
                renderer:  rd_vol_is_online
            }, {
                header:    "Volume",
                id:        'volumename', 
                dataIndex: 'volumename',
                width:     200
            }
            ]);
        
        // create the grid
        var media_grid = new Ext.grid.GridPanel({
            id: 'div-media-grid',
            store: media_store,
            colModel: media_cm,
            enableDragDrop: false,
            loadMask: true,
            width: 400,
            height: 180,
            frame: true
        });

        var form_panel = new Ext.FormPanel({
            labelWidth : 75, // label settings here cascade unless overridden
            frame      : true,
            bodyStyle  : 'padding:5px 5px 0',
            width      : 250,
//          autoHeight : true,
            items: [{
                xtype       :   'tabpanel',
                autoTabs    :   true,
                activeTab   :   0,
                border      :   false,
                bodyStyle   :  'padding:5px 5px 0',
                deferredRender : false,
                items: [{
                    xtype   :   'panel',
                    title   :   'Restore details',
                    items   :  [{
                        xtype          : 'fieldset',
                        title          : 'Restore options',
                        autoHeight     : true,
                        defaults       : {width: 210},
                        bodyStyle  : 'padding:5px 5px 0',
                        items :[ rclient_combo, where_text, replace_combo ]
                    }, {
                        xtype          : 'fieldset',
                        title          : 'Media needed',
                        autoHeight     : true,
                        defaults       : {width: 280},
                        bodyStyle  : 'padding:5px 5px 0',
                        items :[ media_grid, 
                                 {xtype: 'button', id: 'reload_media',
                                  text: 'Compute with directories',
                                  tooltip: 'Can take long time...',
                                  handler:force_reload_media_store}]
                    }],
                }, {
                    xtype   :   'panel',
                    title   :   'Advanced',
                    items   :  [{
                        xtype          : 'fieldset',
                        title          : 'File relocation',
                        autoHeight     : true,
                        defaults       : {width: 210},
                        bodyStyle      : 'padding:5px 5px 0',
                        items :[ usefilerelocation_bp, stripprefix_text, 
                                 addprefix_text, addsuffix_text, useregexp_bp,
                                 rwhere_text ]
                    },{
                        xtype          : 'fieldset',
                        title          : 'Other options',
                        autoHeight     : true,
                        defaults       : {width: 210},
                        bodyStyle      : 'padding:5px 5px 0',
                        defaultType    : 'textfield',
                        items :[{
                            name: 'when_text',
                            fieldLabel: 'When',
                            disabled: true,
                            tooltip: 'YYY-MM-DD HH:MM'
                        }, {
                            name: 'prio_text',
                            fieldLabel: 'Priority',
                            disabled: true,
                            tooltip: '1-100'
                        }]
                    }]
                }]
            }],
            
            buttons: [{
                text: 'Run',
                handler: function() { 
                    if(!form_panel.getForm().isValid()) { alert("invalid") } 
                    else { launch_restore() }
                }
            },{
                text: 'Cancel',
                handler: function() { Ext.brestore.dlglaunch.hide(); }
            }]
        });

        Ext.brestore.dlglaunch = new Ext.Window({
            applyTo     : 'resto-div',
            title       : 'Restore selection',
            layout      : 'fit',
            width       : 400,
            height      : 500,
            closeAction :'hide',
            plain       : true,
            items       : form_panel
        });

//        Ext.brestore.dlglaunch.addKeyListener(27, 
//                                              Ext.brestore.dlglaunch.hide, 
//                                              Ext.brestore.dlglaunch);
/*        
 *       var storage_store = new Ext.data.Store({
 *           proxy: new Ext.data.HttpProxy({
 *               url: '/cgi-bin/bweb/bresto.pl',
 *               method: 'GET',
 *               params:{action:'list_storage'}
 *           }),
 *           
 *           reader: new Ext.data.ArrayReader({
 *           }, Ext.data.Record.create([
 *               {name: 'name' }
 *           ]))
 *       });
 */      
        ////////////////////////////////////////////////////////////////

        function launch_restore() {
            var items = file_selection_store.data.items;
            var tab_fileid=new Array();
            var tab_dirid=new Array();
            var tab_jobid=new Array();
            for(var i=0;i<items.length;i++) {
                if (items[i].data['fileid']) {
                    tab_fileid.push(items[i].data['fileid']);
                } else {
                    tab_dirid.push(items[i].data['pathid']);
                }
                tab_jobid.push(items[i].data['jobid']);
            }
            var res = ';fileid=' + tab_fileid.join(";fileid=");
            var res2 = ';dirid=' + tab_dirid.join(";dirid=");
            var res3 = ';jobid=' + tab_jobid.join(";jobid=");

            var res4 = ';client=' + rclient_combo.getValue();
//            if (storage_combo.getValue()) {
//                res4 = res4 + ';storage=' + storage_combo.getValue();
//            }
            if (Ext.brestore.use_filerelocation) {
                if (useregexp_bp.getValue()) {
                    res4 = res4 + ';regexwhere=' + rwhere_text.getValue();
                } else {
                    var reg = new Array();
                    if (stripprefix_text.getValue()) {
                        reg.push('!' + stripprefix_text.getValue() + '!!i');
                    }
                    if (addprefix_text.getValue()) {
                        reg.push('!^!' + addprefix_text.getValue() + '!');
                    }
                    if (addsuffix_text.getValue()) {
                        reg.push('!([^/])$!$1' + addsuffix_text.getValue() + '!');
                    }
                    res4 = res4 + ';regexwhere=' + reg.join(',');
                }
            } else {
                res4 = res4 + ';where=' + where_text.getValue();
            }
            window.location='/cgi-bin/bweb/bresto.pl?action=restore' + res + res2 + res3 + res4;
        } // end launch_restore


        ////////////////////////////////////////////////////////////////
        reload_media_store();
        Ext.brestore.dlglaunch.show();
//      storage_store.load({params:{action: 'list_storage'}});
    }
});