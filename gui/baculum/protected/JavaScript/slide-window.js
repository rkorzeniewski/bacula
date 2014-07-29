var SlideWindowClass = Class.create({

	windowId: null,
	window: null,
	showEl: null,
	hideEl: null,
	fullSizeEl : null,
	search: null,
	toolbar: null,

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
		contentItems : 'slide-window-element',
		contentAlternatingItems : 'slide-window-element-alternating',
		toolsButtonSuffix : '-slide-window-tools',
		toolbarSuffix : '-slide-window-toolbar'
	},

	initialize: function(windowId, data) {

		this.windowId = windowId;
		this.window = $(this.windowId + this.elements.containerSuffix);
		this.tools = $(this.windowId + this.elements.toolsButtonSuffix);
		
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

	isConfigurationOpen: function() {
		var is_open = false;
		$$(this.elements.configurationWindows).each(function(el) {
			if(el.getStyle('display') == 'block') {
				is_open = true;
				throw $break;
			}
		}.bind(is_open));
		return is_open;
	},

	setSearch: function() {
		var search_pattern = new RegExp(this.search.value)
		$$('div[id=' + this.windowId + this.elements.containerSuffix + '] div.' + this.elements.contentItems).each(function(value){
				
				if(search_pattern.match(value.childNodes[2].textContent) == false) {
					value.setStyle({'display' : 'none'});
				} else {
					value.setStyle({'display' : ''});
				}
			}.bind(search_pattern));
			
			$$('div[id=' + this.windowId + this.elements.containerSuffix + '] tr.' + this.elements.contentItems + ', div[id=' + this.windowId + this.elements.containerSuffix + '] tr.' + this.elements.contentAlternatingItems).each(function(value){
				if(search_pattern.match(value.down('div').innerHTML) == false) {
					value.setStyle({'display' : 'none'});
				} else {
					value.setStyle({'display' : ''});
				}
			}.bind(search_pattern));
	},

	toggleToolbar: function() {
		Effect.toggle($(this.windowId + this.elements.toolbarSuffix), 'slide', { duration: 0.2});
	}
});

document.observe("dom:loaded", function() {
	if(Prototype.Browser.IE  || Prototype.Browser.Gecko) {
		$$('input[type=checkbox], input[type=submit], input[type=radio], a').each(function(el) {
			el.observe('focus', function() {
				el.blur();
			}.bind(el));
		});
	}
});