<div id="tray_bar">
	<img src="<%=$this->getPage()->getTheme()->getBaseUrl()%>/gearwheel-icon.png" alt="<%[ Running jobs: ]%>" /> <%[ Running jobs: ]%> <span class="bold" id="running_jobs"></span>
	<img src="<%=$this->getPage()->getTheme()->getBaseUrl()%>/check-icon.png" alt="<%[ Finished jobs: ]%>" /> <%[ Finished jobs: ]%> <span class="bold" id="finished_jobs"></span>
</div>
<script type="text/javascript">
	var oMonitor;
	var default_refresh_interval = 60000;
	var default_fast_refresh_interval = 10000;
	var timeout_handler;
	document.observe("dom:loaded", function() {
		oMonitor = function() {
			return new Ajax.Request('<%=$this->Service->constructUrl("Monitor")%>', {
				onSuccess: function(response) {
					if (timeout_handler) {
						clearTimeout(timeout_handler);
					}
					var jobs = (response.responseText).evalJSON();
					if (jobs.running_jobs.length > 0) {
						refreshInterval = default_fast_refresh_interval;
					} else {
						refreshInterval = default_refresh_interval;
					}
					job_callback_func();
					$('running_jobs').update(jobs.running_jobs.length);
					$('finished_jobs').update(jobs.terminated_jobs.length);
					timeout_handler = setTimeout("oMonitor()", refreshInterval);
 				}
			});
		};
		oMonitor();
	});
	</script>
