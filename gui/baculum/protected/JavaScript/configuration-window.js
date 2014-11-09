var ConfigurationWindowClass = new Class.create({
	initialize: function(id) {
		this.window_id = id + 'configuration';
		this.progress_id = 'configuration-progress';
		this.lock = false;
	},

	show: function() {
		this.hideAll();
		$(this.window_id).setStyle({'display' : 'block'});
		$$('div[id=' + this.window_id + '] input[type="submit"]').each(function(el) {
			el.observe('click', function() {
				this.progress(true);
			}.bind(this));
		}.bind(this));
	},

	hide: function() {
		$(this.window_id).setStyle({'display' : 'none'});
	},

	hideAll: function() {
		$$('div.configuration').each(function(el){
			el.setStyle({'display' : 'none'});
		});
	},

	progress: function(show) {
		if(show) {
			$(this.progress_id).setStyle({'display' : 'block'});
		} else {
			$(this.progress_id).setStyle({'display' : 'none'});
		}
	},

	is_progress: function() {
		return $(this.progress_id).getStyle('display') == 'block';
	},
	openConfigurationWindow: function(slideWindowObj) {
		if(this.is_progress() === false) {
			this.progress(true);
			if(slideWindowObj.isFullSize() === true) {
				slideWindowObj.resetSize();
			}
		}
	}
});

