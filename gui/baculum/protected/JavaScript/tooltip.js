Opentip.defaultStyle = 'rounded';
function showTip(el, title, description) {
	el.addTip(description, title, {
			stem: true, 
			fixed: true, 
			tipJoint: [ 'left', 'middle' ], 
			target: true,
			showOn: 'creation'
		});
}