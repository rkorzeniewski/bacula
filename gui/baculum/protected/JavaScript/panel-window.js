var PanelWindowClass = Class.create({

	currentWindow: null,
	windowElements: ['container', 'graphs'],
	onShow: null,

	hideOthers: function() {
		 this.windowElements.each(function(element) {
			 if(element != this.currentWindow) {
				Effect.toggle(element, 'slide', {duration: 0.3, afterFinish : function() {
						$(element).hide();
					}.bind(element)
				});
			}
		 }.bind(this));
	},

	show: function(id) {
		if($(id).visible() === true) {
			return;
		}

		this.currentWindow = id;
		Effect.toggle(id, 'slide', {
			duration: 0.3,
			beforeStart: function() {
				this.hideOthers();
			}.bind(this),
			afterFinish: function() {
				if (this.onShow) {
					this.onShow();
				}
			}.bind(this)
		});
	}
});

var PanelWindow;
document.observe("dom:loaded", function() {
	PanelWindow = new PanelWindowClass();
});
