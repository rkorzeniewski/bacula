var SlideWindowClass = Class.create({

	windowId: null,
	window: null,
	showEl: null,
	hideEl: null,
	fullSizeEl : null,
	search: null,
	toolbar: null,
	tools: null,
	options: null,
	configurationObj: null,
	loadRequest : null,
	repeaterEl: null,
	gridEl: null,
	checked: [],
	objects: {},

	size: {
		widthNormal : '437px',
		heightNormal : '325px',
		widthHalf : '437px',
		heightHalf : '586px',
		widthFull : '899px',
		heightFull : '586px'
	},

	elements : {
		containerSuffix : '-slide-window-container',
		configurationWindows : 'div.configuration',
		configurationProgress: 'div.configuration-progress',
		contentItems : 'slide-window-element',
		contentAlternatingItems : 'slide-window-element-alternating',
		toolsButtonSuffix : '-slide-window-tools',
		optionsButtonSuffix : '-slide-window-options',
		actionsSuffix : '-slide-window-actions',
		toolbarSuffix : '-slide-window-toolbar',
		titleSuffix : '-slide-window-title'
	},

	initialize: function(windowId, data) {
		if(typeof(windowId) == "undefined") {
			return;
		}

		this.windowId = windowId;
		this.window = $(this.windowId + this.elements.containerSuffix);
		this.tools = $(this.windowId + this.elements.toolsButtonSuffix);
		this.options = $(this.windowId + this.elements.optionsButtonSuffix);
		
		if(data.hasOwnProperty('showId')) {
				this.showEl = $(data.showId);
		} else {
			alert('slide-window.js - "showId" property does not exists.');
			return false;
		}

		if(data.hasOwnProperty('hideId')) {
			this.hideEl = $(data.hideId);
		} else {
			alert('slide-window.js - "hideId" property does not exists.');
			return false;
		}

		if(data.hasOwnProperty('fullSizeId')) {
			this.fullSizeEl = $(data.fullSizeId);
		} else {
			alert('slide-window.js - "fullSizeId" property does not exists.');
			return false;
		}

		if(data.hasOwnProperty('search')) {
			this.search = $(data.search);
		} else {
			alert('slide-window.js - "search" property does not exists.');
			return false;
		}
		this.setEvents();
	},

	objectExists: function(key) {
		return this.objects.hasOwnProperty(key);
	},

	registerObj: function(key, obj) {
		if(this.objectExists(key) === false) {
			this.objects[key] = obj;
		}
	},

	getObj: function(key) {
		var obj = null;
		if(this.objectExists(key) === true) {
			obj = this.objects[key];
		}
		return obj;
	},

	setEvents: function() {
		this.showEl.observe('click', function(){
			this.openWindow();
		}.bind(this));

		this.hideEl.observe('click', function(){
			this.closeWindow();
		}.bind(this));
		
		this.fullSizeEl.observe('click', function(){
			this.resetSize();
		}.bind(this));

		this.search.observe('keyup', function(){
			this.setSearch();
		}.bind(this));

		this.tools.observe('click', function() {
			this.toggleToolbar();
		}.bind(this));

		this.options.observe('click', function() {
			this.toggleToolbar();
		}.bind(this));
	},
	
	openWindow : function() {
		this.hideOtherWindows();
		Effect.toggle(this.window, 'slide', { duration: 0.3, afterFinish : function() {
				this.normalSizeWindow();
			}.bind(this)
		});
	},

	closeWindow : function() {
		Effect.toggle(this.window, 'slide', { duration: 0.3, afterFinish : function() {
				this.resetSize();
			}.bind(this)
		});
	},
	
	resetSize : function() {
		if(this.isConfigurationOpen()) {
			if(this.isFullSize()) {
				this.halfSizeWindow();
			} else if(this.isHalfSize()) {
					this.normalSizeWindow();
			} else if (this.isNormalSize()){
				this.halfSizeWindow();
			} else {
				this.normalSizeWindow();
			}
		} else {
			if(this.isFullSize()) {
				this.normalSizeWindow();
			} else if(this.isHalfSize() || this.isNormalSize()) {
				this.fullSizeWindow();
			}
		}
	},

	isNormalSize: function() {
		return (this.window.getWidth()  + 'px' == this.size.widthNormal && this.window.getHeight()  + 'px' == this.size.heightNormal);
	},

	isHalfSize: function() {
		return (this.window.getWidth()  + 'px' == this.size.widthHalf && this.window.getHeight()  + 'px' == this.size.heightHalf);
	},

	isFullSize: function() {
		return (this.window.getWidth()  + 'px' == this.size.widthFull && this.window.getHeight()  + 'px' == this.size.heightFull);
	},

	normalSizeWindow: function() {
			new Effect.Morph(this.window, {style : 'width: ' + this.size.widthNormal + '; height: ' + this.size.heightNormal + ';', duration : 0.4});
	},
	
	halfSizeWindow: function() {
			new Effect.Morph(this.window, {style : 'width: ' + this.size.widthHalf + '; height: ' + this.size.heightHalf + ';', duration : 0.4});
	},
	
	fullSizeWindow: function() {
			new Effect.Morph(this.window, {style : 'width: ' + this.size.widthFull + '; height: ' + this.size.heightFull + ';', duration : 0.4});
	},

	hideOtherWindows: function() {
		$$('.slide-window-container').each(function(el, index) {
			el.setStyle({
				display : 'none',
				width : this.size.widthNormal,
				height : this.size.heightNormal
			});
		}.bind(this));
	},

	setConfigurationObj: function(obj) {
		this.configurationObj = obj;
	},

	setWindowElementsEvent: function(repeaterEl, gridEl, requestObj) {
		this.repeaterEl = repeaterEl;
		this.gridEl = gridEl;
		this.loadRequest = requestObj;
		this.markAllChecked(false);
		this.setLoadRequest();
		this.postWindowOpen();
	},

	setLoadRequest: function() {
		var dataList = [];
		if($(this.gridEl)) {
			dataList = $(this.gridEl).select('tr');
			this.makeSortable();
		} else if ($(this.repeaterEl + '_Container')) {
			dataList = $(this.repeaterEl + '_Container').select('div.slide-window-element');
		}

		dataList.each(function(tr) {
			$(tr).observe('click', function(index, clickedEl) {
				var target = clickedEl.target || clickedEl.srcElement;
				var clicked = $(target.id);
				// for element selection action (clicked checkbox) configuration window is not open
				if(clicked && clicked.hasAttribute('type') && clicked.readAttribute('type') == 'checkbox') {
					return;
				}

				var el = $(tr).down('input[type=hidden]')
				if(el) {
					var val = el.getValue();
					this.loadRequest.ActiveControl.CallbackParameter = val;
					this.loadRequest.dispatch();
					this.configurationObj.openConfigurationWindow(this);
				}
			}.bind(this, tr));
		}.bind(this));
	},

	isConfigurationOpen: function() {
		var is_open = false;
		$$(this.elements.configurationWindows, this.elements.configurationProgress).each(function(el) {
			if(el.getStyle('display') == 'block') {
				is_open = true;
				throw $break;
			}
		}.bind(is_open));
		return is_open;
	},

	sortTable: function (col, reverse) {
		var table = document.getElementById(this.gridEl);
		var tb = table.tBodies[0], tr = Array.prototype.slice.call(tb.rows, 0), i;
		reverse = -((+reverse) || -1);
		tr = tr.sort(function (a, b) {
			var val;
			var val_a = a.cells[col].textContent.trim();
			var val_b = b.cells[col].textContent.trim();
			if (!isNaN(parseFloat(val_a)) && isFinite(val_a) && !isNaN(parseFloat(val_b)) && isFinite(val_b)) {
				val = val_a - val_b
			} else {
				val = val_a.localeCompare(val_b);
			}
			return reverse * (val);
		});
		for(i = 0; i < tr.length; ++i) tb.appendChild(tr[i]);
	},

	makeSortable: function () {
		var self = this;
		var table = document.getElementById(this.gridEl);
		table.tHead.style.cursor = 'pointer';
		var th = table.tHead, i;
		th && (th = th.rows[0]) && (th = th.cells);
		if (th) {
			i = th.length;
		} else {
			return;
		}
		var downCounter = 0;
		// skip first column if in column header is input (checkbox for elements mark)
		if (th[0].childNodes[0].nodeName == "INPUT") {
			downCounter = 1;
		}
		while (--i >= downCounter) (function (i) {
			var dir = 1;
			th[i].addEventListener('click', function () {
				self.sortTable(i, (dir = 1 - dir));
			});
		}(i));
	},

	setSearch: function() {
		var search_pattern = new RegExp(this.search.value)
		$$('div[id="' + this.windowId + this.elements.containerSuffix + '"] div.' + this.elements.contentItems).each(function(value){
				
				if(search_pattern.match(value.childNodes[2].textContent) == false) {
					value.setStyle({'display' : 'none'});
				} else {
					value.setStyle({'display' : ''});
				}
			}.bind(search_pattern));
			
			$$('div[id="' + this.windowId + this.elements.containerSuffix + '"] tr.' + this.elements.contentItems + ', div[id="' + this.windowId + this.elements.containerSuffix + '"] tr.' + this.elements.contentAlternatingItems).each(function(value){
				if(search_pattern.match(value.down('div').innerHTML) == false) {
					value.setStyle({'display' : 'none'});
				} else {
					value.setStyle({'display' : ''});
				}
			}.bind(search_pattern));
	},
	setElementsCount : function() {
		var title_el = $(this.windowId + this.elements.titleSuffix);
		var elements_count = $$('div[id="' + this.windowId + this.elements.containerSuffix + '"] div.' + this.elements.contentItems).length || $$('div[id="' + this.windowId + this.elements.containerSuffix + '"] tr.' + this.elements.contentItems + ', div[id="' + this.windowId + this.elements.containerSuffix + '"] tr.' + this.elements.contentAlternatingItems).length;
		var count_el = $(this.windowId + this.elements.titleSuffix).getElementsByTagName('span')[0];
		$(count_el).update(' (' + elements_count + ')');
	},
	toggleToolbar: function() {
		if (this.isToolbarOpen() === false) {
			this.markAllChecked(false);
		}
		Effect.toggle($(this.windowId + this.elements.toolbarSuffix), 'slide', { duration: 0.2});
	},
	isToolbarOpen: function() {
		return $(this.windowId + this.elements.toolbarSuffix).visible();
	},
	setActions: function() {
		var table = $(this.window).down('table');
		var checkboxes = table.select('input[name="actions_checkbox"]');
		checkboxes.each(function(el) {
			el.observe('change', function() {
				var is_checked = this.is_any_checked(checkboxes);
				if(is_checked === true && !this.areActionsOpen()) {
					this.showActions();
				} else if (is_checked === false && this.areActionsOpen()) {
					this.hideActions();
				}
			}.bind(this));
                }.bind(this));
	},
	is_any_checked: function(checkboxes) {
		var is_checked = false;
		checkboxes.each(function(ch) {
			if(ch.checked == true) {
				is_checked = true;
				throw $break;
			}
		});
		return is_checked;
	},

	markAllChecked: function(check) {
		this.checked = [];
		var table = $(this.window).down('table');
		var checkboxes = table.select('input[name="actions_checkbox"]');
		var containerId;
		if(checkboxes.length > 0) {
			checkboxes.each(function(ch, index) {
				if (ch.up('tr').visible()) {
					containerId = ch.getAttribute('rel');
					if (ch.checked == false && check == true) {
						ch.checked = true;
					} else if (ch.checked == true && check == false) {
						ch.checked = false;
					}
					this.markChecked(containerId, ch.checked, ch.value);
				}
			}.bind(this));
			if (containerId) {
				this.packChecked(containerId);
			}
		}

		if(check) {
			this.showActions();
		} else {
			this.hideActions();
		}
	},
	markChecked: function(containerId, checked, param, pack) {
		if (this.checked.length == 0) {
			if(checked == true) {
				this.checked.push(param);
			}
		} else {
			index = this.checked.indexOf(param);
			if(checked === true && index == -1) {
				this.checked.push(param);
			} else if (checked === false && index > -1) {
				this.checked.splice(index, 1);
			}
		}

		if(checked == true) {
			this.showActions();
		} else if(this.checked.length == 0) {
			this.hideActions();
		}

		if (pack === true) {
			this.packChecked(containerId);
		}
	},
	packChecked: function(containerId) {
		var values_packed = this.checked.join(';');
		$(containerId).setValue(values_packed);
	},
	showActions: function() {
		if (this.areActionsOpen()) {
			return;
		}
		if (this.isToolbarOpen()) {
			this.toggleToolbar();
		}
		Effect.toggle($(this.windowId + this.elements.actionsSuffix), 'slide', { duration: 0.2});
	},
	hideActions: function() {
		if (!this.areActionsOpen()) {
			return;
		}
		this.checked = [];
		Effect.toggle($(this.windowId + this.elements.actionsSuffix), 'slide', { duration: 0.2});
	},
	areActionsOpen: function() {
		return $(this.windowId + this.elements.actionsSuffix).visible();
	},
	postWindowOpen: function() {
		this.setActions();
		this.setElementsCount();
	}
});

var SlideWindow = new SlideWindowClass()

document.observe("dom:loaded", function() {
	if(Prototype.Browser.IE  || Prototype.Browser.Gecko) {
		$$('input[type=checkbox], input[type=submit], input[type=radio], a').each(function(el) {
			el.observe('focus', function() {
				el.blur();
			}.bind(el));
		});
	}
});
